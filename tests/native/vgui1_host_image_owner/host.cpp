#include "VGui1HostImageOwner.h"
#include "native_module.h"
#include <dlfcn.h>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

void RejectComponentNew(bool);
#if defined(CSE_RELEASE_INJECTION)
void FailNextRelease();
void ProbeDuringRelease(void (*)());
#endif

namespace {
namespace host = cse_vgui1_host;
#if defined(CSE_TSAN_SHORT_BURST)
constexpr int TransferCount = 128; // Separate race gate, not the ten-thousand-transfer stress proof.
#else
constexpr int TransferCount = 10000;
#endif
constexpr int ExpectedRootValue = 73;
std::uint64_t lastEpoch = 0;

void Check(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "FAIL host image owner: %s\n", message);
        std::abort();
    }
}

std::uint64_t NextEpoch() {
    Check(lastEpoch != UINT64_MAX, "bounded fixture epoch exhaustion");
    return ++lastEpoch;
}

void* Load(const char* path) {
    void* reference = cse_platform::LoadModule(path, cse_platform::ModuleFallback::None);
    Check(reference != nullptr, "real NativeModule owning load");
    return reference;
}

void Release(void* reference) {
    Check(reference && dlclose(reference) == 0, "checked real fixture release");
}

bool Mapped(const char* path) {
    return cse_platform::FindLoadedModule(path) != nullptr;
}

template<class Function> Function Symbol(void* reference, const char* name) {
    void* symbol = cse_platform::GetSymbol(reference, name);
    Check(symbol != nullptr, "actual owned fixture export");
    return reinterpret_cast<Function>(symbol);
}

host::AdmittedImageV1 Image(void* reference, const char* path, std::uint64_t epoch) {
    return {sizeof(host::AdmittedImageV1), host::AdmittedImageVersion, epoch, reference, path};
}

host::Statistics Stats(host::ImageOwner& owner) {
    host::Statistics value{};
    Check(owner.Inspect(value) == host::Result::Ready, "inspect on owner thread");
    return value;
}

class Entry {
public:
    explicit Entry(host::ImageOwner& owner, void* retained = nullptr) : owner_(owner) {
        Check(owner_.EnterRoot(retained) == host::Result::Ready, "genuine root-entry guard");
    }
    ~Entry() {
        Check(owner_.LeaveRoot() == host::Result::Ready, "leave after root function returned");
    }
private:
    host::ImageOwner& owner_;
};

int RefuseClose(void* context) {
    auto& owner = *static_cast<host::ImageOwner*>(context);
    Check(owner.Close() == host::Result::Busy, "cannot release anchor on root callback stack");
    Check(Stats(owner).rootDepth > 0, "actual guarded root stack remains active");
    return ExpectedRootValue;
}

void RejectDependencyProxy(const char* rootPath, const char* proxyPath, std::uint64_t epoch) {
    void* root = Load(rootPath);
    void* proxy = Load(proxyPath);
    Check(cse_platform::GetSymbol(proxy, "CreateInterface") == cse_platform::GetSymbol(root, "CreateInterface"),
        "actual dlsym foreign proxy resolves the root dependency's factory");
    host::ImageOwner invalidOwner;
    Check(invalidOwner.Open(Image(proxy, rootPath, epoch), host::AnchorMode::FirstTransferredReference, nullptr)
        == host::Result::InvalidImage, "foreign proxy cannot masquerade as the admitted root handle");
    host::ImageOwner owner;
    Check(owner.Open(Image(root, rootPath, epoch), host::AnchorMode::AdoptInitialReference, root)
        == host::Result::Ready, "real root anchor for dependency-transfer rejection");
    {
        Entry entry(owner);
        const auto contract = owner.Contract();
        Check(!contract.acceptRoot(contract.context, proxy), "foreign dependency-proxy transfer rejected");
    }
    Check(Mapped(proxyPath), "proxy ownership not consumed on refusal");
    Release(proxy);
    Check(owner.Close() == host::Result::Closed && !Mapped(rootPath), "proxy case releases both genuine owners");
    std::puts("PASS direct-handle identity rejects a real dependent dylib resolving the same factory");
}

