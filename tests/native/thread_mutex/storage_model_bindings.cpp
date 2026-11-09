#include "fixture.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <thread>

static std::atomic<std::uint32_t> nextThreadId{1};
static thread_local std::uint32_t threadId = nextThreadId.fetch_add(1);
static thread_local std::uint32_t forcedThreadId;
static std::mutex pointerCasMutex;

void SetFixtureThreadId(std::uint32_t value) {
    forcedThreadId = value;
}

std::uint32_t ThreadGetCurrentId() {
    return forcedThreadId ? forcedThreadId : threadId;
}

void ThreadSleep(unsigned int duration) {
    if (duration) {
        std::this_thread::sleep_for(std::chrono::milliseconds(duration));
    } else {
        std::this_thread::yield();
    }
}

LONG InterlockedCompareExchange(volatile LONG* destination, LONG value, LONG comparand) {
    __atomic_compare_exchange_n(destination, &comparand, value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return comparand;
}

void* ThreadInterlockedCompareExchangePointer(void* volatile* destination, void* value, void* comparand) {
    // Safe byte-model negative reference: reproduce the old pointer-width CAS
    // operand over owner+depth without executing a misaligned pointer atomic.
    std::lock_guard<std::mutex> lock(pointerCasMutex);
    void* old = nullptr;
    void* storage = const_cast<void*>(static_cast<const volatile void*>(destination));
    std::memcpy(&old, storage, sizeof(old));
    if (old == comparand) {
        std::memcpy(storage, &value, sizeof(value));
    }
    return old;
}
