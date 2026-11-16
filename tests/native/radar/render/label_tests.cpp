#include "Test.h"

#include <cstring>
#include <string>
#include <vector>

#include "NativeSdk.h"
#include "hud/radar/render/RadarDraw.h"
#include "hud/radar/render/text/HudTextBuffer.h"
#include "hud/radar/render/text/RadarLabel.h"

namespace {
struct DrawnLabel {
    int x;
    int y;
    std::string text;
};

std::vector<DrawnLabel> g_Labels;
HudTextBuffer g_Buffer;

int DrawConsoleString(int x, int y, char* text) {
    if (y >= 0) {
        g_Labels.push_back({x, y, text});
    }

    constexpr int kGlyphWidth = 8;
    return x + static_cast<int>(std::strlen(text)) * kGlyphWidth;
}

void SetColor(int, int, int) {}
void SetTextColor(float, float, float) {}

class RadarLabelTest {
  protected:
    cl_enginefunc_t saved_engine_{};
    radar::integration::Performance performance_;
    RadarDrawContext draw_{performance_};

    void SetUp() {
        saved_engine_ = gEngfuncs;
        cl_enginefunc_t engine{};
        engine.pfnDrawConsoleString = DrawConsoleString;
        g_Buffer.Initialize(engine, SetColor);
        g_Labels.clear();
        gEngfuncs.pfnDrawSetTextColor = SetTextColor;
        gEngfuncs.pfnDrawConsoleString = [](int x, int y, char* text) {
            return g_Buffer.DrawConsoleString(x, y, text);
        };
        HudTextZone_SetPassthrough(false);
        HudTextZone_BeginFrame(640, 480);
        HudTextZone_SetZone(20, 20, 80);
    }

  public:
    ~RadarLabelTest() {
        g_Buffer.Reset();
        HudTextZone_ClearZone();
        HudTextZone_SetPassthrough(false);
        gEngfuncs = saved_engine_;
    }
};
} // namespace

RADAR_TEST_F(RadarLabelTest, FallbackLabelKeepsMarkerPositionWhileOtherHudTextMoves) {
    RadarLabel_Draw(draw_, "A", 40, 40, {255, 255, 255}, 0);
    CHECK_EQ(g_Labels.size(), 1u);
    CHECK_EQ(g_Labels[0].x, 40);
    CHECK_EQ(g_Labels[0].y, 40);
    CHECK_EQ(g_Labels[0].text, "A");

    char server_text[] = "HUD";
    gEngfuncs.pfnDrawConsoleString(40, 60, server_text);
    g_Buffer.Flush();
    CHECK_EQ(g_Labels.size(), 2u);
    CHECK_EQ(g_Labels[1].x, 112);
    CHECK_EQ(g_Labels[1].y, 60);
}

RADAR_TEST_F(RadarLabelTest, FallbackLabelPreservesAnExistingPassthrough) {
    HudTextZone_SetPassthrough(true);
    RadarLabel_Draw(draw_, "Player", 40, 40, {255, 255, 255}, 0);
    CHECK_EQ(g_Labels.size(), 1u);
    CHECK_TRUE(!HudTextZone_IsActive());

    char other_label[] = "B";
    gEngfuncs.pfnDrawConsoleString(50, 60, other_label);
    g_Buffer.Flush();
    CHECK_EQ(g_Labels.size(), 2u);
    CHECK_EQ(g_Labels[1].x, 50);
    CHECK_EQ(g_Labels[1].y, 60);
}
