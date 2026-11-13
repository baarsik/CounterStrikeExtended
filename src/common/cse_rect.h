#ifndef CSE_RECT_H
#define CSE_RECT_H

#include <stdint.h>

#if !defined(_WIN32)
// Match the Windows signed32 rectangle and its physical field order on LP64.
typedef struct tagRECT {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
} RECT;
#endif

#if defined(__cplusplus)
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#if !defined(_WIN32)
static_assert(sizeof(RECT) == 4 * sizeof(std::int32_t) && alignof(RECT) == alignof(std::int32_t),
    "Native rectangle storage matches four signed32 coordinates");
static_assert(offsetof(RECT, left) == 0 && offsetof(RECT, top) == sizeof(std::int32_t)
    && offsetof(RECT, right) == 2 * sizeof(std::int32_t) && offsetof(RECT, bottom) == 3 * sizeof(std::int32_t),
    "Native rectangle field order matches tagRECT");
static_assert(std::is_standard_layout<RECT>::value && std::is_trivial<RECT>::value,
    "Native rectangles remain plain records");
#endif

namespace cse_rect {
// Retained Win32 coordinate subtraction wraps in unsigned32 before its signed
// result is used. Map the bits explicitly; no out-of-range signed cast or UB.
constexpr std::int32_t Difference32(std::int32_t position, std::int32_t origin) noexcept {
    const std::uint32_t bits = static_cast<std::uint32_t>(position) - static_cast<std::uint32_t>(origin);
    constexpr auto SignedMaximum = (std::numeric_limits<std::int32_t>::max)();
    constexpr auto UnsignedMaximum = (std::numeric_limits<std::uint32_t>::max)();
    if (bits <= static_cast<std::uint32_t>(SignedMaximum)) {
        return static_cast<std::int32_t>(bits);
    }
    return -1 - static_cast<std::int32_t>(UnsignedMaximum - bits);
}
}
#endif

#endif
