#include "NativeSystemClock.h"
#include <time.h>

namespace cse_vgui2_clock {

bool ReadMonotonicCounter(std::uint64_t& output) {
    timespec sample{};
    if (clock_gettime(CLOCK_MONOTONIC, &sample) != 0) {
        return false;
    }
    static_assert(std::numeric_limits<decltype(sample.tv_sec)>::is_signed
        && sizeof(sample.tv_sec) <= sizeof(std::int64_t), "The reader requires a signed <=64-bit time_t");
    if (sample.tv_nsec < 0 || static_cast<std::uint64_t>(sample.tv_nsec) >= NanosecondsPerSecond) {
        return false;
    }
    return EncodeNanoseconds(static_cast<std::int64_t>(sample.tv_sec),
        static_cast<std::int32_t>(sample.tv_nsec), output);
}

} // namespace cse_vgui2_clock
