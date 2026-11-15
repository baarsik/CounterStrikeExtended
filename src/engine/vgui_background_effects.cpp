#include "vgui_background_effects.h"
#include "vgui_background_blur_gl.h"
#include "../public/vgui_background_effects.h"
#include "../public/vgui_style_service.h"
#include "../public/vgui_style.h"
#include "cvar.h"
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

int CSE_ABI_CALL CurrentStyle() {
    return Cvar_VariableValue(VGuiStyleCvar) > 0.0f ? SmokedTacticalGlassVGuiStyle : OriginalVGuiStyle;
}

bool CSE_ABI_CALL DrawSurface(const BackdropRect* bounds, const BackdropRect* clip,
    const BackdropStyle* style, float radius) {
    if (!bounds || !clip || !style || !render::LegacyIndexedDrawingAllowed()) {
        return false;
    }
    int width = 0;
    int height = 0;
    g_BaseUISurface.GetScreenSize(width, height);
    g_BaseUISurface.DrawFlushText();
    return backgroundBlur.Draw(*bounds, *clip, width, height, *style, radius);
}

StyleApi styleApi{sizeof(StyleApi), StyleApiVersion, CurrentStyle, DrawSurface};

void* CreateStyle() {
    return &styleApi;
}
EXPOSE_INTERFACE_FN(CreateStyle, StyleApi, StyleInterface);
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
