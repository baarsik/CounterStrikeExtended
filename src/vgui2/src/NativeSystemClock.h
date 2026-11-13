#ifndef CSE_VGUI2_NATIVE_SYSTEM_CLOCK_H
#define CSE_VGUI2_NATIVE_SYSTEM_CLOCK_H

#include "../../common/legacy_double_to_int.h"
#include <cstdint>
#include <limits>

namespace cse_vgui2_clock {

constexpr std::uint64_t NanosecondsPerSecond = UINT64_C(1000000000);

// One instance must replace the selected image's singleton/function-static state.
// The future owner must serialize counter reads AND Advance with the same lock,
// and quiesce callers before destroying the state, lock, or containing image.
struct State {
    unsigned int lowShift;
    double frequencyReciprocal;
    bool first;
    std::uint32_t oldTime;
    std::uint32_t sameTimeCount;
    double currentTime;
    double lastCurrentTime;
    bool initialized;
};

inline bool Initialize(State& state, std::uint64_t frequency) {
    constexpr std::uint64_t MaximumReducedFrequency = UINT64_C(2000000);
    constexpr unsigned int CounterWordBits = std::numeric_limits<std::uint32_t>::digits;
    if (frequency == 0) {
        return false;
    }
    unsigned int shift = 0;
    while (frequency > MaximumReducedFrequency) {
        ++shift;
        frequency >>= 1;
    }
    // The selected high/low extraction only admits shifts 0..31. Reject a
    // frequency outside that domain rather than reproduce an undefined shift.
    if (shift >= CounterWordBits) {
        return false;
    }
    State candidate{};
    candidate.lowShift = shift;
    candidate.frequencyReciprocal = 1.0 / static_cast<double>(frequency);
    candidate.first = true;
    candidate.initialized = true;
    state = candidate;
    return true;
}

inline bool Advance(State& state, std::uint64_t counter, double& output) {
    constexpr unsigned int CounterWordBits = std::numeric_limits<std::uint32_t>::digits;
    constexpr std::uint32_t MaximumIgnoredBackwardStep = UINT32_C(0x10000000);
    constexpr std::uint32_t SameTimeLimit = UINT32_C(100000);
    if (!state.initialized || state.lowShift >= CounterWordBits) {
        return false;
    }
    const std::uint32_t sample = static_cast<std::uint32_t>(counter >> state.lowShift);
    if (state.first) {
        state.oldTime = sample;
        state.first = false;
    } else if (sample <= state.oldTime && state.oldTime - sample < MaximumIgnoredBackwardStep) {
        state.oldTime = sample;
    } else {
        const std::uint32_t delta = sample - state.oldTime;
        const double elapsed = static_cast<double>(delta) * state.frequencyReciprocal;
        state.oldTime = sample;
        state.currentTime += elapsed;
        if (state.currentTime == state.lastCurrentTime) {
            ++state.sameTimeCount;
            if (state.sameTimeCount > SameTimeLimit) {
                state.currentTime += 1.0;
                state.sameTimeCount = 0;
            }
        } else {
            state.sameTimeCount = 0;
        }
        state.lastCurrentTime = state.currentTime;
    }
    output = state.currentTime;
    return true;
}

inline std::int32_t Milliseconds(double seconds) {
    constexpr double MillisecondsPerSecond = 1000.0;
    return CseLegacyTruncateDoubleToInt(seconds * MillisecondsPerSecond);
}

inline bool EncodeNanoseconds(std::int64_t seconds, std::int32_t nanoseconds, std::uint64_t& output) {
    if (seconds < 0 || nanoseconds < 0 || static_cast<std::uint64_t>(nanoseconds) >= NanosecondsPerSecond) {
        return false;
    }
    const std::uint64_t positiveSeconds = static_cast<std::uint64_t>(seconds);
    const std::uint64_t positiveNanoseconds = static_cast<std::uint64_t>(nanoseconds);
    constexpr std::uint64_t MaximumCounter = std::numeric_limits<std::uint64_t>::max();
    if (positiveSeconds > (MaximumCounter - positiveNanoseconds) / NanosecondsPerSecond) {
        return false;
    }
    output = positiveSeconds * NanosecondsPerSecond + positiveNanoseconds;
    return true;
}

// Compile-only POSIX reader in this component packet. Failure leaves output
// untouched; a future legacy getter must diagnose and fail before returning time.
// This function does not acquire the future owner's clock lock.
bool ReadMonotonicCounter(std::uint64_t& output);

} // namespace cse_vgui2_clock

#endif
