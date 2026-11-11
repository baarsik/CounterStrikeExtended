#pragma once
#include "StandaloneRaster.h"
#include "cse_vgui.h"

namespace cse::vgui {
class StandaloneWindow {
public:
    StandaloneWindow() = default;
    StandaloneWindow(const StandaloneWindow&) = delete;
    StandaloneWindow& operator=(const StandaloneWindow&) = delete;
    ~StandaloneWindow();
    bool Open(vgui2::HPanel panel, std::uint64_t generation, int x, int y, int width, int height, bool utility);
    void Close();
    bool Resize(int width, int height);
    void SynchronizeBounds(int x, int y, int width, int height);
    void AcceptNativePosition(int& x, int& y);
    int TakeWheel(float delta, vgui2::HPanel target);
    void ResetWheel();
    SDL_Window* Window() const {
        return window_;
    }
    SDL_WindowID Id() const {
        return id_;
    }
    vgui2::HPanel Panel() const {
        return panel_;
    }
    std::uint64_t Generation() const {
        return generation_;
    }
    Uint64 Created() const {
        return created_;
    }
    StandaloneRaster raster;
    bool disabled = false;
    bool dirty = true;
    bool modal = false;

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_WindowID id_ = 0;
    vgui2::HPanel panel_ = 0;
    std::uint64_t generation_ = 0;
    Uint64 created_ = 0;
    int x_ = 0;
    int y_ = 0;
    int width_ = 0;
    int height_ = 0;
    float wheelRemainder_ = 0.0f;
    vgui2::HPanel wheelTarget_ = 0;
};
} // namespace cse::vgui
