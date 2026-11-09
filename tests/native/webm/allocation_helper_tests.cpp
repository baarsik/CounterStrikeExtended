#if defined(CSE_WEBM_OLD_ALLOCATION)
namespace cse {
namespace webm {
short* AllocatePcm(unsigned int count);
}
}
#else
#include "webm_pcm_allocation.h"
#endif

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <new>

namespace {

struct AllocationRequest {
};

std::size_t requestedBytes = 0;
unsigned int allocationCalls = 0;
bool observeRequest = false;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "PCM allocation fixture: %s\n", message);
        std::exit(1);
    }
}

}

void* operator new[](std::size_t bytes) {
    requestedBytes = bytes;
    ++allocationCalls;
    if (observeRequest) {
        throw AllocationRequest();
    }
    // Force a real allocation even for the legal zero-element case.
    void* allocation = std::malloc(bytes == 0 ? 1 : bytes);
    if (allocation == nullptr) {
        throw std::bad_alloc();
    }
    return allocation;
}

void operator delete[](void* allocation) noexcept {
    std::free(allocation);
}

#if defined(__cpp_sized_deallocation)
void operator delete[](void* allocation, std::size_t) noexcept {
    std::free(allocation);
}
#endif

int main() {
    static_assert(sizeof(short) == 2 && sizeof(unsigned int) == 4, "Retained PCM element/count widths");
    constexpr unsigned int maximumSamples = UINT_MAX / static_cast<unsigned int>(sizeof(short));
    const unsigned int ordinaryCounts[] = {0, 1, 4096};
    for (unsigned int count : ordinaryCounts) {
        short* allocation = cse::webm::AllocatePcm(count);
        Require(allocation != nullptr && requestedBytes == static_cast<std::size_t>(count) * sizeof(short),
            "ordinary/empty allocation preserves the exact byte request");
        delete[] allocation;
    }
    observeRequest = true;
    const unsigned int boundaryCounts[] = {0x3F800000u, 0x3F800001u, maximumSamples - 1, maximumSamples};
    for (unsigned int count : boundaryCounts) {
        allocationCalls = 0;
        bool observed = false;
        try {
            delete[] cse::webm::AllocatePcm(count);
        } catch (const AllocationRequest&) {
            observed = true;
        } catch (...) {
        }
        Require(observed && allocationCalls == 1
                && requestedBytes == static_cast<std::size_t>(count) * sizeof(short),
            "every compiler-admissible Win32 count reaches the allocator without a guessed lower bound");
    }
    const unsigned int invalidCounts[] = {maximumSamples + 1, UINT_MAX};
    for (unsigned int count : invalidCounts) {
        allocationCalls = 0;
        bool rejected = false;
        try {
            delete[] cse::webm::AllocatePcm(count);
        } catch (const std::bad_array_new_length&) {
            rejected = true;
        } catch (...) {
        }
        Require(rejected && allocationCalls == 0, "Win32 overflowing array count retains exact exception type");
    }
    std::puts("PCM allocation bounds passed: ordinary3, admissible4, invalid2; no large allocation performed");
    return 0;
}
