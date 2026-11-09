#include <sys/resource.h>
#include <time.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {

constexpr std::uint64_t NanosecondsPerSecond = UINT64_C(1000000000);
constexpr std::uint64_t MicrosecondsPerSecond = UINT64_C(1000000);
constexpr std::uint64_t NanosecondsPerMicrosecond = NanosecondsPerSecond / MicrosecondsPerSecond;

rusage usage{};
timespec elapsed{};
bool failUsage = false;
bool failClock = false;
int usageCalls = 0;
int clockCalls = 0;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::exit(17);
    }
}

int ReadUsage(int who, rusage* result) {
    Require(who == RUSAGE_SELF, "CPU sample must include this process's threads");
    ++usageCalls;
    if (failUsage) {
        return -1;
    }
    *result = usage;
    return 0;
}

int ReadClock(clockid_t clock, timespec* result) {
    Require(clock == CLOCK_MONOTONIC, "CPU fraction needs monotonic wall time");
    ++clockCalls;
    if (failClock) {
        return -1;
    }
    *result = elapsed;
    return 0;
}

}

#define getrusage ReadUsage
#define clock_gettime ReadClock
#include "posix_process_stats.h"
#undef clock_gettime
#undef getrusage

namespace {

constexpr std::uint64_t CpuSentinel = UINT64_C(0x123456789abcdef0);
constexpr std::uint64_t WallSentinel = UINT64_C(0xfedcba9876543210);

void Reject(const char* message) {
    std::uint64_t cpu = CpuSentinel;
    std::uint64_t wall = WallSentinel;
    Require(!cse_process_stats::Read(cpu, wall), message);
    Require(cpu == CpuSentinel && wall == WallSentinel, "Failed sample partially committed");
}

void Reset() {
    usage = {};
    elapsed = {};
    usage.ru_utime = {3, 4};
    usage.ru_stime = {5, 6};
    elapsed = {11, 12};
    failUsage = false;
    failClock = false;
    usageCalls = 0;
    clockCalls = 0;
}

}

int main() {
    Reset();
    std::uint64_t cpu = 0;
    std::uint64_t wall = 0;
    Require(cse_process_stats::Read(cpu, wall), "Valid process sample rejected");
    Require(cpu == UINT64_C(8000010000) && wall == UINT64_C(11000000012), "CPU/wall units differ");
    Require(usageCalls == 1 && clockCalls == 1, "Sample must read each provider once");

    Reset();
    failUsage = true;
    Reject("getrusage failure accepted");
    Require(usageCalls == 1 && clockCalls == 0, "Usage failure must stop before clock read");
    Reset();
    failClock = true;
    Reject("clock failure accepted");
    Require(usageCalls == 1 && clockCalls == 1, "Clock failure call order changed");

    Reset();
    usage.ru_utime.tv_sec = -1;
    Reject("Negative user seconds accepted");
    Reset();
    usage.ru_stime.tv_usec = -1;
    Reject("Negative system fraction accepted");
    Reset();
    usage.ru_utime.tv_usec = MicrosecondsPerSecond;
    Reject("Invalid microsecond fraction accepted");
    Reset();
    elapsed.tv_sec = -1;
    Reject("Negative wall seconds accepted");
    Reset();
    elapsed.tv_nsec = NanosecondsPerSecond;
    Reject("Invalid nanosecond fraction accepted");

    constexpr auto Maximum = (std::numeric_limits<std::uint64_t>::max)();
    constexpr auto MaximumSeconds = Maximum / NanosecondsPerSecond;
    constexpr auto MaximumFraction = Maximum % NanosecondsPerSecond;
    Reset();
    usage.ru_utime.tv_sec = static_cast<decltype(usage.ru_utime.tv_sec)>(MaximumSeconds);
    usage.ru_utime.tv_usec = static_cast<decltype(usage.ru_utime.tv_usec)>(
        MaximumFraction / NanosecondsPerMicrosecond);
    usage.ru_stime = {};
    elapsed.tv_sec = static_cast<decltype(elapsed.tv_sec)>(MaximumSeconds);
    elapsed.tv_nsec = static_cast<decltype(elapsed.tv_nsec)>(MaximumFraction);
    Require(cse_process_stats::Read(cpu, wall) && wall == Maximum
        && cpu == Maximum - MaximumFraction % NanosecondsPerMicrosecond, "Largest representable sample rejected");
    ++elapsed.tv_nsec;
    Reject("Wall nanosecond conversion overflow accepted");
    --elapsed.tv_nsec;
    usage.ru_stime.tv_usec = 1;
    Reject("Combined process CPU overflow accepted");
    usage.ru_stime = {};
    ++usage.ru_utime.tv_sec;
    Reject("CPU second conversion overflow accepted");

    usage = {};
    elapsed = {};
    Require(cse_process_stats::Read(cpu, wall) && cpu == 0 && wall == 0, "Zero sample rejected");
    std::puts("PASS process CPU/wall units, exact boundaries, read failures and atomic output");
}
