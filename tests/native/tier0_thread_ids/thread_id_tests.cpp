#include "thread_api.h"
#include <array>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mach/mach.h>
#include <mutex>
#include <pthread.h>
#include <set>
#include <thread>
#include <type_traits>
#include <vector>

namespace {
constexpr std::size_t kConcurrentWorkers = 16;
constexpr int kLifetimeCycles = 32;
constexpr int kRegistrationQueries = 1000;

unsigned int NativeIdentity() {
    static_assert(std::is_same<mach_port_t, unsigned int>::value, "Native port identity already has public width");
    const mach_port_t identity = pthread_mach_thread_np(pthread_self());
    Require(MACH_PORT_VALID(identity), "valid actual native thread port");
    return identity;
}

void Initial() {
    static_assert(sizeof(Plat_PrimaryThreadID) == sizeof(unsigned long), "Keep the primary variable storage ABI");
    static_assert(std::numeric_limits<unsigned int>::digits == 32, "Public logical IDs remain 32-bit");
    Require(sizeof(Plat_PrimaryThreadID) == 8, "Mac LP64 primary storage remains eight bytes");
    Require(Plat_GetCurrentThreadID() == 0 && Plat_PrimaryThreadID == 0, "unregistered state is zero");
    std::printf("primary_storage_bytes=%zu logical_id_bytes=%zu pthread_bits=0x%llx\n",
        sizeof(Plat_PrimaryThreadID), sizeof(unsigned int),
        static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(pthread_self())));
}

void Registration() {
    Initial();
    const auto native = NativeIdentity();
    Require(Plat_RegisterThread("owned fixture") == native, "register actual native 32-bit identity");
    Require(Plat_GetCurrentThreadID() == native, "registration updates caller TLS");
    Require(Plat_RegisterThread(nullptr) == native, "repeat registration is stable");
    Require(Plat_PrimaryThreadID == 0, "regular registration does not set primary");
}

void Concurrent() {
    Initial();
    std::mutex mutex;
    std::condition_variable condition;
    std::size_t ready = 0;
    bool release = false;
    std::array<unsigned int, kConcurrentWorkers> ids{};
    std::vector<std::thread> workers;
    for (std::size_t index = 0; index < ids.size(); ++index) {
        workers.emplace_back([&, index] {
            Require(Plat_GetCurrentThreadID() == 0, "each new worker starts unregistered");
            const auto native = NativeIdentity();
            ids[index] = Plat_RegisterThread(nullptr);
            Require(ids[index] == native && Plat_GetCurrentThreadID() == native, "worker has its own native ID");
            std::unique_lock<std::mutex> lock(mutex);
            ++ready;
            condition.notify_all();
            condition.wait(lock, [&] { return release; });
            lock.unlock();
            Require(Plat_RegisterThread("repeat") == native && Plat_GetCurrentThreadID() == native,
                "peer registrations do not replace caller TLS");
        });
    }
    {
        std::unique_lock<std::mutex> lock(mutex);
        condition.wait(lock, [&] { return ready == ids.size(); });
        const std::set<unsigned int> unique(ids.begin(), ids.end());
        Require(unique.size() == ids.size() && unique.count(0) == 0, "all simultaneously live IDs are distinct");
        Require(Plat_GetCurrentThreadID() == 0 && Plat_PrimaryThreadID == 0, "workers leave caller registration alone");
        release = true;
        condition.notify_all();
    }
    for (auto& worker : workers) {
        worker.join();
    }
}

void Primary() {
    Initial();
    const auto mainId = Plat_RegisterPrimaryThread();
    Require(mainId == NativeIdentity() && Plat_PrimaryThreadID == mainId, "primary uses caller native identity");
    Require(Plat_GetCurrentThreadID() == mainId, "primary registration updates caller TLS");
    std::mutex mutex;
    std::condition_variable condition;
    unsigned int workerId = 0;
    bool ready = false;
    bool restored = false;
    std::thread worker([&] {
        Require(Plat_GetCurrentThreadID() == 0, "new primary candidate starts unregistered");
        Require(Plat_RegisterThread(nullptr) == NativeIdentity(), "ordinary worker registration");
        Require(Plat_PrimaryThreadID == mainId, "ordinary registration preserves primary");
        std::unique_lock<std::mutex> lock(mutex);
        workerId = Plat_RegisterPrimaryThread();
        Require(workerId == NativeIdentity(), "primary candidate keeps native logical identity");
        ready = true;
        condition.notify_all();
        condition.wait(lock, [&] { return restored; });
        Require(Plat_GetCurrentThreadID() == workerId, "primary restoration does not rewrite worker TLS");
    });
    {
        std::unique_lock<std::mutex> lock(mutex);
        condition.wait(lock, [&] { return ready; });
        Require(workerId != mainId && Plat_PrimaryThreadID == workerId, "controlled primary replacement");
        Require(Plat_GetCurrentThreadID() == mainId, "replacement leaves original caller TLS intact");
        Require(Plat_RegisterPrimaryThread() == mainId && Plat_PrimaryThreadID == mainId, "primary restored");
        restored = true;
        condition.notify_all();
    }
    worker.join();
}

void Lifecycle() {
    Initial();
    for (int index = 0; index < kLifetimeCycles; ++index) {
        std::thread worker([] {
            Require(Plat_GetCurrentThreadID() == 0, "new thread lifetime gets fresh unregistered TLS");
            const auto identity = Plat_RegisterThread(nullptr);
            Require(identity != 0 && identity == NativeIdentity(), "thread lifetime registers a native ID");
            Require(Plat_GetCurrentThreadID() == identity, "current registration remains local");
        });
        worker.join();
    }
    Require(Plat_GetCurrentThreadID() == 0 && Plat_PrimaryThreadID == 0, "lifetime cycles leave caller untouched");
}

void Rights() {
    Initial();
    const auto identity = NativeIdentity();
    mach_port_urefs_t before = 0;
    mach_port_urefs_t after = 0;
    Require(mach_port_get_refs(mach_task_self(), identity, MACH_PORT_RIGHT_SEND, &before) == KERN_SUCCESS,
        "read existing borrowed thread-port rights");
    for (int index = 0; index < kRegistrationQueries; ++index) {
        Require(Plat_RegisterThread(nullptr) == identity, "repeated native registration");
    }
    Require(mach_port_get_refs(mach_task_self(), identity, MACH_PORT_RIGHT_SEND, &after) == KERN_SUCCESS,
        "read rights after repeated registration");
    Require(before == after, "registration does not acquire Mach port rights");
    std::printf("native_id=%u send_refs_before=%u send_refs_after=%u registrations=%d\n",
        identity, before, after, kRegistrationQueries);
}
}

int main(int argc, const char** argv) {
    Require(argc == 2, "one scenario required");
    if (std::strcmp(argv[1], "initial") == 0) {
        Initial();
    } else if (std::strcmp(argv[1], "register") == 0) {
        Registration();
    } else if (std::strcmp(argv[1], "concurrent") == 0) {
        Concurrent();
    } else if (std::strcmp(argv[1], "primary") == 0) {
        Primary();
    } else if (std::strcmp(argv[1], "lifecycle") == 0) {
        Lifecycle();
    } else if (std::strcmp(argv[1], "rights") == 0) {
        Rights();
    } else {
        Require(false, "unknown scenario");
    }
    std::puts("actual_thread_id_scenario_passed=1");
    return 0;
}
