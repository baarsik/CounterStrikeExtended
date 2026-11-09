#define NOMINMAX
#include <windows.h>
#undef GetCurrentTime
#undef ShellExecute
#include "vgui/ISystem.h"
#include <cfenv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>
#include "clock_fp_observation.h"
using namespace vgui2;
namespace {
std::uint64_t frequency = 10000000;
std::uint64_t ticks = 0;
CRITICAL_SECTION* activeLock = nullptr;
int depth = 0;
int initializations = 0;
int frequencyReads = 0;
int deletions = 0;
std::vector<int> trace;
struct ControlledFailure {};
bool failFrequency = false;
bool failCounter = false;
void Require(bool okay, const char* message) {
    if (!okay) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
[[noreturn]] void UnexpectedSystemApi(const char* name) {
    std::fprintf(stderr, "FAIL: unrelated actual System method called: %s\n", name);
    std::abort();
}
void WINAPI ControlledInitialize(CRITICAL_SECTION* section) {
    Require(!activeLock && depth == 0, "one actual constructor lock initialization");
    activeLock = section;
    ++initializations;
}
void WINAPI ControlledDelete(CRITICAL_SECTION* section) {
    Require(section == activeLock && depth == 0, "destruction observes quiescent matching lock");
    activeLock = nullptr;
    ++deletions;
}
void WINAPI ControlledEnter(CRITICAL_SECTION* section) {
    Require(section == activeLock && depth == 0, "enter actual selected lock once");
    ++depth;
    trace.push_back(1);
}
void WINAPI ControlledLeave(CRITICAL_SECTION* section) {
    Require(section == activeLock && depth == 1, "leave selected matching lock");
    --depth;
    trace.push_back(3);
}
BOOL WINAPI ControlledFrequency(LARGE_INTEGER* output) {
    Require(depth == 0 && initializations == 1, "frequency follows lock initialization outside the lock");
    if (failFrequency) {
        throw ControlledFailure{};
    }
    std::memcpy(&output->QuadPart, &frequency, sizeof(frequency));
    ++frequencyReads;
    return TRUE;
}
BOOL WINAPI ControlledCounter(LARGE_INTEGER* output) {
    Require(depth == 1, "read counter occurs inside actual critical section");
    trace.push_back(2);
    if (failCounter) {
        throw ControlledFailure{};
    }
    std::memcpy(&output->QuadPart, &ticks, sizeof(ticks));
    return TRUE;
}
std::uint64_t Bits(double value) {
    std::uint64_t result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}
unsigned int Exceptions() {
    const int flags = std::fetestexcept(FE_ALL_EXCEPT);
    return ((flags & FE_INVALID) ? 1U : 0U) | ((flags & FE_DIVBYZERO) ? 2U : 0U)
        | ((flags & FE_OVERFLOW) ? 4U : 0U) | ((flags & FE_UNDERFLOW) ? 8U : 0U)
        | ((flags & FE_INEXACT) ? 16U : 0U);
}
}
#include "actual_class.inc"
#include "unrelated_methods.inc"
#define InitializeCriticalSection ControlledInitialize
#define DeleteCriticalSection ControlledDelete
#define EnterCriticalSection ControlledEnter
#define LeaveCriticalSection ControlledLeave
#define QueryPerformanceFrequency ControlledFrequency
#define QueryPerformanceCounter ControlledCounter
#include "actual_arithmetic.inc"
#undef InitializeCriticalSection
#undef DeleteCriticalSection
#undef EnterCriticalSection
#undef LeaveCriticalSection
#undef QueryPerformanceFrequency
#undef QueryPerformanceCounter

// Standard explicit-instantiation access rule; no macro changes to the class,
// no guessed layout/raw offsets, and no raw member-pointer addresses.
template<class Tag, typename Tag::Member member>
struct Access {
    friend typename Tag::Member Get(Tag) {
        return member;
    }
};
struct CurrentTag {
    using Member = double CSystem::*;
    friend Member Get(CurrentTag);
};
struct LastTag {
    using Member = double CSystem::*;
    friend Member Get(LastTag);
};
struct ShiftTag {
    using Member = int CSystem::*;
    friend Member Get(ShiftTag);
};
template struct Access<CurrentTag, &CSystem::m_flCurTime>;
template struct Access<LastTag, &CSystem::m_flLastCurTime>;
template struct Access<ShiftTag, &CSystem::m_iLowShift>;
namespace {
void Emit(CSystem& system, unsigned int row, bool emit) {
    trace.clear();
    std::feclearexcept(FE_ALL_EXCEPT);
    const int roundingBefore = std::fegetround();
    const auto controlBefore = ClockFixtureFpControl();
    const auto simdBefore = ClockFixtureSimdControl();
    const double seconds = system.GetCurrentTime();
    const auto secondsExceptions = Exceptions();
    const auto secondsStatus = ClockFixtureFpStatus();
    std::feclearexcept(FE_ALL_EXCEPT);
    const long milliseconds = system.GetTimeMillis();
    const auto millisExceptions = Exceptions();
    const auto millisStatus = ClockFixtureFpStatus();
    Require(ClockFixtureFpControl() == controlBefore && ClockFixtureSimdControl() == simdBefore,
        "observed FP control fields remain unchanged");
    Require(depth == 0 && trace == std::vector<int>({1, 2, 3, 1, 2, 3}), "both actual reads preserve lock/read/leave order");
    Require(std::fegetround() == roundingBefore, "source leaves rounding mode intact");
    if (emit) {
        std::printf("{\"row\":%u,\"counter\":%llu,\"seconds_bits\":%llu,\"millis\":%ld,\"seconds_flags\":%u,\"millis_flags\":%u,\"rounding\":%d,\"control\":%llu,\"simd_control\":%llu,\"seconds_status\":%llu,\"millis_status\":%llu}\n",
            row, static_cast<unsigned long long>(ticks), static_cast<unsigned long long>(Bits(seconds)), milliseconds,
            secondsExceptions, millisExceptions, roundingBefore, static_cast<unsigned long long>(controlBefore),
            static_cast<unsigned long long>(simdBefore), static_cast<unsigned long long>(secondsStatus),
            static_cast<unsigned long long>(millisStatus));
    }
}
}
int main(int argc, char** argv) {
    Require(argc >= 3, "mode and positive frequency are supplied");
    frequency = std::strtoull(argv[2], nullptr, 10);
    Require(frequency > 0, "controlled valid frequency");
    void* storage = ::operator new(sizeof(CSystem));
    std::memset(storage, 0, sizeof(CSystem)); // Before construction: actual global object has zero initialization.
    std::feclearexcept(FE_ALL_EXCEPT);
    if (std::strcmp(argv[1], "constructor-throw") == 0) {
        failFrequency = true;
        try {
            new (storage) CSystem;
            Require(false, "controlled constructor exception propagates");
        } catch (const ControlledFailure&) {
            Require(initializations == 1 && deletions == 0 && activeLock && depth == 0,
                "selected failed constructor does not call destructor/delete initialized lock");
            std::puts("{\"constructor_exception_propagated\":true,\"initialized_lock_deleted_by_source\":false}");
        }
        ControlledDelete(activeLock); // Controlled fixture cleanup; not selected source behavior.
        ::operator delete(storage);
        return 0;
    }
    CSystem* system = new (storage) CSystem;
    const auto constructorFlags = Exceptions();
    const int shift = system->*Get(ShiftTag{});
    Require(shift >= 0 && shift < 32 && frequencyReads == 1, "admitted selected frequency normalization");
    std::fprintf(stderr, "constructor_flags=%u shift=%d initializations=%d\n", constructorFlags, shift, initializations);
    if (std::strcmp(argv[1], "counter-throw") == 0) {
        failCounter = true;
        try {
            system->GetCurrentTime();
            Require(false, "controlled counter exception propagates");
        } catch (const ControlledFailure&) {
            Require(depth == 1 && trace == std::vector<int>({1, 2}),
                "selected exceptional counter path does not leave its lock");
            std::puts("{\"counter_exception_propagated\":true,\"lock_left_by_source\":false}");
        }
        ControlledLeave(activeLock); // Controlled fixture cleanup; not selected source behavior.
    } else if (std::strcmp(argv[1], "normal") == 0) {
        const std::uint32_t values[] = {0, 1, 1, 2, 100, 90, 91, 0x10000000U, 0, 0xffffffffU, 0, 1,
            0x7fffffffU, 0x80000000U, 0xffffffffU, 0xfffffffeU, 0xffffffffU, 0, 5};
        unsigned int row = 0;
        for (auto value : values) {
            ticks = static_cast<std::uint64_t>(value) << shift;
            Emit(*system, row++, true);
        }
    } else if (std::strcmp(argv[1], "seeded") == 0) {
        std::uint64_t seed = UINT64_C(0x927add34e571c083);
        for (unsigned int row = 0; row < 1024; ++row) {
            seed ^= seed << 13;
            seed ^= seed >> 7;
            seed ^= seed << 17;
            ticks = seed;
            Emit(*system, row, true);
        }
    } else if (std::strcmp(argv[1], "same-time") == 0) {
        Require(shift == 0, "same-time fixture frequency requires no shift");
        ticks = 0;
        Emit(*system, 0, true);
        system->*Get(CurrentTag{}) = 1099511627776.0;
        system->*Get(LastTag{}) = 1099511627776.0;
        for (unsigned int row = 1; row <= 100001; ++row) {
            ticks = row;
            Emit(*system, row, row == 1 || row >= 99999);
        }
    } else if (std::strcmp(argv[1], "millis") == 0) {
        const double values[] = {0.0, -0.0, -0.0005, 0.0005, 16777.217, 2147483.647, 2147483.6479,
            2147483.648, -2147483.648, -2147483.6489, 4294967.295};
        unsigned int row = 0;
        for (auto value : values) {
            system->*Get(CurrentTag{}) = value;
            system->*Get(LastTag{}) = value;
            ticks = 0;
            Emit(*system, row++, true);
        }
    } else {
        Require(false, "known reference mode");
    }
    system->CSystem::~CSystem();
    ::operator delete(storage);
    Require(deletions == 1 && !activeLock, "actual destructor has exactly one quiescent delete");
    std::fprintf(stderr, "destructions=%d\n", deletions);
    return 0;
}
