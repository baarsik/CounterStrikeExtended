#include "vgui_smoked_glass.h"
#include "vgui_style_service.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

int CSE_ABI_CALL Style() { return cse::ui::SmokedTacticalGlassVGuiStyle; }
bool CSE_ABI_CALL Draw(const cse::ui::BackdropRect*, const cse::ui::BackdropRect*,
    const cse::ui::BackdropStyle*, float) { return true; }
cse::ui::StyleApi api{sizeof(api), cse::ui::StyleApiVersion, Style, Draw};
void* Factory(const char* name, int*) {
    return std::strcmp(name, cse::ui::StyleInterface) == 0 ? &api : nullptr;
}

int main() {
    using namespace cse::ui;
    static_assert(OriginalVGuiStyle == 0 && SmokedTacticalGlassVGuiStyle == 1, "Saved values stay compatible");
    assert(QueryStyle(Factory) == &api && !QueryStyle(nullptr));
    api.size = 0;
    assert(!QueryStyle(Factory));
    api.size = sizeof(api);
    assert(RoundedInset(100, 60, 8, 0.5f) > 4.0f);
    assert(RoundedInset(100, 60, 8, 30) == 0.0f);
    assert(RoundedInset(100, 60, 8, 0.5f) == RoundedInset(100, 60, 8, 59.5f));
    assert(RoundedInset(4, 4, 100, 2) == 0.0f);
    assert(RoundedInset(100, 60, std::numeric_limits<float>::quiet_NaN(), 1) == 0.0f);
    assert(RoundedInset(0, 0, 8, 0) == 0.0f);
    Rgba result{};
    assert(SmokedSchemeColor("Label.TextColor", {255, 180, 0, 255}, result));
    assert(result.r == Text.r && result.g == Text.g && result.b == Text.b);
    assert(!SmokedSchemeColor("TeamTerrorist", {255, 0, 0, 255}, result));
    assert(SmokedSchemeColor("BgColor", {0, 0, 0, 0}, result) && result.a == 0);
    assert(LegacyAlpha(255) == 0 && LegacyAlpha(0) == 255);
    for (int width : {320, 640, 1440, 3840}) {
        const auto rail = SpectatorRail(width, 144, 24);
        for (const auto& rect : {rail.surface, rail.settings, rail.player, rail.health, rail.camera,
                 rail.previous, rail.next}) {
            assert(rect.x >= 0 && rect.y >= 0 && rect.width >= 0 && rect.height >= 0);
            assert(rect.x + rect.width <= width && rect.y + rect.height <= 144);
        }
        assert(rail.settings.x + rail.settings.width <= rail.previous.x);
        assert(rail.player.x + rail.player.width <= rail.health.x);
        assert(rail.next.x + rail.next.width <= rail.camera.x);
    }
    constexpr int RasterWidth = 100;
    constexpr int RasterHeight = 60;
    std::vector<int> coverage(RasterWidth * RasterHeight);
    int rectangles = 0;
    VisitRoundedFill(RasterWidth, RasterHeight, 8.0f, [&](int left, int top, int right, int bottom) {
        ++rectangles;
        for (int y = top; y < bottom; ++y) {
            for (int x = left; x < right; ++x) {
                ++coverage[y * RasterWidth + x];
            }
        }
    });
    for (int y = 0; y < RasterHeight; ++y) {
        const int inset = static_cast<int>(std::ceil(RoundedInset(RasterWidth, RasterHeight, 8, y + 0.5f)));
        for (int x = 0; x < RasterWidth; ++x) {
            assert(coverage[y * RasterWidth + x] == (x >= inset && x < RasterWidth - inset ? 1 : 0));
        }
    }
    assert(rectangles < 20);
    rectangles = 0;
    VisitRoundedOutline(400, 800, 8.0f, [&](int, int, int, int) { ++rectangles; });
    assert(rectangles < 40); // Large windows must not submit two immediate-mode draws per scanline.
}
