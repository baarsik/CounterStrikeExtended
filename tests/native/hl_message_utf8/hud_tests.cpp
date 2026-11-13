#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <string>
#include <vector>
#include "wrect.h"
#include "cl_dll.h"
#include "FileSystem.h"
#include "../../../src/vgui2/src/public/vgui/IVGui.h"
#include "../../../src/engine/vgui/ISurface.h"
#include "../../../src/engine/vgui/IScheme.h"
#include "../../../src/engine/IKeyValues.h"
extern "C" int FixtureKeyValuesBytes();
#include "legacy_message_text.h"

extern "C" void CseSteamModuleInitialize() {}
constexpr int maxHUDMessages = 16;
constexpr int ScreenWidth = 640;
constexpr int ScreenHeight = 480;
constexpr int HUD_ACTIVE = 1;
#define ARRAYSIZE(array) (sizeof(array) / sizeof((array)[0]))
cl_enginefunc_t gEngfuncs{};
struct Hud {
    SCREENINFO m_scrinfo{};
    float m_flTime = 0;
    wrect_t rect{};
    const wrect_t& GetSpriteRect(int) {
        return rect;
    }
    int GetSprite(int) {
        return 0;
    }
} gHUD;
struct message_parms_t {
    client_textmessage_t* pMessage{};
    float time{};
    int x{};
    int y{};
    int totalWidth{};
    int totalHeight{};
    int width{};
    int lines{};
    int lineLength{};
    int length{};
    int r{};
    int g{};
    int b{};
    int text{};
    int fadeBlend{};
    float charTime{};
    float fadeTime{};
};
class CHudMessage {
public:
    client_textmessage_t* m_pMessages[maxHUDMessages]{};
    float m_startTime[maxHUDMessages]{};
    message_parms_t m_parms{};
    float m_gameTitleTime{};
    client_textmessage_t* m_pGameTitle{};
    int m_HUD_title_half{};
    int m_HUD_title_life{};
    bool m_bEndAfterMessage{};
    int m_iFlags = HUD_ACTIVE;
    float FadeBlend(float, float, float, float);
    int XPosition(float, int, int);
    int YPosition(float, int);
    void MessageScanNextChar();
    void MessageScanStart();
    void MessageDrawScan(client_textmessage_t*, float);
    int Draw(float);
};
struct Glyph {
    int x;
    int y;
    int character;
    int red;
    int green;
    int blue;
};
std::vector<Glyph> glyphs;
int TextMessageDrawChar(int x, int y, int character, int red, int green, int blue) {
    glyphs.push_back({x, y, character, red, green, blue});
    return 0;
}
void SPR_Set(int, float, float, float) {}
void SPR_DrawAdditive(int, int, int, const wrect_t*) {}
int FixtureRandom() {
    return 0;
}
#define rand FixtureRandom
#include "message.inc"
#undef rand

