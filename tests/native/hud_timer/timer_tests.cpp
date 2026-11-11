#include "cse_vgui.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <iostream>
#include <string>
#include <vector>

// External UI boundaries have deterministic glyph metrics and record rendered output.
constexpr int kDigitWidth = 12;
constexpr int kDigitHeight = 16;
constexpr int kLegacyHeight = 20;
constexpr int kColonWidth = 7;
constexpr vgui2::HFont kLegacyFont = 11;
constexpr vgui2::HFont kCreditsFont = 22;
constexpr vgui2::HFont kReinitializedFont = 33;
constexpr int kScaledFontThreshold = 1300;
constexpr int kHdFontThreshold = 1024;
constexpr int kScaledFontHeight = 40;
constexpr int kHdFontHeight = 30;
constexpr int kLowFontHeight = 16;
constexpr int kGlyphLeftBearing = 1;
constexpr int kGlyphRightBearing = 1;
constexpr int kLegacyGlyphBody = 5;
constexpr int kCreditsGlyphBody = 11;
constexpr int kDigitsPerGroup = 2;
constexpr int HIDEHUD_TIMER = 1;
constexpr int HUD_ACTIVE = 1;
constexpr int WEAPON_SUIT = 0;
constexpr int DHN_DRAWZERO = 1;
constexpr int DHN_2DIGITS = 2;
constexpr int DHN_FILLZERO = 4;
int ScreenWidth = 2560;
int ScreenHeight = 1440;
struct Color {
    int r = 0;
    int g = 0;
    int b = 0;
    void SetColor(int red, int green, int blue, int) {
        r = red;
        g = green;
        b = blue;
    }
};
struct Glyph {
    int x;
    int y;
    int ch;
    vgui2::HFont font;
    Color color;
};
struct Number {
    int x;
    int y;
    int flags;
    int value;
    Color color;
};
std::vector<Glyph> glyphs;
std::vector<Number> numbers;
int stopwatchDraws = 0;
namespace vgui2 {
struct IScheme {
    HFont legacy = kLegacyFont;
    HFont credits = kCreditsFont;
    HFont GetFont(const char* name, bool) {
        return std::strcmp(name, "Legacy_CreditsFont") == 0 ? legacy : credits;
    }
    const char* GetResourceString(const char*) {
        return "1";
    }
};
struct ISchemeManager {
    IScheme owned;
    int GetDefaultScheme() {
        return 1;
    }
    IScheme* GetIScheme(int) {
        return &owned;
    }
};
struct Surface {
    HFont font = 0;
    int x = 0;
    int y = 0;
    Color color;
    HFont measured = 0;
    int GetFontTall(HFont handle) {
        measured = handle;
        if (handle != kCreditsFont) {
            return kLegacyHeight;
        }
        if (ScreenHeight >= kScaledFontThreshold) {
            return kScaledFontHeight;
        }
        return ScreenHeight >= kHdFontThreshold ? kHdFontHeight : kLowFontHeight;
    }
    void DrawSetTextFont(HFont value) {
        font = value;
    }
    void DrawSetTextPos(int left, int top) {
        x = left;
        y = top;
    }
    void DrawSetTextColor(Color value) {
        color = value;
    }
    void DrawUnicodeCharAdd(wchar_t ch) {
        glyphs.push_back({x, y, ch, font, color});
    }
    void GetCharABCwide(HFont handle, int, int& a, int& b, int& c) {
        a = kGlyphLeftBearing;
        b = handle == kCreditsFont ? kCreditsGlyphBody : kLegacyGlyphBody;
        c = kGlyphRightBearing;
    }
};
ISchemeManager manager;
Surface ui;
ISchemeManager* scheme() {
    return &manager;
}
Surface* surface() {
    return &ui;
}
} // namespace vgui2
struct BaseSurface {
    void ResetViewPort() {
    }
} g_BaseUISurface;
vgui2::HFont _consoleFont = 0;
vgui2::HFont _creditsFont = 0;
Color _col;
#include "fonts.inc"
const wchar_t* VGUI2_Find_String(const char*) {
    return L":";
}
int VGUI2_Draw_CharacterAdd(int, int, int, int, int, int, unsigned int);
int VGUI2_DrawStringClient(int, int, const char*, int, int, int);
struct EngineFunctions {
    int (*pfnDrawString)(int, int, const char*, int, int, int) = VGUI2_DrawStringClient;
    int (*pfnVGUI2DrawCharacterAdd)(int, int, int, int, int, int, unsigned int) = VGUI2_Draw_CharacterAdd;
} gEngfuncs;
struct wrect_t {
    int left = 0;
    int right = kDigitWidth;
    int top = 0;
    int bottom = kDigitHeight;
};
struct CHud {
    int m_iHideHUDDisplay = 0;
    int m_iWeaponBits = 1;
    int m_iFontHeight = kDigitHeight;
    int m_iFontEngineHeight = 0;
    int m_HUD_number_0 = 0;
    wrect_t rect;
    int GetSpriteIndex(const char*) {
        return 1;
    }
    int GetSprite(int index) {
        return index;
    }
    wrect_t& GetSpriteRect(int) {
        return rect;
    }
    int DrawHudNumber(int x, int y, int flags, int value, int r, int g, int b) {
        numbers.push_back({x, y, flags, value, {r, g, b}});
        return x + kDigitsPerGroup * kDigitWidth;
    }
    int DrawHudString(int, int, int, const char*, int, int, int);
    void MeasureLayoutFont();
} gHUD;
struct CHudMessage {
    int m_HUD_title_half = 0;
    int m_HUD_title_life = 0;
    int VidInit();
};
struct CHudBase {
    int m_iFlags = 0;
    virtual ~CHudBase() = default;
    virtual int Init() {
        return 1;
    }
    virtual int VidInit() {
        return 1;
    }
    virtual int Draw(float) {
        return 1;
    }
    virtual void Reset() {
    }
};
#include "classes.inc"
int CHudRoundTimer::Init() {
    return 1;
}
int CHudCareerTaskTimer::Init() {
    return 1;
}
struct Viewport {
    bool visible = false;
    bool IsSpectatorGUIVisible() {
        return visible;
    }
} viewport;
Viewport* gViewPortInterface = &viewport;
struct CareerUI {
    bool playing = true;
    int length = 1;
    bool IsPlayingMatch() {
        return playing;
    }
    int GetWinfastLength() {
        return length;
    }
} careerUI;
CareerUI* g_pCareerUI = &careerUI;
bool bombPlanted = false;
bool IsBombPlanted() {
    return bombPlanted;
}
void SPR_Set(int, int, int, int) {
}
void SPR_DrawAdditive(int, int, int, wrect_t*) {
    ++stopwatchDraws;
}
void ScaleColors(int&, int&, int&, int);
#include "bodies.inc"
#include "metrics.inc"

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

