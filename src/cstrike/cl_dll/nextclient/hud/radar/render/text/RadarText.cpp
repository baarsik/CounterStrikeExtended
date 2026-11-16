#include "RadarText.h"

#include <algorithm>
#include <array>
#include <cwchar>
#include <vgui_controls/Controls.h>
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include "legacy_text_encoding.h"
#include "NativeSdk.h"
#include "radar_engine_api.h"

namespace {
constexpr int kFontWeight = 500;
constexpr int kMinimumFontPixels = 8;
constexpr int kMaximumFontPixels = 128;
constexpr std::size_t kLabelCells = 128;
} // namespace

TextFont_t RadarText::ResolveFont(int device_px) {
    if (!vgui2::surface() || device_px < kMinimumFontPixels || device_px > kMaximumFontPixels) {
        return 0;
    }
    for (int i = 0; i < font_count_; ++i) {
        if (fonts_[i].device_px == device_px) {
            return fonts_[i].font;
        }
    }
    if (font_count_ == kMaxFonts) {
        return 0;
    }
    const vgui2::HFont font = vgui2::surface()->CreateFont();
    if (!vgui2::surface()->AddGlyphSetToFont(font, "Verdana", device_px, kFontWeight, 0, 0,
                                             vgui2::ISurface::FONTFLAG_ANTIALIAS, 0, 0)) {
        return 0;
    }
    fonts_[font_count_++] = {device_px, static_cast<TextFont_t>(font)};
    return static_cast<TextFont_t>(font);
}

bool RadarText::CheckAvailable(int device_px) {
    return draw_.CheckPrimsAvailable() && ResolveFont(device_px) != 0;
}

int RadarText::ResolveStandardPx() const {
    if (!vgui2::scheme() || !vgui2::surface()) {
        return 0;
    }
    vgui2::IScheme* scheme = vgui2::scheme()->GetIScheme(vgui2::scheme()->GetDefaultScheme());
    const vgui2::HFont font = scheme ? scheme->GetFont("EngineFont", false) : 0;
    return font ? vgui2::surface()->GetFontTall(font) : 0;
}

void RadarText::DrawCentered(int, const char* utf8, ncl_math::Vector2 pos, int device_px, ncl_math::Color color) {
    const TextFont_t font = ResolveFont(device_px);
    if (!font || !utf8) {
        return;
    }
    std::array<wchar_t, kLabelCells> text{};
    cse::legacy_text::Utf8ToWideCells(utf8, text.data(), static_cast<int>(sizeof(text)));
    std::array<cse::radar::Glyph, kMaxChars> glyphs{};
    std::array<CseRadarTexture, kMaxChars> atlases{};
    const auto* renderer = NativeRadar_Renderer();
    int count = 0;
    int width = 0;
    // Atlas baking can bind a texture. Finish queued primitives before querying
    // all glyphs, then submit the complete label through the clipped radar shader.
    draw_.FlushPrims();
    for (wchar_t character : text) {
        if (!character || count == kMaxChars) {
            break;
        }
        cse::radar::Glyph& glyph = glyphs[count];
        if (renderer) {
            CseRadarGlyph resolved{};
            const auto result = renderer->ResolveGlyph(NativeRadar_RendererState().rendererEpoch,
                static_cast<std::uint32_t>(font), static_cast<std::uint32_t>(character), &resolved, sizeof(resolved));
            if (result != CSE_RADAR_OK) {
                NativeRadar_ReportRenderResult("glyph", result);
                continue;
            }
            atlases[count] = resolved.atlas;
            glyph.bearing_x = resolved.bearingX;
            glyph.wide = resolved.width;
            glyph.tall = resolved.height;
            glyph.advance = resolved.advance;
            std::copy(resolved.uv, resolved.uv + 4, glyph.uv);
        } else if (!NativeRadar_GetGlyph(static_cast<std::uint32_t>(font), static_cast<std::uint32_t>(character), glyph)) {
            continue;
        }
        width += glyph.advance;
        ++count;
    }
    float pen = pos.x - width * 0.5f;
    for (int i = 0; i < count; ++i) {
        const cse::radar::Glyph& glyph = glyphs[i];
        const float left = pen + glyph.bearing_x;
        const ncl_math::Aabb2 rectangle{{left, pos.y}, {left + glyph.wide, pos.y + glyph.tall}};
        const ncl_math::Aabb2 uv{{glyph.uv[0], glyph.uv[1]}, {glyph.uv[2], glyph.uv[3]}};
        if (glyph.wide > 0 && renderer && atlases[i].serial) {
            draw_.DrawGlyph(atlases[i], rectangle, uv, color);
        } else if (glyph.wide > 0 && glyph.texture != 0) {
            draw_.DrawGlyph(glyph.texture, rectangle, uv, color);
        }
        pen += glyph.advance;
    }
}

void RadarText::ResetGl() {
    // VGUI owns fonts and its atlas. Glyphs are looked up for each draw so a
    // language change or context rebuild cannot reuse stale cached GL names.
}