void NativeInitial(const char* rootPath, const char* foreignPath, const char* misplacedPath, std::uint64_t epoch) {
    void* root = Load(rootPath);
    host::ImageOwner owner;
    const auto admitted = Image(root, rootPath, epoch);
    auto invalid = admitted;
    invalid.epoch = 0;
    Check(owner.Open(invalid, host::AnchorMode::AdoptInitialReference, root) == host::Result::InvalidArgument,
        "zero epoch refuses without taking the genuine initial handle");
    invalid = admitted;
    invalid.absolutePath = foreignPath;
    Check(owner.Open(invalid, host::AnchorMode::AdoptInitialReference, root) == host::Result::InvalidImage,
        "path-to-actual-image mismatch refuses before adoption");
    void* misplaced = Load(misplacedPath);
    const auto misplacedProbe = Symbol<int (*)(const host::AdmittedImageV1*)>(misplaced, "MisplacedOwnerProbe");
    Check(misplacedProbe(&admitted) == static_cast<int>(host::Result::InvalidCallbackImage),
        "actual helper linked inside a dylib cannot pretend its callback belongs to main");
    Release(misplaced);
    const auto burst = Symbol<int (*)(const host::HostReleasesV2*, const char*, int)>(root, "RootBurst");
    const auto ping = Symbol<int (*)()>(root, "RootPing");
    const auto probe = Symbol<int (*)(int (*)(void*), void*)>(root, "RootProbe");
    RejectComponentNew(true);
    Check(owner.Open(admitted, host::AnchorMode::AdoptInitialReference, root) == host::Result::Ready,
        "success adopts actual launcher-like initial owning load");
    root = nullptr; // The caller no longer owns that reference.
    auto contract = owner.Contract();
    Check(contract.structBytes == sizeof(contract) && contract.version == 2, "distinct semantic V2 POD");
    {
        Entry entry(owner);
        {
            Entry nested(owner);
            Check(Stats(owner).rootDepth == 2, "nested entry depth");
            Check(probe(RefuseClose, &owner) == ExpectedRootValue, "actual root calls main depth control");
        }
        Check(!contract.acceptRoot(contract.context, nullptr), "null transfer refuses ownership");
        void* foreign = Load(foreignPath);
        Check(!contract.acceptRoot(contract.context, foreign), "real foreign-image transfer rejected");
        Check(Mapped(foreignPath), "false accepted nothing: caller still owns foreign image");
        Release(foreign);
        auto oldContract = contract;
        oldContract.version = 1;
        Check(burst(&oldContract, rootPath, 1) == -1, "root explicitly rejects former V1 semantics");
        Check(burst(&contract, rootPath, TransferCount) == TransferCount, "real repeated transfer/balance");
        const auto measured = Stats(owner);
        Check(measured.retainedReferences == 1 && measured.acceptedTransfers == TransferCount
            && measured.balancedTransfers == TransferCount && measured.maximumRetainedReferences == 2,
            "ten thousand switches retain one genuine anchor, never a Run-length queue");
        Check(ping() == ExpectedRootValue && Mapped(rootPath), "root still live after real callback balances");
    }
    RejectComponentNew(false);
    bool workerPass = false;
    std::thread worker([&] {
        host::Statistics untouched{};
        untouched.epoch = UINT64_MAX;
        workerPass = !contract.acceptRoot(contract.context, nullptr)
            && owner.EnterRoot() == host::Result::WrongThread && owner.LeaveRoot() == host::Result::WrongThread
            && owner.RetryBalance() == host::Result::WrongThread && owner.Close() == host::Result::WrongThread
            && owner.Inspect(untouched) == host::Result::WrongThread && untouched.epoch == UINT64_MAX;
    });
    worker.join();
    Check(workerPass, "wrong-thread operations reject before state access and preserve output");
    void* workerReference = Load(rootPath);
    std::atomic<bool> started{false};
    std::atomic<bool> stop{false};
    bool allRejected = true;
    std::thread concurrent([&] {
        started.store(true);
        while (!stop.load()) {
            host::Statistics untouched{};
            untouched.epoch = UINT64_MAX;
            if (contract.acceptRoot(contract.context, workerReference)
                || owner.Inspect(untouched) != host::Result::WrongThread || untouched.epoch != UINT64_MAX) {
                allRejected = false;
                break;
            }
            std::this_thread::yield();
        }
    });
    while (!started.load()) {
        std::this_thread::yield();
    }
    {
        Entry entry(owner);
        Check(burst(&contract, rootPath, TransferCount) == TransferCount,
            "actual owner mutation while worker makes invalid same-image callbacks");
    }
    stop.store(true);
    concurrent.join();
    Check(allRejected, "concurrent invalid callbacks accepted no ownership or diagnostics writes");
    Release(workerReference);
    RejectComponentNew(true);
    Check(owner.Close() == host::Result::Closed, "depth-zero checked release of actual initial anchor");
    Check(!Mapped(rootPath), "all ten thousand refs balanced: actual root image is absent");
    Check(owner.Close() == host::Result::Closed, "idempotent close");
    RejectComponentNew(false);
    void* next = Load(rootPath);
    const auto repeatedEpoch = Image(next, rootPath, epoch);
    Check(owner.Open(repeatedEpoch, host::AnchorMode::AdoptInitialReference, next) == host::Result::InvalidEpoch,
        "same owner refuses reused epoch without consuming caller handle");
    Release(next);
    std::printf("PASS initial-anchor transfers=%d retained=1 final-unmapped epoch=%llu\n", TransferCount * 2,
        static_cast<unsigned long long>(epoch));
}

