#include "fixture.h"

static thread_local std::uint32_t forcedThreadId;

void SetFixtureThreadId(std::uint32_t value) {
    forcedThreadId = value;
}

extern "C" unsigned long ThreadGetCurrentId() {
    return forcedThreadId ? static_cast<unsigned long>(forcedThreadId) : ::GetCurrentThreadId();
}

extern "C" void ThreadSleep(unsigned int duration) {
    ::Sleep(duration);
}

extern "C" void* ThreadInterlockedCompareExchangePointer(void* volatile* destination, void* value, void* comparand) {
    return InterlockedCompareExchangePointer(destination, value, comparand);
}
