#pragma once

#include "../public/vgui_backdrop.h"
#include <memory>

namespace cse {
namespace ui {

// Context-owned, shared scene blur. Capture is called before HUD/VGUI foreground painting.
// Draw requests capture for the next frame; the first visible frame uses the caller's fallback.
class BackgroundBlur {
public:
    BackgroundBlur();
    ~BackgroundBlur();
    BackgroundBlur(const BackgroundBlur&) = delete;
    BackgroundBlur& operator=(const BackgroundBlur&) = delete;

    void BeginFrame();
    bool Capture(int x, int y, int width, int height);
    bool Draw(const BackdropRect& panel, const BackdropRect& clip, int screenWidth, int screenHeight,
        const BackdropStyle& style);
    void Shutdown(); // Must run while the owning context is current.

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ui
} // namespace cse
