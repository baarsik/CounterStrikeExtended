#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

// Only the relevant Windows LLP64 storage and 32-bit interlocked parameter are
// modeled. All constructor/method bodies below are extracted without changes.
using LONG = std::int32_t;
std::uint32_t ThreadGetCurrentId();
void ThreadSleep(unsigned int duration);
LONG InterlockedCompareExchange(volatile LONG* destination, LONG value, LONG comparand);
LONG InterlockedExchange(volatile LONG* destination, LONG value);
void* ThreadInterlockedCompareExchangePointer(void* volatile* destination, void* value, void* comparand);

class CThreadFastMutex {
#include "actual_mutex_members.inc"

    volatile std::uint32_t m_ownerID;
    int m_depth;
};
