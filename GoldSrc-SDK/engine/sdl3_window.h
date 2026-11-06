#pragma once

#include "../common/sdl3_input.h"
#include "ivideomode.h"
#include "vid.h"

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
} // namespace cse