void Require(bool result, const char* message) {
    if (!result) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
vgui2::ISurface* fontSurface{};
vgui2::HFont creditsFont{};
int Width(int character) {
    int a = 0;
    int b = 0;
    int c = 0;
    fontSurface->GetCharABCwide(creditsFont, character, a, b, c);
    return a + b + c;
}
int ScreenInfo(SCREENINFO* info) {
    info->iWidth = ScreenWidth;
    info->iHeight = ScreenHeight;
    return 1;
}
client_textmessage_t Message(const std::string& text, int effect = 0) {
    client_textmessage_t message{};
    message.pMessage = text.c_str();
    message.x = 0;
    message.y = 0;
    message.effect = effect;
    message.r1 = message.g1 = message.b1 = 240;
    message.r2 = message.g2 = message.b2 = 160;
    message.fadein = 0.1f;
    message.fadeout = 0.5f;
    message.holdtime = 2;
    message.fxtime = 0.25f;
    return message;
}
void CheckGlyphs(const std::string& text, const std::vector<int>& expected, int effect) {
    CHudMessage hud;
    auto message = Message(text, effect);
    glyphs.clear();
    hud.MessageDrawScan(&message, 1);
    std::vector<int> normal;
    for (const auto& glyph : glyphs) {
        if (glyph.red != message.r2) {
            normal.push_back(glyph.character);
        }
    }
    Require(normal == expected, "HUD must emit complete Unicode glyphs, never UTF8 byte fragments");
    Require(hud.m_parms.length == static_cast<int>(expected.size()), "effect length counts visible characters");
    int expectedWidth = 0;
    for (int character : expected) {
        expectedWidth += Width(character);
    }
    Require(hud.m_parms.totalWidth == expectedWidth, "HUD width must use actual engine CreditsFont ABC metrics");
    Require(hud.m_parms.totalHeight == fontSurface->GetFontTall(creditsFont), "HUD height uses selected CreditsFont");
}
int main(int argc, char** argv) {
    Require(argc == 4, "usage: hud_tests runtime resources case");
    std::string runtime = argv[1];
    void* engine = dlopen((runtime + "/hw.dylib").c_str(), RTLD_NOW | RTLD_LOCAL);
    void* gui = dlopen((runtime + "/vgui2.dylib").c_str(), RTLD_NOW | RTLD_LOCAL);
    void* files = dlopen((runtime + "/filesystem_stdio.dylib").c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!engine || !gui || !files) {
        std::fprintf(stderr, "%s\n", dlerror());
    }
    Require(engine && gui && files, "load retained source-built providers");
    auto engineFactory = reinterpret_cast<CreateInterfaceFn>(dlsym(engine, "CreateInterface"));
    auto guiFactory = reinterpret_cast<CreateInterfaceFn>(dlsym(gui, "CreateInterface"));
    auto fileFactory = reinterpret_cast<CreateInterfaceFn>(dlsym(files, "CreateInterface"));
    auto* engineFunctions = static_cast<cl_enginefunc_t*>(dlsym(engine, "gEngfuncs"));
    Require(engineFunctions != nullptr, "actual engine callback table");
    engineFunctions->pfnGetScreenInfo = ScreenInfo;
    gEngfuncs.pfnDrawCharacter = reinterpret_cast<pfnEngSrc_pfnDrawCharacter_t>(
        dlsym(engine, "_Z24TextMessageDrawCharacteriiiiii"));
    Require(gEngfuncs.pfnDrawCharacter != nullptr, "actual draw callback owner");
    auto* filesystem = static_cast<IFileSystem*>(fileFactory("VFileSystem009", nullptr));
    filesystem->Mount();
    filesystem->AddSearchPathNoWrite(argv[2], "GAME");
    auto* vgui = static_cast<vgui2::IVGui*>(guiFactory("VGUI_ivgui006", nullptr));
    CreateInterfaceFn factories[] = {engineFactory, guiFactory, fileFactory};
    Require(vgui->Init(factories, 3), "initialize real VGUI services with engine surface");
    static_cast<IKeyValues*>(guiFactory("KeyValues003", nullptr))->RegisterSizeofKeyValues(FixtureKeyValuesBytes());
    fontSurface = static_cast<vgui2::ISurface*>(engineFactory("VGUI_Surface026", nullptr));
    Require(fontSurface != nullptr, "actual engine-owned surface");
    fontSurface->SetProportionalBase(ScreenWidth, ScreenHeight);
    auto* schemes = static_cast<vgui2::ISchemeManager*>(guiFactory("VGUI_Scheme009", nullptr));
    const auto schemeHandle = schemes->LoadSchemeFromFile("resource/TrackerScheme.res", "BaseUI");
    Require(schemeHandle != 0, "load retained TrackerScheme");
    auto* scheme = schemes->GetIScheme(schemeHandle);
    creditsFont = scheme->GetFont(std::atoi(scheme->GetResourceString("UseScaledCreditsFont")) ?
        "CreditsFont" : "Legacy_CreditsFont", true);
    Require(creditsFont && fontSurface->GetFontTall(creditsFont) > 0, "actual selected CreditsFont");
    // A real game initializes the embedded root before exposing its HUD. This
    // sentinel is never traversed by this rendering-callback fixture.
    fontSurface->SetEmbeddedPanel(1);
    gHUD.m_scrinfo.iCharHeight = 13;
    for (auto& width : gHUD.m_scrinfo.charWidths) {
        width = 7;
    }
    auto manager = reinterpret_cast<void* (*)()>(dlsym(engine, "_Z11FontManagerv"));
    auto fontForChar = reinterpret_cast<void* (*)(void*, vgui2::HFont, wchar_t)>(
        dlsym(engine, "_ZN12CFontManager14GetFontForCharEjw"));
    Require(manager && fontForChar, "actual engine font coverage probe");
    for (wchar_t character : std::wstring(L"ПриветZażółćgęśląjaźń")) {
        Require(fontForChar(manager(), creditsFont, character) != nullptr, "CreditsFont glyph coverage");
    }
    const std::string scenario = argv[3];
    if (scenario == "russian") {
        CheckGlyphs(u8"Привет", {0x41f, 0x440, 0x438, 0x432, 0x435, 0x442}, 0);
    } else if (scenario == "polish") {
        CheckGlyphs(u8"Zażółć", {'Z', 'a', 0x17c, 0xf3, 0x142, 0x107}, 0);
    } else if (scenario == "effects") {
        for (int effect : {0, 1, 2}) {
            CheckGlyphs(u8"Żук", {0x17b, 0x443, 0x43a}, effect);
        }
        CHudMessage hud;
        const std::string text = u8"ЖЖ";
        auto message = Message(text, 1);
        const int width = Width(0x416);
        message.x = static_cast<float>(ScreenWidth - width) / ScreenWidth;
        glyphs.clear();
        hud.MessageDrawScan(&message, 1);
        Require(glyphs.size() == 4, "flicker and normal draws share actual Unicode bounds");
        Require(glyphs[0].x == glyphs[1].x && glyphs[2].x == glyphs[3].x,
            "flicker highlight overlays its normal glyph");
    } else if (scenario == "timing") {
        CHudMessage hud;
        const std::string text = u8"Привет";
        auto message = Message(text, 2);
        hud.m_pMessages[0] = &message;
        glyphs.clear();
        gHUD.m_flTime = 0.15f;
        hud.Draw(gHUD.m_flTime);
        Require(std::fabs(hud.m_parms.fadeTime - 2.6f) < 0.0001f,
            "effect2 fade starts after six Unicode characters, not twelve bytes");
        Require(std::count_if(glyphs.begin(), glyphs.end(), [](const Glyph& g) {
            return g.red || g.green || g.blue;
        }) == 1, "effect2 reveals one complete glyph after its first interval");
        gHUD.m_flTime = 3.2f;
        hud.Draw(gHUD.m_flTime);
        Require(hud.m_pMessages[0] == nullptr, "effect2 expires using the same Unicode count as its scan");
    } else if (scenario == "multiline") {
        for (const std::string text : {"i\nWWW", "i\nWWW\n"}) {
            CHudMessage hud;
            auto message = Message(text);
            message.x = -0.5f;
            glyphs.clear();
            hud.MessageDrawScan(&message, 1);
            const int width = Width('W') * 3;
            Require(hud.m_parms.totalWidth == width, "last nonempty line contributes to maximum width");
            Require(glyphs.size() == 4 && glyphs[0].x == ScreenWidth / 2 - width,
                "right-aligned lines share the complete final-line width");
            Require(hud.m_parms.length == 4, "line separators do not add effect2 delay");
            Require(glyphs[1].y == fontSurface->GetFontTall(creditsFont), "real line height positions next line");
        }
    } else if (scenario == "long-line") {
        CheckGlyphs(std::string(90, 'i'), std::vector<int>(90, 'i'), 0);
    } else if (scenario == "no-final-newline") {
        const char exact[] = {'A', '\0'};
        CheckGlyphs(exact, {'A'}, 0);
        CheckGlyphs("", {}, 0);
    } else if (scenario == "clipping") {
        CHudMessage hud;
        std::string text = u8"Ж";
        for (int line = 1; line < 40; ++line) {
            text += u8"\nЖ";
        }
        auto message = Message(text, 1);
        // An oversized block begins above the viewport. Only complete final lines fit.
        message.y = 0;
        glyphs.clear();
        hud.MessageDrawScan(&message, 1);
        const int fontHeight = fontSurface->GetFontTall(creditsFont);
        Require(fontHeight == 17 || fontHeight == 22, "retained scaled/legacy fixture font height");
        const std::size_t expectedGlyphs = fontHeight == 17 ? 56 : 42;
        const int expectedFirstY = fontHeight == 17 ? 4 : 18;
        Require(glyphs.size() == expectedGlyphs,
            "clip oversized block to complete visible lines and both flicker passes");
        Require(glyphs.front().y == expectedFirstY, "first complete glyph begins inside actual font bounds");
        for (const auto& glyph : glyphs) {
            Require(glyph.x >= 0 && glyph.x + Width(glyph.character) <= ScreenWidth &&
                glyph.y >= 0 && glyph.y + fontSurface->GetFontTall(creditsFont) <= ScreenHeight,
                "normal and highlight draws remain inside actual glyph bounds");
        }
    } else if (scenario == "byte-fallback") {
        for (const std::string text : {std::string("caf\xe9", 4), std::string("A\xc3Z", 3),
                std::string("\x93test\x94", 6)}) {
            CHudMessage hud;
            auto message = Message(text);
            glyphs.clear();
            hud.MessageDrawScan(&message, 1);
            Require(glyphs.size() == text.size(), "invalid UTF8 and CP1252 preserve complete byte glyphs");
            for (std::size_t index = 0; index < text.size(); ++index) {
                Require(glyphs[index].character == static_cast<unsigned char>(text[index]),
                    "fallback keeps unsigned legacy byte values");
                Require(glyphs[index].x == static_cast<int>(index) * 7, "fallback uses legacy SCREENINFO widths");
            }
            Require(hud.m_parms.totalHeight == 13, "byte fallback retains SCREENINFO height");
        }
    } else if (scenario == "service-lifetime") {
        auto callback = gEngfuncs.pfnDrawCharacter;
        for (int unavailable : {0, 1}) {
            CHudMessage hud;
            if (unavailable == 0) {
                gEngfuncs.pfnDrawCharacter = nullptr;
            } else {
                gEngfuncs.pfnDrawCharacter = callback;
                fontSurface->SetEmbeddedPanel(0);
            }
            const std::string text = "ASCII";
            auto message = Message(text);
            glyphs.clear();
            hud.MessageDrawScan(&message, 1);
            Require(glyphs.size() == 5 && hud.m_parms.totalWidth == 35 && hud.m_parms.totalHeight == 13,
                "unavailable services use byte metrics without stale borrowed font pointers");
        }
        gEngfuncs.pfnDrawCharacter = callback;
        fontSurface->SetEmbeddedPanel(1);
        CheckGlyphs(u8"Żук", {0x17b, 0x443, 0x43a}, 0);
    } else {
        Require(false, "unknown scenario");
    }
    std::printf("PASS: %s (CreditsFont=%u tall=%d)\n", argv[3], creditsFont, fontSurface->GetFontTall(creditsFont));
    // Providers remain owned through process teardown; fixture roots are never traversed.
    return 0;
}
