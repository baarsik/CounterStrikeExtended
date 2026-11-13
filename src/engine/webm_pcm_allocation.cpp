#include "webm_pcm_allocation.h"

#include <climits>
#include <cstddef>
#include <new>

namespace cse {
namespace webm {

short* AllocatePcm(unsigned int sampleCount) {
    static_assert(sizeof(unsigned int) == 4 && sizeof(short) == 2, "Retained Win32 PCM count/element widths");
    // MSVC Win32 new short[count] admits byte counts through UINT32_MAX - 1.
    // A larger count uses its overflow sentinel and throws bad_array_new_length.
    constexpr unsigned int maximumSamples = UINT_MAX / static_cast<unsigned int>(sizeof(short));
    if (sampleCount > maximumSamples) {
        throw std::bad_array_new_length();
    }
    return static_cast<short*>(::operator new[](static_cast<std::size_t>(sampleCount) * sizeof(short)));
}

}
}
