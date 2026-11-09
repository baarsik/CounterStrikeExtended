#pragma once

#include "../common/sdl3_input.h"
#include "ivideomode.h"
#include "vid.h"
#include <cmath>
#include <cstdint>
#include <limits>

namespace cse {
struct MouseCoordinateSpace {
    int windowWidth = 0;
    int windowHeight = 0;
    int renderWidth = 0;
    int renderHeight = 0;
    float aspectX = 1.0f;
    float aspectY = 1.0f;
};

inline MouseCoordinateSpace GetMouseCoordinateSpace(SDL_Window *window) {
    MouseCoordinateSpace space;
    SDL_GetWindowSize(window, &space.windowWidth, &space.windowHeight);
    if (VideoMode_IsWindowed()) {
        SDL_GetWindowSizeInPixels(window, &space.renderWidth, &space.renderHeight);
    } else {
        VideoMode_GetCurrentVideoMode(&space.renderWidth, &space.renderHeight, nullptr);
        space.aspectX = GetXMouseAspectRatioAdjustment();
        space.aspectY = GetYMouseAspectRatioAdjustment();
    }
    return space;
}

inline void WindowToEngineMousePosition(SDL_Window *window, float windowX, float windowY, int &x, int &y) {
    const MouseCoordinateSpace space = GetMouseCoordinateSpace(window);
    x = WindowToRenderCoordinate(windowX, space.windowWidth, space.renderWidth, space.aspectX);
    y = WindowToRenderCoordinate(windowY, space.windowHeight, space.renderHeight, space.aspectY);
}

inline void GetEngineMousePosition(SDL_Window *window, int *x, int *y) {
    float windowX = 0.0f;
    float windowY = 0.0f;
    SDL_GetMouseState(&windowX, &windowY);
    WindowToEngineMousePosition(window, windowX, windowY, *x, *y);
}

inline void WarpEngineMousePosition(SDL_Window *window, int x, int y) {
    const MouseCoordinateSpace space = GetMouseCoordinateSpace(window);
    SDL_WarpMouseInWindow(window, RenderToWindowCoordinate(x, space.windowWidth, space.renderWidth, space.aspectX),
                          RenderToWindowCoordinate(y, space.windowHeight, space.renderHeight, space.aspectY));
}

// Main-thread query only. Owning engine TUs use precise FP without contraction;
// keep the legacy float scale/truncation followed by double aspect/truncation.
inline bool TryGetEngineMousePositionAndCenter(SDL_Window *window, int *x, int *y, int *centerX, int *centerY) {
    if (!window || !videomode || !x || !y || !centerX || !centerY) {
        return false;
    }

    MouseCoordinateSpace space;
    if (!SDL_GetWindowSize(window, &space.windowWidth, &space.windowHeight)) {
        return false;
    }
    if (VideoMode_IsWindowed()) {
        if (!SDL_GetWindowSizeInPixels(window, &space.renderWidth, &space.renderHeight)) {
            return false;
        }
    } else {
        VideoMode_GetCurrentVideoMode(&space.renderWidth, &space.renderHeight, nullptr);
        space.aspectX = GetXMouseAspectRatioAdjustment();
        space.aspectY = GetYMouseAspectRatioAdjustment();
    }
    if (space.windowWidth <= 0 || space.windowHeight <= 0 || space.renderWidth <= 0 || space.renderHeight <= 0 ||
        !std::isfinite(space.aspectX) || !std::isfinite(space.aspectY) || space.aspectX <= 0.0f || space.aspectY <= 0.0f) {
        return false;
    }

    float windowX = 0.0f;
    float windowY = 0.0f;
    SDL_GetMouseState(&windowX, &windowY);
    if (!std::isfinite(windowX) || !std::isfinite(windowY)) {
        return false;
    }

    static_assert(std::numeric_limits<int>::digits == 31, "Engine mouse coordinates require signed 32-bit int");
    constexpr int minimumPosition = (std::numeric_limits<int>::min)();
    constexpr int maximumPosition = (std::numeric_limits<int>::max)();
    // Floating-to-integer truncation is valid strictly between these bounds,
    // including negative fractions whose truncated value is minimumPosition.
    constexpr double truncationLowerExclusive = static_cast<double>(minimumPosition) - 1.0;
    constexpr double truncationUpperExclusive = static_cast<double>(maximumPosition) + 1.0;
    const auto tryAxis = [=](float position, int windowExtent, int renderExtent, float aspect,
                            int center, int &result) {
        const float scaledValue = position * static_cast<float>(renderExtent) / static_cast<float>(windowExtent);
        if (!std::isfinite(scaledValue) || static_cast<double>(scaledValue) <= truncationLowerExclusive ||
            static_cast<double>(scaledValue) >= truncationUpperExclusive) {
            return false;
        }
        const int scaled = static_cast<int>(scaledValue);
        const std::int64_t scaledDifference = static_cast<std::int64_t>(scaled) - center;
        if (scaledDifference < minimumPosition || scaledDifference > maximumPosition) {
            return false;
        }
        const double adjustedValue = scaled + (static_cast<double>(aspect) - 1.0) * static_cast<int>(scaledDifference);
        if (!std::isfinite(adjustedValue) || adjustedValue <= truncationLowerExclusive ||
            adjustedValue >= truncationUpperExclusive) {
            return false;
        }
        const int adjusted = static_cast<int>(adjustedValue);
        const std::int64_t adjustedDifference = static_cast<std::int64_t>(adjusted) - center;
        if (adjustedDifference < minimumPosition || adjustedDifference > maximumPosition) {
            return false;
        }
        result = adjusted;
        return true;
    };

    const int resultCenterX = space.renderWidth / 2;
    const int resultCenterY = space.renderHeight / 2;
    int resultX = 0;
    int resultY = 0;
    if (!tryAxis(windowX, space.windowWidth, space.renderWidth, space.aspectX, resultCenterX, resultX) ||
        !tryAxis(windowY, space.windowHeight, space.renderHeight, space.aspectY, resultCenterY, resultY)) {
        return false;
    }
    *x = resultX;
    *y = resultY;
    *centerX = resultCenterX;
    *centerY = resultCenterY;
    return true;
}
} // namespace cse
