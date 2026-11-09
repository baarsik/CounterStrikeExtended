#include "NativeSystemClock.h"
#include <cfenv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <type_traits>
#include "clock_fp_observation.h"

namespace {
using namespace cse_vgui2_clock;

void Require(bool okay, const char* message) {
    if (!okay) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
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

void Emit(State& state, std::uint64_t counter, unsigned int row, bool emit) {
    std::feclearexcept(FE_ALL_EXCEPT);
    const int roundingBefore = std::fegetround();
    const auto controlBefore = ClockFixtureFpControl();
    const auto simdBefore = ClockFixtureSimdControl();
    double seconds = -123.0;
    Require(Advance(state, counter, seconds), "first controlled counter advances");
    const auto secondsExceptions = Exceptions();
    const auto secondsStatus = ClockFixtureFpStatus();
    std::feclearexcept(FE_ALL_EXCEPT);
    double secondRead = -456.0;
    Require(Advance(state, counter, secondRead), "GetTimeMillis performs a second controlled read");
    const auto milliseconds = Milliseconds(secondRead);
    const auto millisExceptions = Exceptions();
    const auto millisStatus = ClockFixtureFpStatus();
    Require(ClockFixtureFpControl() == controlBefore && ClockFixtureSimdControl() == simdBefore,
        "observed FP control fields remain unchanged");
    Require(std::fegetround() == roundingBefore, "component leaves rounding mode intact");
    if (emit) {
        std::printf("{\"row\":%u,\"counter\":%llu,\"seconds_bits\":%llu,\"millis\":%d,\"seconds_flags\":%u,\"millis_flags\":%u,\"rounding\":%d,\"control\":%llu,\"simd_control\":%llu,\"seconds_status\":%llu,\"millis_status\":%llu}\n",
            row, static_cast<unsigned long long>(counter), static_cast<unsigned long long>(Bits(seconds)),
            milliseconds, secondsExceptions, millisExceptions, roundingBefore, static_cast<unsigned long long>(controlBefore),
            static_cast<unsigned long long>(simdBefore), static_cast<unsigned long long>(secondsStatus),
            static_cast<unsigned long long>(millisStatus));
    }
}

void Boundaries() {
    static_assert(std::is_standard_layout<State>::value && std::is_trivial<State>::value,
        "The unselected state has no constructor, worker, service, or ownership callback");
    static_assert(sizeof(decltype(Milliseconds(0.0))) == 4
        && std::is_signed<decltype(Milliseconds(0.0))>::value, "The native clock boundary is signed32");
    State state{};
    State before{};
    double output = 321.0;
    Require(!Advance(state, 0, output) && output == 321.0, "uninitialized state rejects without output");
    Require(!Initialize(state, 0) && std::memcmp(&state, &before, sizeof(state)) == 0,
        "zero frequency rejects without changing state");
    // This frequency requires shift 32; the actual source's high/low expression
    // would shift by 32. It is explicitly outside the preserved input domain.
    Require(!Initialize(state, UINT64_C(2000001) << 31)
        && std::memcmp(&state, &before, sizeof(state)) == 0, "unsupported shift rejects without mutation");
    Require(Initialize(state, 1000000000) && state.lowShift == 9, "nanosecond reader normalization");
    output = 321.0;
    state.lowShift = 32;
    Require(!Advance(state, 0, output) && output == 321.0, "corrupt shift rejects before shifts");
    std::uint64_t encoded = 987;
    Require(!EncodeNanoseconds(-1, 0, encoded) && encoded == 987, "negative seconds reject unchanged");
    Require(!EncodeNanoseconds(0, -1, encoded) && encoded == 987, "negative nanoseconds reject unchanged");
    Require(!EncodeNanoseconds(0, 1000000000, encoded) && encoded == 987, "invalid nanosecond span rejects");
    Require(!EncodeNanoseconds(std::numeric_limits<std::int64_t>::max(), 0, encoded)
        && encoded == 987, "nanosecond multiplication overflow rejects");
    Require(EncodeNanoseconds(18446744073, 709551615, encoded)
        && encoded == std::numeric_limits<std::uint64_t>::max(), "maximum encoded nanoseconds admitted");
    encoded = 987;
    Require(!EncodeNanoseconds(18446744073, 709551616, encoded) && encoded == 987,
        "nanosecond addition overflow rejects");
    const std::int64_t seconds[] = {0, 1, 2, 1000, 2147483647, 4294967296, 18446744072, 18446744073};
    const std::int32_t nanos[] = {0, 1, 999, 1000000, 709551615, 709551616, 999999999};
    for (auto second : seconds) {
        for (auto nano : nanos) {
            encoded = 987;
            const bool valid = EncodeNanoseconds(second, nano, encoded);
            std::printf("{\"seconds\":%lld,\"nanos\":%d,\"valid\":%s,\"encoded\":%llu}\n",
                static_cast<long long>(second), nano, valid ? "true" : "false",
                static_cast<unsigned long long>(encoded));
        }
    }
    // Copying the pure state does not share a hidden function-static origin.
    Require(Initialize(state, 1000000), "fresh independent state initializes");
    Require(Advance(state, 123, output) && output == 0.0, "first sample establishes origin");
    State copy = state;
    Require(Advance(state, 124, output), "first state advances");
    Require(copy.oldTime == 123 && copy.currentTime == 0.0, "no hidden shared origin between states");
    std::fprintf(stderr, "PASS pure state failure/ownership/encoding boundaries\n");
}
}

int main(int argc, char** argv) {
    Require(argc >= 2, "scenario supplied");
    if (std::strcmp(argv[1], "boundaries") == 0) {
        Boundaries();
        return 0;
    }
    Require(argc == 3, "frequency supplied");
    const auto frequency = std::strtoull(argv[2], nullptr, 10);
    State state{};
    std::feclearexcept(FE_ALL_EXCEPT);
    Require(Initialize(state, frequency), "valid controlled frequency initializes");
    std::fprintf(stderr, "constructor_flags=%u shift=%u\n", Exceptions(), state.lowShift);
    if (std::strcmp(argv[1], "normal") == 0) {
        const std::uint32_t values[] = {0, 1, 1, 2, 100, 90, 91, 0x10000000U, 0, 0xffffffffU, 0, 1,
            0x7fffffffU, 0x80000000U, 0xffffffffU, 0xfffffffeU, 0xffffffffU, 0, 5};
        unsigned int row = 0;
        for (auto value : values) {
            Emit(state, static_cast<std::uint64_t>(value) << state.lowShift, row++, true);
        }
    } else if (std::strcmp(argv[1], "seeded") == 0) {
        std::uint64_t seed = UINT64_C(0x927add34e571c083);
        for (unsigned int row = 0; row < 1024; ++row) {
            seed ^= seed << 13;
            seed ^= seed >> 7;
            seed ^= seed << 17;
            Emit(state, seed, row, true);
        }
    } else if (std::strcmp(argv[1], "same-time") == 0) {
        Require(state.lowShift == 0, "same-time fixture frequency requires no shift");
        Emit(state, 0, 0, true);
        state.currentTime = 1099511627776.0;
        state.lastCurrentTime = 1099511627776.0;
        for (unsigned int row = 1; row <= 100001; ++row) {
            Emit(state, row, row, row == 1 || row >= 99999);
        }
    } else if (std::strcmp(argv[1], "millis") == 0) {
        const double values[] = {0.0, -0.0, -0.0005, 0.0005, 16777.217, 2147483.647, 2147483.6479,
            2147483.648, -2147483.648, -2147483.6489, 4294967.295};
        unsigned int row = 0;
        for (auto value : values) {
            state.currentTime = value;
            state.lastCurrentTime = value;
            Emit(state, 0, row++, true);
        }
    } else {
        Require(false, "known controlled scenario");
    }
    return 0;
}
