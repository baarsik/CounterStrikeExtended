#include "posix_process_stats.h"

#include <cstdio>

int main() {
    std::uint64_t previousCpu = 0;
    std::uint64_t previousWall = 0;
    constexpr int SampleCount = 100;
    for (int sample = 0; sample < SampleCount; ++sample) {
        std::uint64_t cpu = 0;
        std::uint64_t wall = 0;
        if (!cse_process_stats::Read(cpu, wall) || cpu < previousCpu || wall < previousWall || wall == 0) {
            std::fputs("Process CPU/monotonic provider read failed or moved backward\n", stderr);
            return 17;
        }
        previousCpu = cpu;
        previousWall = wall;
    }
    std::puts("PASS 100 real process CPU/monotonic samples; no throughput or CPU-fraction accuracy claim");
}
