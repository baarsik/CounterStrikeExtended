#include "StandaloneWindow.h"
#include <cmath>
#include <limits>

namespace cse::vgui {
StandaloneWindow::~StandaloneWindow() {
    Close();
}

bool StandaloneWindow::Open(vgui2::HPanel panel, std::uint64_t generation, int x, int y, int width, int height,
                            bool utility) {
    Close();
    if (!SDL_IsMainThread() || width <= 0 || height <= 0 || width > MaximumRasterDimension ||
        height > MaximumRasterDimension) {
        return false;
    }
    SDL_WindowFlags flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_BORDERLESS;
    if (utility) {
        flags |= SDL_WINDOW_UTILITY;
    }
    window_ = SDL_CreateWindow("CounterStrikeExtended", width, height, flags);
    if (!window_) {
        return false;
    }
    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (!renderer_ || !raster.Open(renderer_, generation) || !Resize(width, height)) {
        Close();
        return false;
    }
    SDL_SetWindowPosition(window_, x, y);
    x_ = x;
    y_ = y;
    if (!SDL_StartTextInput(window_)) {
        Close();
        return false;
    }
    id_ = SDL_GetWindowID(window_);
    panel_ = panel;
    generation_ = generation;
    created_ = SDL_GetTicksNS();
    return true;
}

void StandaloneWindow::Close() {
    ResetWheel();
    // SDL recursively destroys native child windows. Their renderers/textures
    // belong to separate StandaloneWindow owners, so detach before retiring us.
    if (window_) {
        int count = 0;
        SDL_Window** windows = SDL_GetWindows(&count);
        for (int index = 0; index < count; ++index) {
            SDL_Window* child = windows[index];
            if (SDL_GetWindowParent(child) == window_) {
                SDL_SetWindowModal(child, false);
                SDL_SetWindowParent(child, nullptr);
                SDL_HideWindow(child);
            }
        }
        SDL_free(windows);
    }
    // Renderer textures must die before their renderer, which must die before its window.
    raster.Close();
    SDL_DestroyRenderer(renderer_);
    renderer_ = nullptr;
    if (window_) {
        SDL_StopTextInput(window_);
    }
    SDL_DestroyWindow(window_);
    window_ = nullptr;
    id_ = 0;
    panel_ = 0;
}

bool StandaloneWindow::Resize(int width, int height) {
    const float scale = SDL_GetWindowPixelDensity(window_);
    if (!raster.Resize(width, height, scale > 0.0f ? scale : 1.0f)) {
        return false;
    }
    width_ = width;
    height_ = height;
    return true;
}

void StandaloneWindow::SynchronizeBounds(int x, int y, int width, int height) {
    // Compare retained requests with the last synchronized request, not live OS
    // bounds: a pending user move/resize must survive until its event is handled.
    if (width > 0 && height > 0 && (width != width_ || height != height_)) {
        if (SDL_SetWindowSize(window_, width, height)) {
            Resize(width, height);
        }
    }
    if (x != x_ || y != y_) {
        if (SDL_SetWindowPosition(window_, x, y)) {
            x_ = x;
            y_ = y;
        }
    }
}

void StandaloneWindow::AcceptNativePosition(int& x, int& y) {
    // The queue may still contain an older move from popup creation. SDL's live
    // native bounds represent the latest accepted OS position.
    SDL_GetWindowPosition(window_, &x, &y);
    x_ = x;
    y_ = y;
}
int StandaloneWindow::TakeWheel(float delta, vgui2::HPanel target) {
    if (target != wheelTarget_) {
        ResetWheel();
        wheelTarget_ = target;
    }
    const double amount = static_cast<double>(wheelRemainder_) + delta;
    if (!std::isfinite(amount) || amount > std::numeric_limits<int>::max() ||
        amount < std::numeric_limits<int>::min()) {
        ResetWheel();
        return 0;
    }
    const int whole = static_cast<int>(amount);
    wheelRemainder_ = static_cast<float>(amount - whole);
    return whole;
}

void StandaloneWindow::ResetWheel() {
    wheelRemainder_ = 0.0f;
    wheelTarget_ = 0;
}
} // namespace cse::vgui
