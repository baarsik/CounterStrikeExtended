#pragma once

#include <algorithm>
#include <cmath>

namespace cse {
namespace ui {

struct BackdropRect {
    int x;
    int y;
    int width;
    int height;
};

struct BackdropStyle {
    float opacityTop;
    float opacityBottom;
    float darkness;
};

inline float UnitValue(float value) {
    return std::isfinite(value) ? (std::clamp)(value, 0.0f, 1.0f) : 0.0f;
}

// Both the GPU composite and the fallback use the same smooth fade and endpoints.
inline float BackdropOpacity(const BackdropStyle& style, float position) {
    const float distance = UnitValue(position);
    const float smooth = distance * distance * (3.0f - 2.0f * distance);
    const float top = UnitValue(style.opacityTop);
    return top + (UnitValue(style.opacityBottom) - top) * smooth;
}

} // namespace ui
} // namespace cse
