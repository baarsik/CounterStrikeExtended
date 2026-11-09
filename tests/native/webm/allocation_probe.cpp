#include "allocation_investigation.h"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <new>

AllocationObservation allocationObservation = {};

// No large allocation is made. This records whether array-new reaches the allocator,
// including an overflow sentinel, then throws an identifiable controlled exception.
void* operator new[](std::size_t bytes) {
    allocationObservation.bytes = bytes;
    ++allocationObservation.calls;
    throw AllocationReached();
}

void operator delete[](void* allocation) noexcept {
    std::free(allocation);
}

#if defined(__cpp_sized_deallocation)
void operator delete[](void* allocation, std::size_t) noexcept {
    std::free(allocation);
}
#endif

short* ExplicitInvalidLength(unsigned int) {
    throw std::bad_array_new_length();
}

namespace {

struct OuterLifetime {
    ~OuterLifetime() {
        ++allocationObservation.outerDestructors;
    }
};

void Probe(const char* label, unsigned int variant, unsigned int count, bool bridge) {
    allocationObservation = {};
    const char* result = "returned";
    try {
        OuterLifetime lifetime;
        if (bridge) {
            if (variant == 3) {
                delete[] CallThroughNoExceptionsPlain(count);
            } else if (variant == 4) {
                delete[] CallThroughNoExceptionsScalar(count);
            } else {
                CallThroughNoExceptions(variant, count);
            }
        } else {
            short* allocation = variant == 0 ? ReferenceWindowsAllocation(count) : ReferenceNativeAllocation(count);
            delete[] allocation;
        }
    } catch (const AllocationReached&) {
        result = "allocator-reached";
    } catch (const std::bad_array_new_length&) {
        result = "bad-array-new-length";
    } catch (const std::bad_alloc&) {
        result = "bad-alloc";
    } catch (...) {
        result = "other";
    }
    std::printf("%s,%u,%s,%llu,%u,%u,%u\n", label, count, result,
        static_cast<unsigned long long>(allocationObservation.bytes), allocationObservation.calls,
        allocationObservation.innerDestructors, allocationObservation.outerDestructors);
}

void RecordTermination() {
    std::fprintf(stderr, "terminate,allocation_bytes=%llu,calls=%u,inner_destructors=%u,outer_destructors=%u\n",
        static_cast<unsigned long long>(allocationObservation.bytes), allocationObservation.calls,
        allocationObservation.innerDestructors, allocationObservation.outerDestructors);
    std::fflush(nullptr);
    constexpr int observedTerminationExitCode = 86;
    std::_Exit(observedTerminationExitCode);
}

}

int main(int argc, char** argv) {
    static_assert(sizeof(unsigned int) == 4, "Actual PCM expression uses unsigned32 products");
    std::set_terminate(RecordTermination);
    if (argc == 2) {
        const char* cases[] = {"windows-noeh-caller", "native-noeh-caller", "invalid-length-noeh-caller",
            "plain-noeh-caller", "scalar-noeh-caller"};
        for (unsigned int index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
            if (std::strcmp(argv[1], cases[index]) == 0) {
                Probe(cases[index], index, 4096, true);
                return 0;
            }
        }
        return 2;
    }
    constexpr unsigned int inheritedLimit = 0x3F800000u;
    constexpr unsigned int signedByteLimit = 0x7FFFFFFFu;
    const unsigned int counts[] = {
        0, 1, 4096, inheritedLimit - 1, inheritedLimit, inheritedLimit + 1,
        signedByteLimit / sizeof(short), signedByteLimit / sizeof(short) + 1,
        signedByteLimit, signedByteLimit + 1, UINT_MAX
    };
    std::printf("pointer_bytes=%zu,long_bytes=%zu,size_bytes=%zu\n", sizeof(void*), sizeof(long), sizeof(std::size_t));
    std::puts("variant,count,result,allocation_bytes,allocation_calls,inner_destructors,outer_destructors");
    for (unsigned int count : counts) {
        Probe("windows", 0, count, false);
        Probe("native-current", 1, count, false);
    }
    return 0;
}
