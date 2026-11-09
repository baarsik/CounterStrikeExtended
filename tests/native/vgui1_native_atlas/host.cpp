#include "VGui1NativeRuntime.h"
#include "native_module.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <exception>
#include <cstring>

namespace {
void Check(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "FAILED owned loader: %s\n", message);
        std::abort();
    }
}
struct HostQueue {
    std::vector<void*> references;
    int rootDepth = 0;
    bool reject = false;
    bool TryDrain() {
        if (rootDepth != 0) {
            return false;
        }
        for (auto* reference : references) {
            cse_platform::UnloadModule(reference);
        }
        references.clear();
        return true;
    }
    void Drain() {
        Check(TryDrain(), "never drain while returning through root code");
    }
};
bool Enqueue(void* value, void* ownedRootReference) noexcept {
    auto& queue = *static_cast<HostQueue*>(value);
    Check(queue.rootDepth > 0 && ownedRootReference, "main-image callback receives genuine owned root reference");
    if (queue.reject) {
        return false;
    }
    queue.references.push_back(ownedRootReference);
    Check(!queue.TryDrain(), "defined negative: host refuses drain while the real root callback is on stack");
    return true;
}
class RootCall {
public:
    explicit RootCall(HostQueue& queue) : queue_(queue) {
        ++queue_.rootDepth;
    }
    ~RootCall() {
        --queue_.rootDepth;
    }
private:
    HostQueue& queue_;
};
template<class Function> Function Symbol(void* image, const char* name) {
    auto* value = cse_platform::GetSymbol(image, name);
    Check(value != nullptr, "exact owned export");
    return reinterpret_cast<Function>(value);
}
}
int main(int argc, char** argv) {
    Check(argc == 4 || argc == 5, "root provider repeats and optional fault mode");
    const int repeats = std::atoi(argv[3]);
    Check(repeats > 0 && repeats <= 16, "bounded repeats");
    using Open = int (*)(std::uint64_t, const char*, const char*, const cse_vgui1_native::HostReleasesV1*);
    using Call = int (*)();
    for (int index = 0; index < repeats; ++index) {
        HostQueue queue;
        const cse_vgui1_native::HostReleasesV1 host{sizeof(host), cse_vgui1_native::HostReleasesVersion,
            &queue, Enqueue};
        void* image = cse_platform::LoadModule(argv[1], cse_platform::ModuleFallback::None);
        Check(image != nullptr, "load actual owned root dylib");
        if (argc == 5) {
            std::set_terminate([] {
                std::fputs("EXPECTED allocation termination before EH host catch\n", stderr);
                std::_Exit(86);
            });
            try {
                RootCall call(queue);
                if (std::strcmp(argv[4], "--fault-runtime") == 0) {
                    using Fault = void (*)(const char*, const char*, const cse_vgui1_native::HostReleasesV1*);
                    Symbol<Fault>(image, "FixtureFaultRuntime")(argv[1], argv[2], &host);
                } else {
                    Check(Symbol<Open>(image, "FixtureOpen")(1, argv[1], argv[2], &host) == 0, "fault setup");
                    using Fault = void (*)();
                    Symbol<Fault>(image, std::strcmp(argv[4], "--fault-pin") == 0
                        ? "FixtureFaultPin" : "FixtureFaultAtlas")();
                }
            } catch (...) {
                std::fputs("RED allocation exception escaped into the EH host catch\n", stderr);
                std::_Exit(90);
            }
            return 91;
        }
        {
            RootCall call(queue);
            Check(Symbol<Open>(image, "FixtureOpen")(static_cast<std::uint64_t>(index) + 1,
                argv[1], argv[2], &host) == 0, "nonreused epoch and real main-image ownership route");
            Check(Symbol<Call>(image, "FixtureSuite")() == 0, "actual root/API suite");
            Check(Symbol<Call>(image, "FixtureConcurrentGuards")() == 0, "concurrent callback control");
            Check(Symbol<Call>(image, "FixtureHoldSelection")() == 0, "owned unpin setup");
            queue.reject = true;
            Check(Symbol<Call>(image, "FixtureRejectUnpin")() == 0, "defined failed-host acceptance control");
            queue.reject = false;
            Check(Symbol<Call>(image, "FixtureRetryUnpin")() == 0, "explicit unpin retry");
        }
        queue.Drain(); // Host code, strictly after all root returns.
        const auto close = Symbol<Call>(image, "FixtureClose");
        const auto rejectClose = Symbol<Call>(image, "FixtureRejectClose");
        cse_platform::UnloadModule(image);
        Check(cse_platform::FindLoadedModule(argv[1]) != nullptr
            && cse_platform::FindLoadedModule(argv[2]) != nullptr, "only genuine Runtime refs retain images");
        queue.reject = true;
        {
            RootCall call(queue);
            Check(rejectClose() == 0, "queue failure retains last self reference");
        }
        Check(queue.references.empty() && cse_platform::FindLoadedModule(argv[1]) != nullptr,
            "failed close accepted no ownership and unmapped no root code");
        queue.reject = false;
        {
            RootCall call(queue);
            Check(close() == 0, "last-self Close returns before queued owned ref is drained");
        }
        Check(queue.references.size() == 1 && cse_platform::FindLoadedModule(argv[1]) != nullptr,
            "actual mapping remains through return solely via host-owned deferred root reference");
        queue.Drain();
        Check(cse_platform::FindLoadedModule(argv[1]) == nullptr
            && cse_platform::FindLoadedModule(argv[2]) == nullptr, "host drain finally unmaps both actual images");
        std::printf("PASS actual deferred-last-self dylib cycle epoch=%d\n", index + 1);
    }
    return 0;
}
