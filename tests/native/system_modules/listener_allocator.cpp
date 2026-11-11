#include "listener_allocator.h"
#include <cstdlib>
#include <execinfo.h>
#include <algorithm>

namespace {
void *expectedPointer = nullptr;
std::size_t expectedSize = 0;
unsigned int failures = 0;
void *failureCaller = nullptr;
bool armed = false;
constexpr int MaxFrames = 24;
void *failureFrames[MaxFrames]{};
int failureFrameCount = 0;
}

extern "C" void ListenerAllocatorArm(void *pointer, std::size_t size) {
    expectedPointer = pointer;
    expectedSize = size;
    failures = 0;
    failureCaller = nullptr;
    failureFrameCount = 0;
    // Negative control proves the tests require a physical allocation failure.
    armed = std::getenv("CSE_LISTENER_INTERPOSE_DISABLE") == nullptr;
}

extern "C" unsigned int ListenerAllocatorFailures() {
    return failures;
}

extern "C" void *ListenerAllocatorCaller() {
    return failureCaller;
}

extern "C" int ListenerAllocatorFrames(void **frames, int capacity) {
    const int count = std::min(capacity, failureFrameCount);
    std::copy_n(failureFrames, count, frames);
    return count;
}

extern "C" void *FailListenerRealloc(void *pointer, std::size_t size) {
    if (armed && pointer == expectedPointer && size == expectedSize) {
        armed = false;
        ++failures;
        failureCaller = __builtin_return_address(0);
        failureFrameCount = backtrace(failureFrames, MaxFrames);
        return nullptr;
    }
    return std::realloc(pointer, size);
}

// Mach-O interposition affects the executable and actual dlopened providers. dyld
// leaves references within this image bound to the original allocator (including
// ASan's allocator when instrumented). Production code and its allocation API stay unchanged.
__attribute__((used)) static const struct {
    void *replacement;
    void *original;
} interposition __attribute__((section("__DATA,__interpose"))) = {
    reinterpret_cast<void *>(&FailListenerRealloc), reinterpret_cast<void *>(&realloc)
};
