#include "SocketImageOwner.h"
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <pthread.h>
#include <thread>
#include <unistd.h>
using namespace cse::socket_lifetime;

static const HostTable* table = nullptr;
static const API* producer = nullptr;
static unsigned int effects[4]{};
static bool checkBusy = false;
static Status busyResult = Status::Invalid;

extern "C" void __cyg_profile_func_enter(void* function, void*) {
    if (!table) {
        return;
    }
    const void* roles[]{reinterpret_cast<const void*>(table->owns), reinterpret_cast<const void*>(table->createUdp),
                        reinterpret_cast<const void*>(table->closeUdp), reinterpret_cast<const void*>(table->quiescent)};
    for (unsigned int index = 0; index < 4; ++index) {
        if (function == roles[index]) {
            ++effects[index];
        }
    }
    if (checkBusy && function == roles[1]) {
        checkBusy = false;
        busyResult = producer->retire();
    }
}
extern "C" void __cyg_profile_func_exit(void*, void*) {}

static void Require(bool condition, const char* text) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", text);
        std::abort();
    }
}

int main(int argc, char** argv) {
    Require(argc == 2, "provider path");
    table = static_cast<const HostTable*>(dlsym(RTLD_MAIN_ONLY, HostSymbol));
    Require(table != nullptr, "genuine exported main canonical tuple");
    Require(InitializeProcess(), "explicit permanent owner init");
    for (unsigned int cycle = 0; cycle < 3; ++cycle) {
        void* image = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
        Require(image != nullptr, "cold real producer image");
        producer = static_cast<const API*>(dlsym(image, Symbol));
        Dl_info physical{};
        Require(producer && dladdr(producer, &physical), "real typed producer");
        Binding binding{sizeof(Binding), Version, 1, physical.dli_fbase, reinterpret_cast<void*>(1), getpid(), 0,
                        table->owns, table->createUdp, table->closeUdp, table->quiescent};
        for (unsigned int left = 0; left < 4; ++left) {
            for (unsigned int right = left + 1; right < 4; ++right) {
                Binding corrupt = binding;
                constexpr std::size_t PointerBytes = sizeof(void*);
                unsigned char temporary[PointerBytes]{};
                auto* bytes = reinterpret_cast<unsigned char*>(&corrupt) + offsetof(Binding, owns);
                std::memcpy(temporary, bytes + left * PointerBytes, PointerBytes);
                std::memcpy(bytes + left * PointerBytes, bytes + right * PointerBytes, PointerBytes);
                std::memcpy(bytes + right * PointerBytes, temporary, PointerBytes);
                std::memset(effects, 0, sizeof(effects));
                Require(producer->bind(&corrupt) == Status::Invalid, "wrong-role genuine main pointer rejected");
                Require(effects[0] + effects[1] + effects[2] + effects[3] == 0,
                        "all four role matches precede even ownership callback");
            }
        }
        for (unsigned int kind = 0; kind < 5; ++kind) {
            Binding corrupt = binding;
            if (kind == 0) {
                --corrupt.size;
            } else if (kind == 1) {
                ++corrupt.version;
            } else if (kind == 2) {
                corrupt.reserved = 1;
            } else if (kind == 3) {
                corrupt.imageBase = nullptr;
            } else {
                corrupt.epoch = 0;
            }
            std::memset(effects, 0, sizeof(effects));
            Require(producer->bind(&corrupt) == Status::Invalid, "invalid record rejected");
            Require(effects[0] + effects[1] + effects[2] + effects[3] == 0, "record reject has no adapter effects");
        }
        Require(producer->bind(&binding) == Status::MissingOwner, "unknown opaque nonnull context safe rejection");
        binding.originPID = -1;
        Require(producer->bind(&binding) == Status::WrongProcess, "wrong PID before foreign effects");
        binding.originPID = getpid();
        Status threadResult = Status::Ready;
        std::thread worker([&] { threadResult = producer->bind(&binding); });
        worker.join();
        Require(threadResult == Status::WrongThread, "worker cannot bind");
        SocketImageOwner owner;
        Require(owner.Open(image, argv[1]), "actual image anchor and producer bind");
        Require(!table->owns(&owner, physical.dli_fbase, UINT64_MAX), "wrong epoch cannot use genuine owner context");
        Require(!table->owns(&owner, nullptr, cycle + 1), "wrong image cannot use genuine owner context");
        CreateResult created{99, 99};
        Require(table->createUdp(reinterpret_cast<void*>(1), physical.dli_fbase, 1, &created) == Status::MissingOwner &&
                created.descriptor == -1 && created.nativeError == 0, "unknown context initializes create output safely");
        CloseResult closed{CloseDisposition::Consumed, 99};
        Require(table->closeUdp(reinterpret_cast<void*>(1), physical.dli_fbase, 1, 1, &closed) == Status::MissingOwner &&
                closed.disposition == CloseDisposition::NotAttempted && closed.nativeError == 0,
                "unknown context initializes close disposition without syscall");
        Require(table->createUdp(&owner, physical.dli_fbase, cycle + 1, nullptr) == Status::Invalid &&
                table->closeUdp(&owner, physical.dli_fbase, cycle + 1, 1, nullptr) == Status::Invalid,
                "missing output rejected before effects");

        const auto open = reinterpret_cast<int (*)(int)>(dlsym(image, "TestOpen"));
        const auto close = reinterpret_cast<void (*)(int)>(dlsym(image, "TestClose"));
        checkBusy = true;
        const int descriptor = open(-1);
        Require(descriptor > 0 && busyResult == Status::Busy, "complete create callback blocks retirement");
        Require(producer->retire() == Status::LiveSockets && !owner.Retire() && owner.Active(),
                "live owner cannot retire and remains callable for cleanup");
        close(descriptor);
        Require(owner.Retire() && !owner.Active(), "empty terminal retry releases real anchor");
        std::memset(effects, 0, sizeof(effects));
        Require(open(-1) == 0, "terminal create rejected");
        close(descriptor);
        Require(effects[0] + effects[1] + effects[2] + effects[3] == 0, "terminal calls have no host effects");
        Require(producer->retire() == Status::Retired, "terminal retirement idempotent");
        producer = nullptr;
        Require(dlclose(image) == 0, "physical provider release");
        void* mapped = dlopen(argv[1], RTLD_NOW | RTLD_NOLOAD);
        Require(!mapped, "producer actually unmapped before next cycle");
    }
    std::puts("canonical_roles=4 swapped_pairs=6 cycles=3 physical_unmaps=3 busy_live_retry_terminal=passed");
}