void clearDraws() {
    glyphs.clear();
    numbers.clear();
    stopwatchDraws = 0;
}

void initializeFonts() {
    CHudMessage message;
    require(message.VidInit() == 1, "message font initialization");
    gHUD.MeasureLayoutFont();
    VGUI2_Draw_Init();
}

template <class Timer> Timer timer() {
    Timer value;
    value.Reset();
    value.VidInit();
    value.m_flNewPeriod = 125;
    return value;
}

template <class Timer> void checkLayout(Timer& value, int expectedY, int expectedX) {
    clearDraws();
    const auto measuredFont = vgui2::ui.measured;
    require(value.Draw(10) == 1, "draw return value preserved");
    require(glyphs.size() == 1 && numbers.size() == 2, "one colon between two digit groups");
    const auto& colon = glyphs.front();
    std::cout << "boundary: layoutFont=" << measuredFont << " colonFont=" << colon.font << " colon=(" << colon.x << ','
              << colon.y << ") nextX=" << numbers[1].x << '\n';
    require(colon.font == measuredFont, "colon font must equal actual layout metric font");
    require(colon.ch == ':', "colon glyph unchanged");
    require(colon.y == expectedY, "colon baseline preserves legacy height centering");
    require(colon.x == expectedX, "colon starts at absolute minute digit end");
    require(numbers[1].x == expectedX + kColonWidth, "callback width advances absolute x exactly once");
    require(numbers[0].value == 2 && numbers[1].value == 5, "125 second countdown preserved");
    require(stopwatchDraws == 1, "stopwatch retained");
    require(numbers[0].flags == (DHN_DRAWZERO | DHN_2DIGITS), "minute digit flags retained");
    require(numbers[1].flags == (DHN_DRAWZERO | DHN_2DIGITS | DHN_FILLZERO), "seconds padded");
}

void fontLayout(bool careerOnly, int height) {
    struct Case {
        int width;
        int height;
        int x;
        int roundY;
        int careerY;
    };
    for (const auto& test :
         {Case{640, 480, 320, 453, 429}, Case{1920, 1080, 960, 1053, 1029}, Case{2560, 1440, 1280, 1413, 1389}}) {
        if (test.height != height) {
            continue;
        }
        ScreenWidth = test.width;
        ScreenHeight = test.height;
        initializeFonts();
        auto round = timer<CHudRoundTimer>();
        auto career = timer<CHudCareerTaskTimer>();
        career.m_bCountdown = true;
        if (careerOnly) {
            checkLayout(career, test.careerY, test.x);
        } else {
            checkLayout(round, test.roundY, test.x);
        }
    }
}

void widthAccounting() {
    initializeFonts();
    auto round = timer<CHudRoundTimer>();
    checkLayout(round, 1413, 1280);
    require(round.m_closestRight == 1329, "closest-right includes colon width once");
    gHUD.m_iWeaponBits = 0;
    round.m_flNewPeriod = 125;
    checkLayout(round, 39, 1280);
}

