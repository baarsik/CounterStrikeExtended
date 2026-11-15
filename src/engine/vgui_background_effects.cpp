#include "vgui_background_effects.h"
#include "vgui_background_blur_gl.h"
#include "../public/vgui_background_effects.h"
#include "vgui2/BaseUISurface.h"
#include "../vgui2/controls/controls.h"
#include "vgui/IPanel.h"
#include "render/native/legacy_dispatch.h"

extern BaseUISurface g_BaseUISurface;

namespace cse {
namespace ui {
namespace {
BackgroundBlur backgroundBlur;

bool CSE_ABI_CALL DrawBackground(std::uintptr_t handle, const BackdropStyle* style) {
    if (!handle || !style || !vgui2::ipanel() || !render::LegacyIndexedDrawingAllowed()) {
        return false;
    }
    const auto panel = static_cast<vgui2::VPANEL>(handle);
    BackdropRect bounds{};
    vgui2::ipanel()->GetAbsPos(panel, bounds.x, bounds.y);
    vgui2::ipanel()->GetSize(panel, bounds.width, bounds.height);
    int right = 0;
    int bottom = 0;
    BackdropRect clip{};
    vgui2::ipanel()->GetClipRect(panel, clip.x, clip.y, right, bottom);
    clip.width = right - clip.x;
    clip.height = bottom - clip.y;
    int screenWidth = 0;
    int screenHeight = 0;
    g_BaseUISurface.GetScreenSize(screenWidth, screenHeight);
    g_BaseUISurface.DrawFlushText();
    return backgroundBlur.Draw(bounds, clip, screenWidth, screenHeight, *style);
}

BackgroundEffectsApi api{sizeof(BackgroundEffectsApi), BackgroundEffectsVersion, DrawBackground};

void* CreateBackgroundEffects() {
    return &api;
}
EXPOSE_INTERFACE_FN(CreateBackgroundEffects, BackgroundEffectsApi, BackgroundEffectsInterface);
} // namespace

void BeginBackgroundEffectsFrame() {
    backgroundBlur.BeginFrame();
}

void CaptureBackgroundEffects(int x, int y, int width, int height) {
    if (render::LegacyIndexedDrawingAllowed()) {
        backgroundBlur.Capture(x, y, width, height);
    }
}

void ShutdownBackgroundEffects() {
    backgroundBlur.Shutdown();
}

} // namespace ui
} // namespace cse