void FirstTransferred(const char* rootPath, std::uint64_t epoch) {
    void* outer = Load(rootPath);
    const auto hold = Symbol<int (*)(const char*)>(outer, "RootHold");
    const auto retained = Symbol<void* (*)()>(outer, "RootRetained");
    const auto transfer = Symbol<int (*)(const host::HostReleasesV2*)>(outer, "RootTransferHeld");
    const auto ping = Symbol<int (*)()>(outer, "RootPing");
    const auto burst = Symbol<int (*)(const host::HostReleasesV2*, const char*, int)>(outer, "RootBurst");
    Check(hold(rootPath) == 1, "actual independent root-owned baseline load");
    void* proof = retained();
    host::ImageOwner owner;
    Check(owner.Open(Image(outer, rootPath, epoch), host::AnchorMode::FirstTransferredReference, nullptr)
        == host::Result::Ready, "explicit first-transfer mode");
    const auto contract = owner.Contract();
    void* extra = Load(rootPath);
    Check(!contract.acceptRoot(contract.context, extra), "outside root entry accepts nothing");
    Release(extra);
    Check(owner.EnterRoot() == host::Result::MissingAnchor, "no genuine anchor or entry proof refuses");
    {
        Entry entry(owner, proof);
        Release(outer);
        outer = nullptr;
        Check(Mapped(rootPath), "only actual root-owned baseline retains image before callback");
        RejectComponentNew(true);
        Check(transfer(&contract) == 1, "first accepted root-owned ref becomes main-image anchor");
        Check(retained() == nullptr && Stats(owner).retainedReferences == 1, "ownership really transferred");
        Check(burst(&contract, rootPath, TransferCount) == TransferCount, "coalesce after original host ref dropped");
        Check(ping() == ExpectedRootValue, "last original reference transfer returned through still-mapped code");
        RejectComponentNew(false);
    }
    Check(Mapped(rootPath), "anchor stays mapped through outermost return");
    Check(owner.Close() == host::Result::Closed && !Mapped(rootPath), "main depth zero finally unmaps root");
    std::printf("PASS first-transferred-anchor transfers=%d original-host-dropped final-unmapped\n", TransferCount);
}

#if defined(CSE_RELEASE_INJECTION)
host::ImageOwner* probeOwner = nullptr;
void* reentrantReference = nullptr;
bool reentrantRejected = false;
void DuringRelease() {
    const auto contract = probeOwner->Contract();
    reentrantRejected = !contract.acceptRoot(contract.context, reentrantReference)
        && probeOwner->Close() == host::Result::Busy && probeOwner->RetryBalance() == host::Result::Busy;
}

