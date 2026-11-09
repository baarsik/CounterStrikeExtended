#pragma once

#include <cstdint>
#include <limits>

namespace cse {
namespace particle_memory {

constexpr std::int32_t SpiralAddressPhase(std::uintptr_t address) {
    // The Win32 rule uses the address's low signed 32 bits as its phase.
    const std::uint32_t lowBits = static_cast<std::uint32_t>(address);
    constexpr auto maximumPositivePhase = static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max());
    if (lowBits <= maximumPositivePhase) {
        return static_cast<std::int32_t>(lowBits);
    }
    constexpr std::uint32_t maximumBits = std::numeric_limits<std::uint32_t>::max();
    return -1 - static_cast<std::int32_t>(maximumBits - lowBits);
}

}
}