void hidden() {
    initializeFonts();
    auto round = timer<CHudRoundTimer>();
    auto career = timer<CHudCareerTaskTimer>();
    career.m_bCountdown = true;
    for (int reason = 0; reason < 3; ++reason) {
        gHUD.m_iHideHUDDisplay = reason == 0 ? HIDEHUD_TIMER : 0;
        viewport.visible = reason == 1;
        bombPlanted = reason == 2;
        clearDraws();
        require(round.Draw(10) == 1, "hidden round returns success");
        require(glyphs.empty() && numbers.empty() && stopwatchDraws == 0, "round early exit suppresses draws");
        if (reason != 2) {
            require(career.Draw(10) == 1, "hidden career returns success");
            require(glyphs.empty() && numbers.empty(), "career early exit suppresses draws");
        }
    }
    viewport.visible = false;
    bombPlanted = false;
    for (int reason = 0; reason < 4; ++reason) {
        g_pCareerUI = reason == 0 ? nullptr : &careerUI;
        careerUI.playing = reason != 1;
        careerUI.length = reason == 2 ? 0 : 1;
        career.m_flNewPeriod = reason == 3 ? -1.0f : 125.0f;
        clearDraws();
        require(career.Draw(10) == 1 && glyphs.empty() && numbers.empty(), "career eligibility suppresses draws");
    }
}

void panic() {
    initializeFonts();
    auto round = timer<CHudRoundTimer>();
    round.m_flNewPeriod = 2;
    require(round.Draw(10) == 1, "low time draw");
    require(numbers[1].value == 2 && glyphs[0].color.r == 255 && glyphs[0].color.g == 16, "low-time red retained");
    require(std::fabs(round.m_flNextToggle - 10.05f) < 0.00001f, "two-second panic cadence");
    clearDraws();
    round.Draw(10.06f);
    require(glyphs[0].color.r == 100 && glyphs[0].color.g == 62, "panic alternates scaled orange");
    clearDraws();
    round.Draw(13);
    require(numbers[0].value == 0 && numbers[1].value == 0 && glyphs[0].color.g == 16,
            "expired round clamps zero and stays red");
    auto career = timer<CHudCareerTaskTimer>();
    career.m_bCountdown = true;
    career.m_flNewPeriod = 2;
    clearDraws();
    career.Draw(10);
    require(numbers[1].value == 2 && glyphs[0].color.g == 16, "career countdown panic retained");
    clearDraws();
    career.Draw(13);
    require(numbers[1].value == 0 && glyphs[0].color.g == 16, "expired career clamps zero");
}

void careerFade() {
    initializeFonts();
    auto career = timer<CHudCareerTaskTimer>();
    career.m_flNewPeriod = -125;
    career.m_bTaskComplete = true;
    career.m_flFadeDelay = 2;
    career.Draw(10);
    require(numbers[0].value == 2 && numbers[1].value == 5, "career elapsed timer preserved");
    require(glyphs[0].color.r == 0 && glyphs[0].color.g == 62 && career.m_flFadeStart == 12,
            "completed task green and fade delay preserved");
    clearDraws();
    career.Draw(13.5f);
    require(career.m_flAlpha == 0.5f && glyphs[0].color.g == 30, "career halfway fade preserved");
    clearDraws();
    career.Draw(16);
    require(career.m_flAlpha == 0 && glyphs[0].color.g == 0, "career exhausted fade preserved");
}

void currentFont() {
    initializeFonts();
    auto round = timer<CHudRoundTimer>();
    auto career = timer<CHudCareerTaskTimer>();
    career.m_bCountdown = true;
    checkLayout(round, 1413, 1280);
    vgui2::manager.owned.legacy = kReinitializedFont;
    _font = 0;
    initializeFonts();
    round.m_flNewPeriod = 125;
    checkLayout(round, 1413, 1280);
    checkLayout(career, 1389, 1280);
    clearDraws();
    gHUD.DrawHudString(100, 20, 200, ":", 1, 2, 3);
    require(glyphs[0].font == kCreditsFont, "unrelated generic string retains credits font");
}

int main(int argc, char** argv) {
    require(argc == 2, "scenario argument required");
    const std::string scenario = argv[1];
    if (scenario.find("font-round-") == 0 || scenario.find("font-career-") == 0) {
        fontLayout(scenario.find("career") != std::string::npos, std::stoi(scenario.substr(scenario.rfind('-') + 1)));
    } else if (scenario == "width") {
        widthAccounting();
    } else if (scenario == "hidden") {
        hidden();
    } else if (scenario == "panic") {
        panic();
    } else if (scenario == "career-fade") {
        careerFade();
    } else if (scenario == "current-font") {
        currentFont();
    } else {
        require(false, "unknown scenario");
    }
    std::cout << "PASS " << scenario << '\n';
}
