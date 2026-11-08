#include "tier0/platform.h"
#include "tier0/fasttimer.h"
#include "../../../GoldSrc-SDK/tier0/cpu_topology.h"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#if CSE_CPU_X86 && !defined(_WIN32)
#include <cpuid.h>
#endif

extern bool CheckMMXTechnology();
extern bool CheckSSETechnology();
extern bool CheckSSE2Technology();
extern bool Check3DNowTechnology();
extern bool CheckCMOVTechnology();
extern bool CheckFCMOVTechnology();
extern bool CheckRDTSCTechnology();
extern const char* GetProcessorVendorId();

// The production CPU implementation's fatal diagnostic is a fixture failure.
extern "C" void Error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
    std::abort();
}

namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "CPU fixture failed: %s\n", message);
        std::exit(1);
    }
}
} // namespace

int main() {
    const CPUInformation* info = GetCPUInformation();
    Require(info == GetCPUInformation(), "stable information identity");
    Require(info->m_Size == sizeof(*info), "native structure size");
    Require(info->m_Speed > 0 && g_ClockSpeed == static_cast<uint64>(info->m_Speed), "positive matching tick rate");
    Require(info->m_nLogicalProcessors >= 1 && info->m_nPhysicalProcessors >= 1, "nonzero topology");
    Require(info->m_nPhysicalProcessors <= info->m_nLogicalProcessors, "bounded physical cores");
    Require(info->m_szProcessorID && std::strlen(info->m_szProcessorID) <= 12, "bounded vendor identity");
    Require(std::strcmp(info->m_szProcessorID, GetProcessorVendorId()) == 0, "vendor identity stable");
    Require(info->m_bMMX == CheckMMXTechnology() && info->m_bSSE == CheckSSETechnology()
        && info->m_bSSE2 == CheckSSE2Technology() && info->m_b3DNow == Check3DNowTechnology()
        && info->m_bCMOV == CheckCMOVTechnology() && info->m_bFCMOV == CheckFCMOVTechnology()
        && info->m_bRDTSC == CheckRDTSCTechnology(), "feature discovery agrees with production checks");
#if defined(_M_ARM64) || defined(__aarch64__)
    Require(!info->m_bRDTSC && !info->m_bCMOV && !info->m_bFCMOV && !info->m_bMMX
        && !info->m_bSSE && !info->m_bSSE2 && !info->m_b3DNow, "ARM does not advertise x86 instructions");
    Require(std::strcmp(info->m_szProcessorID, "GenericARM64") == 0, "ARM architecture identity");
#endif
    const auto empty = cse_cpu::BoundTopology(0, 0);
    Require(empty.logical == 1 && empty.physical == 1, "empty topology fallback");
    const auto excessive = cse_cpu::BoundTopology(1024, 512);
    Require(excessive.logical == 255 && excessive.physical == 255, "legacy byte counts saturate");
    const auto reversed = cse_cpu::BoundTopology(4, 8);
    Require(reversed.logical == 4 && reversed.physical == 4, "physical count bounded by logical count");
    const auto boundary = cse_cpu::BoundTopology(256, 255);
    Require(boundary.logical == 255 && boundary.physical == 255, "count does not wrap to zero");
#if CSE_CPU_X86
    unsigned int eax = 0;
    unsigned int ebx = 0;
    unsigned int ecx = 0;
    unsigned int edx = 0;
#if defined(_WIN32)
    int registers[4] = {};
    __cpuid(registers, 1);
    edx = static_cast<unsigned int>(registers[3]);
    (void)eax;
    (void)ebx;
    (void)ecx;
#else
    __cpuid(1, eax, ebx, ecx, edx);
#endif
    constexpr unsigned int MmxBit = 1u << 23;
    constexpr unsigned int SseBit = 1u << 25;
    constexpr unsigned int Sse2Bit = 1u << 26;
    constexpr unsigned int RdtscBit = 1u << 4;
    Require(info->m_bMMX == ((edx & MmxBit) != 0) && info->m_bSSE == ((edx & SseBit) != 0)
        && info->m_bSSE2 == ((edx & Sse2Bit) != 0) && info->m_bRDTSC == ((edx & RdtscBit) != 0),
        "x86 features match independent instruction query");
#endif
#if defined(_WIN32)
    LARGE_INTEGER nativeFrequency;
    Require(QueryPerformanceFrequency(&nativeFrequency) != 0, "independent Windows timer frequency");
    Require(Plat_PerformanceFrequency() == static_cast<uint64>(nativeFrequency.QuadPart), "native timer rate");
#else
    Require(Plat_PerformanceFrequency() == 1000000000, "POSIX timer nanosecond rate");
#endif
#if CSE_CPU_ARM64
    Require(static_cast<uint64>(info->m_Speed) == Plat_PerformanceFrequency(), "ARM profiler timer rate");
#endif
    const uint64 rawStart = Plat_PerformanceCounter();
    CFastTimer timer;
    const auto start = std::chrono::steady_clock::now();
    timer.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    timer.End();
    const double elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    const double measured = timer.m_Duration.GetMillisecondsF();
    const uint64 rawEnd = Plat_PerformanceCounter();
    const double nativeMilliseconds = static_cast<double>(rawEnd - rawStart) * 1000.0
        / static_cast<double>(Plat_PerformanceFrequency());
    Require(nativeMilliseconds > elapsed * 0.5 && nativeMilliseconds < elapsed * 2.0,
        "native timer sample and frequency agree");
    // Scheduling may delay this process. Compare with independently measured elapsed time, not requested sleep.
    Require(measured > elapsed * 0.5 && measured < elapsed * 2.0, "counter and frequency use the same units");
    CCycleCount previous;
    previous.Sample();
    for (int index = 0; index < 10000; ++index) {
        CCycleCount next;
        next.Sample();
        Require(next.m_Int64 >= previous.m_Int64, "monotonic profiler samples");
        previous = next;
    }
    CCycleCount initialized;
    initialized.Init(10.0f);
    const double roundTrip = initialized.GetMillisecondsF();
    Require(roundTrip > 9.99 && roundTrip <= 10.0, "millisecond initialization units");
    std::printf("CPU vendor=%s logical=%u physical=%u tick_rate=%lld measured_ms=%.6f elapsed_ms=%.6f\n",
        info->m_szProcessorID, static_cast<unsigned>(info->m_nLogicalProcessors),
        static_cast<unsigned>(info->m_nPhysicalProcessors), info->m_Speed, measured, elapsed);
    return 0;
}
