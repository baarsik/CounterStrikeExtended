#pragma once

#include "../../../../public/vgui_smoked_glass.h"
#include "../../../../public/vgui_style_service.h"
#include "VGUI_Panel.h"
#include "VGUI_Color.h"

namespace vgui { class Font; }

namespace cse_vgui1 {
namespace smoked {

const cse::ui::StyleApi* Style();
inline bool IsTextFont(vgui::Scheme::SchemeFont role) {
    return role == vgui::Scheme::sf_primary1 || role == vgui::Scheme::sf_primary3;
}

vgui::Font* TextFont(vgui::Font* original, vgui::Scheme::SchemeFont role);

inline bool Enabled() {
    const auto* style = Style();
    return style && style->CurrentStyle() == cse::ui::SmokedTacticalGlassVGuiStyle;
}

inline cse::ui::Rgba TextColor(vgui::Color& original) {
    int red = 0;
    int green = 0;
    int blue = 0;
    int alpha = 0;
    original.getColor(red, green, blue, alpha);
    vgui::Scheme::SchemeColor source = vgui::Scheme::sc_user;
    original.getColor(source);
    if (Enabled() && source != vgui::Scheme::sc_user) {
        return {cse::ui::Text.r, cse::ui::Text.g, cse::ui::Text.b, alpha};
    }
    return {red, green, blue, alpha};
}

template<typename Paint>
void Fill(int width, int height, float radius, cse::ui::Rgba color, Paint paint) {
    cse::ui::VisitRoundedFill(width, height, radius, [&](int x0, int y0, int x1, int y1) {
        paint(color, x0, y0, x1, y1);
    });
}

template<typename Paint>
void Background(vgui::Panel* panel, bool material, cse::ui::Rgba color, Paint paint) {
    int width = 0;
    int height = 0;
    panel->getPaintSize(width, height);
    vgui::Panel* root = panel;
    while (root->getParent()) {
        root = root->getParent();
    }
    int rootX = 0;
    int rootY = 0;
    int rootRight = 0;
    int rootBottom = 0;
    root->getAbsExtents(rootX, rootY, rootRight, rootBottom);
    constexpr float ReferenceHeight = 1080.0f;
    constexpr float MinimumScale = 0.75f;
    constexpr float MaximumScale = 1.0f;
    const float radius = (material ? cse::ui::SurfaceRadius : cse::ui::ControlRadius) *
        (std::clamp)((rootBottom - rootY) / ReferenceHeight, MinimumScale, MaximumScale);
    bool drawn = false;
    if (material) {
        cse::ui::BackdropRect bounds{};
        int right = 0;
        int bottom = 0;
        panel->getAbsExtents(bounds.x, bounds.y, right, bottom);
        int leftInset = 0;
        int topInset = 0;
        int rightInset = 0;
        int bottomInset = 0;
        panel->getInset(leftInset, topInset, rightInset, bottomInset);
        bounds.x += leftInset - rootX;
        bounds.y += topInset - rootY;
        bounds.width = width;
        bounds.height = height;
        cse::ui::BackdropRect clip{};
        panel->getClipRect(clip.x, clip.y, right, bottom);
        clip.width = right - clip.x;
        clip.height = bottom - clip.y;
        clip.x -= rootX;
        clip.y -= rootY;
        drawn = Style()->DrawSurface(&bounds, &clip, &cse::ui::SmokedBackdrop, radius);
    }
    if (!drawn) {
        Fill(width, height, radius, material ? cse::ui::SurfaceFallback : color, paint);
    }
    if (material) {
        Fill(width, height, radius, cse::ui::Glaze, paint);
    }
    if (material || panel->hasFocus()) {
        const auto edge = panel->hasFocus() ? cse::ui::Accent : cse::ui::Edge;
        cse::ui::VisitRoundedOutline(width, height, radius, [&](int x0, int y0, int x1, int y1) {
            paint(edge, x0, y0, x1, y1);
        });
    }
}

} // namespace smoked
} // namespace cse_vgui1
