#include "fixture.h"

#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

constexpr std::uint32_t BeforeGuard = 0x89abcdef;
constexpr std::uint32_t AfterGuard = 0x76543210;
constexpr int DepthSentinel = 0x13579;

struct alignas(8) GuardedMutex {
    std::uint32_t before = BeforeGuard;
    CThreadFastMutex mutex;
    std::uint32_t after = AfterGuard;
};

static_assert(std::is_standard_layout<CThreadFastMutex>::value, "Fast mutex layout must remain standard");
static_assert(sizeof(CThreadFastMutex) == 8, "Fast mutex storage must remain eight bytes");
static_assert(alignof(CThreadFastMutex) == 4, "Fast mutex must require only four-byte alignment");
static_assert(offsetof(CThreadFastMutex, m_ownerID) == 0, "Fast mutex owner offset changed");
static_assert(offsetof(CThreadFastMutex, m_depth) == 4, "Fast mutex depth offset changed");
static_assert(offsetof(GuardedMutex, mutex) == 4, "Fixture must exercise a four-modulo-eight owner address");
#if defined(_WIN32)
static_assert(sizeof(unsigned long) == 4, "Windows thread IDs must retain DWORD storage");
static_assert(std::is_same<decltype(CThreadFastMutex::m_ownerID), volatile unsigned long>::value,
    "Production owner field type changed");
#endif

static void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

static void RequireGuards(const GuardedMutex& guarded) {
    Require(guarded.before == BeforeGuard && guarded.after == AfterGuard, "Mutex changed an adjacent guard");
}

struct ForcedThreadId {
    explicit ForcedThreadId(std::uint32_t value) {
        SetFixtureThreadId(value);
    }

    ~ForcedThreadId() {
        SetFixtureThreadId(0);
    }
};

static void TestLayout() {
    CThreadFastMutex mutex;
    Require(mutex.m_ownerID == 0 && mutex.m_depth == 0, "Constructor changed initial owner/depth");
    std::printf("mutex_bytes=%zu alignment=%zu owner_bytes=%zu depth_offset=%zu pointer_bytes=%zu\n",
        sizeof(mutex), alignof(CThreadFastMutex), sizeof(mutex.m_ownerID), offsetof(CThreadFastMutex, m_depth),
        sizeof(void*));
}

static void TestRecursion() {
    GuardedMutex guarded;
    const volatile CThreadFastMutex& constant = guarded.mutex;
    const unsigned long owner = ThreadGetCurrentId();
    Require(guarded.mutex.TryLock(), "Initial TryLock failed");
    Require(guarded.mutex.m_ownerID == owner && guarded.mutex.m_depth == 1, "Initial owner/depth incorrect");
    Require(constant.TryLock(), "Const volatile recursive TryLock failed");
    constant.Lock(0);
    Require(guarded.mutex.m_ownerID == owner && guarded.mutex.m_depth == 3, "Recursion changed owner or depth");
    constant.Unlock();
    Require(guarded.mutex.m_ownerID == owner && guarded.mutex.m_depth == 2, "Recursive unlock released owner");
    guarded.mutex.Unlock();
    guarded.mutex.Unlock();
    Require(guarded.mutex.m_ownerID == 0 && guarded.mutex.m_depth == 0, "Final unlock did not release owner");
    RequireGuards(guarded);
}

static void TestOperandIsolation() {
    GuardedMutex guarded;
    // Deliberately non-normal depth proves that compare-exchange observes only
    // the 32-bit owner. This is an operand-width probe, not a valid unlocked state.
    guarded.mutex.m_depth = DepthSentinel;
    Require(guarded.mutex.TryLock(), "CAS inspected the neighboring depth instead of only the 32-bit owner");
    Require(guarded.mutex.m_ownerID == ThreadGetCurrentId(), "CAS did not preserve the current owner ID");
    Require(guarded.mutex.m_depth == DepthSentinel + 1, "CAS overwrote neighboring depth bytes");
    guarded.mutex.Unlock();
    Require(guarded.mutex.m_depth == DepthSentinel, "Depth changed by more than one lock/unlock step");
    RequireGuards(guarded);
    guarded.mutex.m_ownerID = 0;
    guarded.mutex.m_depth = 0;
}

static void TestHighOwnerId() {
    constexpr std::uint32_t HighIds[] = {0x80000000u, 0xffffffffu, 0xfedcba98u};
    for (std::uint32_t id : HighIds) {
        ForcedThreadId forced(id);
        GuardedMutex guarded;
        Require(guarded.mutex.TryLock(), "High-bit thread ID could not acquire the mutex");
        Require(guarded.mutex.m_ownerID == id && guarded.mutex.m_depth == 1, "High-bit owner ID was truncated");
        Require(guarded.mutex.TryLock(), "High-bit owner could not recursively acquire");
        Require(guarded.mutex.m_depth == 2, "High-bit owner recursion depth incorrect");
        SetFixtureThreadId(0x1234);
        Require(!guarded.mutex.TryLock(), "Another ID acquired a mutex owned by a high-bit thread ID");
        Require(guarded.mutex.m_ownerID == id && guarded.mutex.m_depth == 2, "Failed CAS modified owner or depth");
        SetFixtureThreadId(id);
        guarded.mutex.Unlock();
        guarded.mutex.Unlock();
        Require(guarded.mutex.m_ownerID == 0 && guarded.mutex.m_depth == 0, "High-bit owner did not release");
        RequireGuards(guarded);
    }
}