void Failures(const char* rootPath) {
    {
        void* external = Load(rootPath);
        host::ImageOwner validationOwner;
        FailNextRelease();
        Check(validationOwner.Open(Image(external, rootPath, NextEpoch()), host::AnchorMode::AdoptInitialReference,
            external) == host::Result::ReleaseFailed, "failed validation-query release is retained explicitly");
        Check(Stats(validationOwner).retainedReferences == 1 && Stats(validationOwner).acceptedTransfers == 0,
            "failed Open adopted no caller handle and retains only its own validation load");
        Release(external); // Still genuinely caller-owned after Open failure.
        Check(Mapped(rootPath), "only the failed query reference now retains the actual image");
        Check(validationOwner.Close() == host::Result::Closed && !Mapped(rootPath),
            "explicit partial-Open cleanup releases its own query without a lost reference");
    }
    void* initial = Load(rootPath);
    host::ImageOwner owner;
    Check(owner.Open(Image(initial, rootPath, NextEpoch()), host::AnchorMode::AdoptInitialReference, initial)
        == host::Result::Ready, "failure fixture owns actual anchor");
    const auto contract = owner.Contract();
    {
        Entry entry(owner);
        void* transferred = Load(rootPath);
        FailNextRelease();
        Check(contract.acceptRoot(contract.context, transferred), "post-accept failure still accepts entire reference");
        const auto failed = Stats(owner);
        Check(failed.retainedReferences == 2 && failed.releasePending && failed.acceptedTransfers == 1
            && failed.balancedTransfers == 0 && failed.nativeReleaseCode == -1 && failed.nativeErrno == EAGAIN
            && failed.nativeError[0] != '\0',
            "failed real boundary did not run dlclose: anchor plus pending ref retained");
        void* rejected = Load(rootPath);
        Check(!contract.acceptRoot(contract.context, rejected), "full failed-balance slot rejects new ownership");
        Release(rejected);
        FailNextRelease();
        Check(owner.RetryBalance() == host::Result::ReleaseFailed && Stats(owner).retainedReferences == 2,
            "repeated failure retains the same bounded pair");
        Check(owner.RetryBalance() == host::Result::Ready && Stats(owner).retainedReferences == 1,
            "explicit retry really balances the retained extra ref");
        probeOwner = &owner;
        reentrantReference = Load(rootPath);
        ProbeDuringRelease(DuringRelease);
        void* extra = Load(rootPath);
        Check(contract.acceptRoot(contract.context, extra), "normal balance with controlled reentry");
        ProbeDuringRelease(nullptr);
        Check(reentrantRejected, "reentry rejects without balancing borrowed/caller-owned reference");
        Release(reentrantReference);
        reentrantReference = nullptr;
    }
    FailNextRelease();
    Check(owner.Close() == host::Result::ReleaseFailed && Mapped(rootPath), "failed anchor close retains mapping");
    Check(Stats(owner).retainedReferences == 1 && owner.EnterRoot() == host::Result::ReleaseFailed,
        "closing failure refuses new entries");
    Check(owner.Close() == host::Result::Closed && !Mapped(rootPath), "explicit final close retry unmaps");
    std::puts("PASS injected fail-before-close pending-slot, refusal, reentry and actual retry releases");
}
#endif
}

int main(int argc, char** argv) {
    Check(argc == 6, "four exact image paths plus bounded cycles");
    const int cycles = std::atoi(argv[5]);
    Check(cycles > 0 && cycles <= 4, "bounded cycle count");
    Check(!Mapped(argv[1]) && !Mapped(argv[2]) && !Mapped(argv[3]), "initial images absent");
    for (int index = 0; index < cycles; ++index) {
        RejectDependencyProxy(argv[1], argv[4], NextEpoch());
        NativeInitial(argv[1], argv[2], argv[3], NextEpoch());
        FirstTransferred(argv[1], NextEpoch());
#if defined(CSE_RELEASE_INJECTION)
        Failures(argv[1]);
#endif
    }
    return 0;
}
