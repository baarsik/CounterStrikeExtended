#ifndef CSE_POSIX_PROCESS_STATS_H
#define CSE_POSIX_PROCESS_STATS_H

#if !defined(_WIN32)
#include <sys/resource.h>
#include <time.h>

#include <cstdint>
#include <limits>

namespace cse_process_stats {
namespace detail {

constexpr std::uint64_t NanosecondsPerSecond = UINT64_C(1000000000);
constexpr std::uint64_t MicrosecondsPerSecond = UINT64_C(1000000);

template<std::uint64_t SubsecondsPerSecond>
inline bool ToNanoseconds(std::intmax_t seconds, std::intmax_t subseconds, std::uint64_t& result) noexcept {
    static_assert(SubsecondsPerSecond > 0 && NanosecondsPerSecond % SubsecondsPerSecond == 0,
        "Process-stat fractions must have an integral nanosecond scale");
    if (seconds < 0 || subseconds < 0 || static_cast<std::uintmax_t>(subseconds) >= SubsecondsPerSecond) {
        return false;
    }
    constexpr auto Maximum = (std::numeric_limits<std::uint64_t>::max)();
    if (static_cast<std::uintmax_t>(seconds) > Maximum / NanosecondsPerSecond) {
        return false;
    }
    const auto whole = static_cast<std::uint64_t>(seconds) * NanosecondsPerSecond;
    const auto fraction = static_cast<std::uint64_t>(subseconds) * (NanosecondsPerSecond / SubsecondsPerSecond);
    if (fraction > Maximum - whole) {
        return false;
    }
    result = whole + fraction;
    return true;
}

}

// Only differences are consumed by Host_UpdateStats. Both clocks use nanoseconds;
// CPU time includes all process threads and wall time has no calendar-clock jumps.
inline bool Read(std::uint64_t& cpuTicks, std::uint64_t& runTicks) noexcept {
    rusage usage{};
    timespec elapsed{};
    if (::getrusage(RUSAGE_SELF, &usage) != 0 || ::clock_gettime(CLOCK_MONOTONIC, &elapsed) != 0) {
        return false;
    }
    std::uint64_t user = 0;
    std::uint64_t system = 0;
    std::uint64_t wall = 0;
    if (!detail::ToNanoseconds<detail::MicrosecondsPerSecond>(usage.ru_utime.tv_sec, usage.ru_utime.tv_usec, user)
        || !detail::ToNanoseconds<detail::MicrosecondsPerSecond>(usage.ru_stime.tv_sec, usage.ru_stime.tv_usec, system)
        || !detail::ToNanoseconds<detail::NanosecondsPerSecond>(elapsed.tv_sec, elapsed.tv_nsec, wall)
        || system > (std::numeric_limits<std::uint64_t>::max)() - user) {
        return false;
    }
    cpuTicks = user + system;
    runTicks = wall;
    return true;
}

}
#endif
#endif
