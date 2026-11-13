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
    bool BackingResizePending() const;
    bool HasSizeIntent(int width, int height) const;
    bool SizeRequestInFlight() const;
    bool MatchesSizeRequest(int width, int height) const;
    void BeginSizeRequest(int width, int height);
    void EndSizeRequest(bool completed);
    void AcceptRetainedSize(int width, int height);
    void SynchronizePosition(int x, int y);
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
    // Retained comparison baseline is independent of the raster's allocation and native completion.
    int retainedWidth_ = 0;
    int retainedHeight_ = 0;
    int requestedWidth_ = 0;
    int requestedHeight_ = 0;
    enum class SizeRequestResult { None, Pending, Accepted, Rejected };
    SizeRequestResult sizeRequestResult_ = SizeRequestResult::None;
    bool backingResizePending_ = false;
    float wheelRemainder_ = 0.0f;
    vgui2::HPanel wheelTarget_ = 0;
};
} // namespace cse::vgui
