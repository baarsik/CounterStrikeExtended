#pragma once

// cl_util.h defines a legacy function-like fabs macro. Preserve that caller
// policy while allowing the standard header's actual declarations to parse.
#pragma push_macro("fabs")
#undef fabs
#include <cmath>
#pragma pop_macro("fabs")
#include <cstdint>
#include <limits>

namespace cse {

using CameraMouseQuery = int (*)(int *, int *, int *, int *);

struct CameraMouseSnapshot {
    int x = 0;
    int y = 0;
    int centerX = 0;
    int centerY = 0;
    int historyX = 0;
    int historyY = 0;
};

inline int CameraMouseHalfDifference(int position, int center) {
    static_assert(std::numeric_limits<int>::digits == 31, "Camera mouse coordinates require signed32 int");
    constexpr int legacyMouseDivisor = 2;
    const std::int64_t difference = static_cast<std::int64_t>(position) - center;
    // Any two signed32 positions differ by at most UINT32_MAX. Dividing first
    // preserves legacy truncation while bringing both directions into int.
    return static_cast<int>(difference / legacyMouseDivisor);
}

// Preserve the legacy float multiply and truncation for historical mouse
// positions, without invoking an out-of-range floating-to-int conversion.
inline bool TryCameraMouseHistory(int position, float sensitivity, bool unscaledWhenZero, int &result) {
    static_assert(std::numeric_limits<int>::digits == 31, "Camera mouse history requires signed32 int");
    if (unscaledWhenZero && sensitivity == 0.0f) {
        result = position;
        return true;
    }
    constexpr double lowerExclusive = static_cast<double>((std::numeric_limits<int>::min)()) - 1.0;
    constexpr double upperExclusive = static_cast<double>((std::numeric_limits<int>::max)()) + 1.0;
    const float scaled = position * sensitivity;
    if (!std::isfinite(scaled) || static_cast<double>(scaled) <= lowerExclusive
        || static_cast<double>(scaled) >= upperExclusive) {
        return false;
    }
    result = static_cast<int>(scaled);
    return true;
}

inline bool TryReadCameraMouse(CameraMouseQuery query, float sensitivity, bool unscaledWhenZero,
    CameraMouseSnapshot &result) {
    CameraMouseSnapshot next;
    if (!query || !query(&next.x, &next.y, &next.centerX, &next.centerY)
        || !TryCameraMouseHistory(next.x, sensitivity, unscaledWhenZero, next.historyX)
        || !TryCameraMouseHistory(next.y, sensitivity, unscaledWhenZero, next.historyY)) {
        return false;
    }
    result = next;
    return true;
}

}
