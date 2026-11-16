#pragma once

#include "hud/radar/render/RadarDraw.h"
#include "hud/radar/integration/Performance.h"

#include <array>
#include <cstdint>

#include <ncl_math/vec3.h>
#include <ncl_math/color.h>
#include <ncl_math/aabb.h>

typedef int TextFont_t;

class RadarText {
  private:
    RadarDrawContext& draw_;
    radar::integration::Performance& performance_;

    static constexpr int kMaxChars = 40;

    struct SizedFont {
        int device_px;
        TextFont_t font;
    };

    // Must precede fonts_, whose type uses this array bound.
    static constexpr int kMaxFonts = 48;

    std::array<SizedFont, kMaxFonts> fonts_{};
    int font_count_{};

  public:
    RadarText(RadarDrawContext& draw, radar::integration::Performance& performance)
        : draw_(draw), performance_(performance) {}
    bool CheckAvailable(int device_px);

    // Zero until the scheme font resolves.
    int ResolveStandardPx() const;

    // pos is the horizontal center and the top of the text.
    void DrawCentered(int slot, const char* utf8, ncl_math::Vector2 pos, int device_px, ncl_math::Color color);

    void ResetGl();

  private:
    TextFont_t ResolveFont(int device_px);
};
