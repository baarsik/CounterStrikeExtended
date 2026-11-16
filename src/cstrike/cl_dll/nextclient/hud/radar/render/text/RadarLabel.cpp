#include "RadarLabel.h"

#include "main.h"

#include <ncl_math/scalar.h>
#include "NativeSdk.h"

#include "hud/radar/render/RadarDraw.h"
#include "hud/radar/render/text/HudTextZone.h"

namespace {
// kFlashFadeStart stays below kLabelFlashHide so the ramp is already under way where
// the unbatched path cuts.
constexpr float kFlashFadeStart = 90.0f;
constexpr float kFlashFadeEnd = 200.0f;

class ScopedTextPassthrough {
  private:
    bool previous_ = HudTextZone_SetPassthrough(true);

  public:
    ScopedTextPassthrough() = default;
    ScopedTextPassthrough(const ScopedTextPassthrough&) = delete;
    ScopedTextPassthrough& operator=(const ScopedTextPassthrough&) = delete;

    ~ScopedTextPassthrough() {
        HudTextZone_SetPassthrough(previous_);
    }
};
} // namespace

float RadarLabel_ComputeFlashFade(int flash_alpha) {
    return 1.0f - ncl_math::SmoothStep(kFlashFadeStart, kFlashFadeEnd, (float)flash_alpha);
}

void RadarLabel_Draw(RadarDrawContext& draw, const char* text, int x, int y, ncl_math::Color color, int flash_alpha) {
    // A hard cut, not a ramp: this font's drop shadow is baked black into the glyph and
    // cannot be faded. Not the additive shadow-free engine font instead, which is a much
    // larger, non-scaling face.
    if (flash_alpha >= kLabelFlashHide) {
        return;
    }

    ScopedTextPassthrough passthrough;
    ncl_math::ColorF faded = draw.ApplyFade(color).ToFloat();
    gEngfuncs.pfnDrawSetTextColor(faded.r * faded.a, faded.g * faded.a, faded.b * faded.a);
    gEngfuncs.pfnDrawConsoleString(x, y, const_cast<char*>(text));
}
