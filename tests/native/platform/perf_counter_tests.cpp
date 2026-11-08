#include <sys/time.h>

namespace {
constexpr time_t FutureEpoch = static_cast<time_t>(1) << 33;
time_t seconds = FutureEpoch;
suseconds_t microseconds = 250000;

int FixtureGetTime(timeval *value, void *) {
    value->tv_sec = seconds;
    value->tv_usec = microseconds;
    return 0;
}
} // namespace

// Drive the production POSIX counter at a timestamp beyond the Win32 integer range.
#define gettimeofday FixtureGetTime
#include "../../../GoldSrc-SDK/game_shared/perf_counter.h"
#undef gettimeofday

int main() {
    CPerformanceCounter counter;
    if (counter.GetCurTime() != 0.25) {
        return 1;
    }
    ++seconds;
    microseconds = 500000;
    return counter.GetCurTime() == 1.5 ? 0 : 1;
}