static void TestContendedTry() {
    GuardedMutex guarded;
    guarded.mutex.Lock(0);
    const unsigned long owner = ThreadGetCurrentId();
    bool acquired = true;
    std::thread contender([&] {
        acquired = guarded.mutex.TryLock();
        if (acquired) {
            guarded.mutex.Unlock();
        }
    });
    contender.join();
    Require(!acquired, "Contended TryLock acquired another thread's mutex");
    Require(guarded.mutex.m_ownerID == owner && guarded.mutex.m_depth == 1,
        "Contended TryLock modified the current owner's storage");
    guarded.mutex.Unlock();
    RequireGuards(guarded);
}

static void TestUnlock() {
    GuardedMutex guarded;
    guarded.mutex.Lock(0);
    guarded.mutex.Lock(0);
    guarded.mutex.Unlock();
    Require(guarded.mutex.m_ownerID == ThreadGetCurrentId() && guarded.mutex.m_depth == 1,
        "Partial recursive unlock released the owner");
    guarded.mutex.Unlock();
    bool acquired = false;
    bool ownerMatched = false;
    std::thread successor([&] {
        acquired = guarded.mutex.TryLock();
        if (acquired) {
            ownerMatched = guarded.mutex.m_ownerID == ThreadGetCurrentId() && guarded.mutex.m_depth == 1;
            guarded.mutex.Unlock();
        }
    });
    successor.join();
    Require(acquired && ownerMatched, "Successor did not acquire released owner storage");
    Require(guarded.mutex.m_ownerID == 0 && guarded.mutex.m_depth == 0, "Successor did not fully release");
    RequireGuards(guarded);
}

static void TestAlignment() {
    GuardedMutex guarded[3];
    for (GuardedMutex& entry : guarded) {
        const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(&entry.mutex.m_ownerID);
        constexpr std::uintptr_t OwnerAlignment = 4;
        constexpr std::uintptr_t PointerAlignment64 = 8;
        Require(address % OwnerAlignment == 0 && address % PointerAlignment64 == OwnerAlignment,
            "Fixture did not exercise a legal owner address with only four-byte alignment");
        Require(entry.mutex.TryLock(), "Four-byte-aligned owner could not acquire");
        entry.mutex.Unlock();
        Require(entry.mutex.m_ownerID == 0 && entry.mutex.m_depth == 0, "Aligned lock/unlock changed owner/depth");
        RequireGuards(entry);
    }
}

#if defined(_WIN32)
static void TestContention() {
    GuardedMutex guarded;
    constexpr int Workers = 4;
    constexpr int Iterations = 4000;
    std::atomic<bool> start{false};
    std::atomic<int> active{0};
    std::atomic<int> failures{0};
    std::uint64_t counter = 0;
    std::vector<std::thread> workers;
    for (int worker = 0; worker < Workers; ++worker) {
        workers.emplace_back([&] {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (int iteration = 0; iteration < Iterations; ++iteration) {
                guarded.mutex.Lock(0);
                if (active.fetch_add(1) != 0 || guarded.mutex.m_ownerID != ThreadGetCurrentId()
                    || guarded.mutex.m_depth != 1) {
                    ++failures;
                }
                ++counter;
                if (active.fetch_sub(1) != 1) {
                    ++failures;
                }
                guarded.mutex.Unlock();
            }
        });
    }
    start.store(true, std::memory_order_release);
    for (std::thread& worker : workers) {
        worker.join();
    }
    Require(failures.load() == 0 && counter == static_cast<std::uint64_t>(Workers) * Iterations,
        "Contended critical sections overlapped or lost an update");
    Require(guarded.mutex.m_ownerID == 0 && guarded.mutex.m_depth == 0, "Contended mutex remained owned");
    RequireGuards(guarded);
}
#endif

int main(int argc, char** argv) {
    try {
        Require(argc == 2, "Select one mutex fixture scenario");
        const char* scenario = argv[1];
        if (std::strcmp(scenario, "layout") == 0) {
            TestLayout();
        } else if (std::strcmp(scenario, "recursion") == 0) {
            TestRecursion();
        } else if (std::strcmp(scenario, "operand-isolation") == 0) {
            TestOperandIsolation();
        } else if (std::strcmp(scenario, "high-owner-id") == 0) {
            TestHighOwnerId();
        } else if (std::strcmp(scenario, "contended-try") == 0) {
            TestContendedTry();
        } else if (std::strcmp(scenario, "unlock") == 0) {
            TestUnlock();
        } else if (std::strcmp(scenario, "alignment") == 0) {
            TestAlignment();
#if defined(_WIN32)
        } else if (std::strcmp(scenario, "contention") == 0) {
            TestContention();
#endif
        } else {
            throw std::runtime_error("Unknown mutex fixture scenario");
        }
#if defined(_WIN32)
        std::printf("Windows production-header mutex passed: %s\n", scenario);
#else
        std::printf("Portable LLP64 storage/production-method-body model passed: %s\n", scenario);
#endif
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
    return 0;
}
