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
    AcceptRetainedSize(width, height);
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
    retainedWidth_ = 0;
    retainedHeight_ = 0;
    requestedWidth_ = 0;
    requestedHeight_ = 0;
    sizeRequestResult_ = SizeRequestResult::None;
    backingResizePending_ = false;
}

bool StandaloneWindow::Resize(int width, int height) {
    const float scale = SDL_GetWindowPixelDensity(window_);
    const bool resized = raster.Resize(width, height, scale > 0.0f ? scale : 1.0f);
    backingResizePending_ = !resized;
    return resized;
}

bool StandaloneWindow::BackingResizePending() const {
    return backingResizePending_;
}

bool StandaloneWindow::HasSizeIntent(int width, int height) const {
    return width > 0 && height > 0 && (width != retainedWidth_ || height != retainedHeight_);
}

bool StandaloneWindow::SizeRequestInFlight() const {
    return sizeRequestResult_ == SizeRequestResult::Pending;
}

bool StandaloneWindow::MatchesSizeRequest(int width, int height) const {
    return width == requestedWidth_ && height == requestedHeight_;
}

void StandaloneWindow::BeginSizeRequest(int width, int height) {
    retainedWidth_ = width;
    retainedHeight_ = height;
    requestedWidth_ = width;
    requestedHeight_ = height;
    sizeRequestResult_ = SizeRequestResult::Pending;
}

void StandaloneWindow::EndSizeRequest(bool completed) {
    sizeRequestResult_ = completed ? SizeRequestResult::Accepted : SizeRequestResult::Rejected;
}

void StandaloneWindow::AcceptRetainedSize(int width, int height) {
    retainedWidth_ = width;
    retainedHeight_ = height;
}

void StandaloneWindow::SynchronizePosition(int x, int y) {
    // Preserve pending OS moves when the retained position has not changed.
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
