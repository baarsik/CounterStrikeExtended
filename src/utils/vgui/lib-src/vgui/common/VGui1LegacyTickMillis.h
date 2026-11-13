#ifndef VGUI1_LEGACY_TICK_MILLIS_H
#define VGUI1_LEGACY_TICK_MILLIS_H

#include <cstdint>
#include <limits>

namespace vgui {
namespace detail {

static_assert(std::numeric_limits<int>::digits == std::numeric_limits<std::int32_t>::digits,
    "VGUI tick storage requires a signed 32-bit int");

// Preserve the measured Windows signed32 ordering, including its wrap quirks.
static inline std::int32_t SignedTickMillis(std::uint32_t encodedMillis) noexcept {
    constexpr std::uint32_t MaximumSignedTickMillis = (std::numeric_limits<std::int32_t>::max)();
    constexpr std::uint32_t MaximumEncodedTickMillis = (std::numeric_limits<std::uint32_t>::max)();
    if (encodedMillis <= MaximumSignedTickMillis) {
        return static_cast<std::int32_t>(encodedMillis);
    }
    return -1 - static_cast<std::int32_t>(MaximumEncodedTickMillis - encodedMillis);
}

static inline std::int32_t NormalizeTickMillis(long millis) noexcept {
    return SignedTickMillis(static_cast<std::uint32_t>(millis));
}

static inline std::int32_t AddTickMillis(long millis, int intervalMillis) noexcept {
    const std::uint32_t encodedMillis = static_cast<std::uint32_t>(millis);
    const std::uint32_t encodedIntervalMillis = static_cast<std::uint32_t>(intervalMillis);
    return SignedTickMillis(encodedMillis + encodedIntervalMillis);
}

}
}

#endif
