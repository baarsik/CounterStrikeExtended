#pragma once

#include "vgui/ISystem.h"

#include <cstdint>

namespace cse {
namespace vgui_control_time {

inline std::uint32_t Milliseconds(vgui2::ISystem& system) {
    constexpr double MillisecondsPerSecond = 1000.0;
    // The Windows SDK also defines a GetCurrentTime() macro.
    const double seconds = (system.GetCurrentTime)();
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(seconds * MillisecondsPerSecond));
}

// A future deadline can be at most one scheduled interval ahead. Unsigned subtraction
// keeps that distance across a wrap and treats older, long-idle controls as ready.
inline bool Waiting(std::uint32_t now, std::uint32_t deadline, std::uint32_t maximumDelay) {
    const std::uint32_t remaining = deadline - now;
    return remaining != 0 && remaining <= maximumDelay;
}

}
}
