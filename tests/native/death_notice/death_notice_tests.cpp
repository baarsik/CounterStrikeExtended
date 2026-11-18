#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "notices.inc"

struct Rect {
    int left;
    int right;
    int top;
    int bottom;
};

struct DrawEvent {
    std::string text;
    int sprite;
    int x;
    int y;
    std::array<float, 3> color;
};

static std::vector<DrawEvent> events;
static int fontHeight = 33;
static int selectedSprite = 0;
static std::array<float, 3> spriteColor;
static std::array<float, 3> textColor;
static bool spectateOnly = false;
static int g_iUser1 = 0;
static int ScreenWidth = 1000;
static constexpr int glyphAdvance = 8;

struct Hud {
    float m_flTime = 10.0f;
    std::array<Rect, 4> rects = {{{0, 0, 0, 0}, {0, 32, 0, 16}, {0, 12, 0, 16}, {0, 20, 0, 16}}};
    int GetSprite(int id) {
        return id;
    }

    Rect& GetSpriteRect(int id) {
        return rects.at(id);
    }

    int GetSpriteWidth(int id) {
        return rects.at(id).right - rects.at(id).left;
    }

    int GetSpriteHeight(int id) {
        return rects.at(id).bottom - rects.at(id).top;
    }
} gHUD;

struct Viewport {
    bool tutor = false;
    bool allowed = true;
    bool IsTutorTextWindowOpen() {
        return tutor;
    }

    bool AllowedToPrintText() {
        return allowed;
    }

    int GetSpectatorTopBarHeight() {
        return 80;
    }
} viewport;
static Viewport* gViewPortInterface = &viewport;

struct Engine {
    bool IsSpectateOnly() {
        return spectateOnly;
    }

    void pfnDrawConsoleStringLen(const char* text, int* width, int* height) {
        *width = static_cast<int>(std::strlen(text)) * glyphAdvance;
        *height = fontHeight;
    }

    void pfnDrawSetTextColor(float r, float g, float b) {
        textColor = {r, g, b};
    }

    int pfnDrawConsoleString(int x, int y, char* text) {
        events.push_back({text, 0, x, y, textColor});
        return x + static_cast<int>(std::strlen(text)) * glyphAdvance;
    }
} gEngfuncs;

static void SPR_Set(int sprite, int r, int g, int b) {
    selectedSprite = sprite;
    spriteColor = {static_cast<float>(r), static_cast<float>(g), static_cast<float>(b)};
}

static void SPR_DrawAdditive(int, int x, int y, const Rect*) {
    events.push_back({"", selectedSprite, x, y, spriteColor});
}

class CHudDeathNotice {
public:
    int Draw(float time);
    int GetDeathNoticeY();
    int m_HUD_d_skull = 3;
    int m_iDeathNoticeGap = 40;
    int m_iDeathNoticeTop = 32;
    int m_iDeathTextGap = 5;
    int m_iDeathNoticeGapRight = 12;
};

#include "bodies.inc"

static void Require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

static DeathNoticeItem Notice() {
    DeathNoticeItem notice{};
    std::strcpy(notice.szKiller, "killer");
    std::strcpy(notice.szVictim, "victim");
    notice.iId = 1;
    notice.iHeadShotId = 2;
    notice.flDisplayTime = 15.0f;
    return notice;
}

static void CheckEvent(std::size_t index, const char* text, int sprite, int x, int y) {
    const DrawEvent& event = events.at(index);
    Require(event.text == text && event.sprite == sprite && event.x == x && event.y == y,
        "event " + std::to_string(index) + ": expected text=" + text + " sprite=" + std::to_string(sprite)
        + " x=" + std::to_string(x) + " y=" + std::to_string(y) + "; got text=" + event.text
        + " sprite=" + std::to_string(event.sprite) + " x=" + std::to_string(event.x)
        + " y=" + std::to_string(event.y));
}

static void CheckHeadshotRow(std::size_t first, int textY, int weaponY, int headshotY) {
    CheckEvent(first, "killer", 0, 833, textY);
    CheckEvent(first + 1, "", 1, 886, weaponY);
    CheckEvent(first + 2, "", 2, 923, headshotY);
    CheckEvent(first + 3, "victim", 0, 940, textY);
}

static void Run(const std::string& scenario) {
    CHudDeathNotice hud;
    rgDeathNoticeList[0] = Notice();
    if (scenario.rfind("font-", 0) == 0) {
        struct FontCase {
            int height;
            int spriteY;
        };
        constexpr FontCase cases[] = {{13, 32}, {16, 32}, {21, 34}, {28, 38}, {33, 40}, {70, 59}};
        for (const FontCase& font : cases) {
            if (scenario == "font-" + std::to_string(font.height)) {
                fontHeight = font.height;
                Require(hud.Draw(10.0f) == 1, "draw return");
                Require(events.size() == 4, "headshot row draw count");
                CheckHeadshotRow(0, 32, font.spriteY, font.spriteY);
                return;
            }
        }
    } else if (scenario == "independent-heights" || scenario == "taller-sprite") {
        gHUD.rects[1].bottom = scenario == "taller-sprite" ? 40 : 10;
        gHUD.rects[2].bottom = 24;
        hud.Draw(10.0f);
        Require(events.size() == 4, "independent sprite draw count");
        CheckHeadshotRow(0, 32, scenario == "taller-sprite" ? 32 : 43, 36);
        return;
    } else if (scenario == "no-headshot") {
        rgDeathNoticeList[0].iHeadShotId = -1;
        hud.Draw(10.0f);
        Require(events.size() == 3, "no headshot draw count");
        CheckEvent(0, "killer", 0, 850, 32);
        CheckEvent(1, "", 1, 903, 40);
        CheckEvent(2, "victim", 0, 940, 32);
        return;
    } else if (scenario == "assistant-rarity") {
        std::strcpy(rgDeathNoticeList[0].szAssistant, " + ally");
        std::strcpy(rgDeathNoticeList[0].szRarity, " [NS]");
        hud.Draw(10.0f);
        Require(events.size() == 6, "assistant and rarity draw count");
        CheckEvent(0, "killer", 0, 737, 32);
        CheckEvent(1, " + ally", 0, 785, 32);
        CheckEvent(2, "", 1, 846, 40);
        CheckEvent(3, "", 2, 883, 40);
        CheckEvent(4, " [NS]", 0, 900, 32);
        CheckEvent(5, "victim", 0, 940, 32);
        return;
    } else if (scenario == "narrow-optional-text") {
        ScreenWidth = 320;
        std::strcpy(rgDeathNoticeList[0].szAssistant, " + exceptionally_long_assistant_name");
        std::strcpy(rgDeathNoticeList[0].szRarity, " [BLIND] [NS] [WALL] [SMOKE] [FLASH] [REV]");
        hud.Draw(10.0f);
        Require(events.size() == 6, "narrow row retains assistant and a rarity badge");
        Require(events[0].x >= 0, "optional text does not hide killer off-screen");
        Require(events[1].text.find(" + ") == 0, "assistant identity remains visible");
        Require(events[4].text == " [BLIND]", "rarity fits by whole badges");
        CheckEvent(5, "victim", 0, 260, 32);
        return;
    } else if (scenario == "suicide") {
        rgDeathNoticeList[0].iSuicide = 1;
        hud.Draw(10.0f);
        Require(events.size() == 3, "suicide draw count");
        CheckEvent(0, "", 1, 886, 40);
        CheckEvent(1, "", 2, 923, 40);
        CheckEvent(2, "victim", 0, 940, 32);
        return;
    } else if (scenario == "non-player") {
        rgDeathNoticeList[0].iNonPlayerKill = 1;
        hud.Draw(10.0f);
        Require(events.size() == 3, "non-player draw count");
        CheckEvent(0, "killer", 0, 833, 32);
        CheckEvent(1, "", 1, 886, 40);
        CheckEvent(2, "", 2, 923, 40);
        return;
    } else if (scenario == "skull-fallback") {
        rgDeathNoticeList[0].iId = -1;
        rgDeathNoticeList[0].iHeadShotId = -1;
        rgDeathNoticeList[0].iSuicide = 1;
        hud.Draw(10.0f);
        Require(events.size() == 2, "skull fallback draw count");
        CheckEvent(0, "", 3, 915, 40);
        CheckEvent(1, "victim", 0, 940, 32);
        return;
    } else if (scenario == "rows") {
        float killerColor[] = {0.6f, 0.8f, 1.0f};
        float victimColor[] = {1.0f, 0.25f, 0.25f};
        rgDeathNoticeList[0].KillerColor = killerColor;
        rgDeathNoticeList[0].VictimColor = victimColor;
        rgDeathNoticeList[0].iTeamKill = 1;
        rgDeathNoticeList[1] = Notice();
        hud.Draw(10.0f);
        Require(events.size() == 8, "two row draw count");
        CheckHeadshotRow(0, 32, 40, 40);
        CheckHeadshotRow(4, 72, 80, 80);
        Require(events[0].color == std::array<float, 3>{0.6f, 0.8f, 1.0f}, "killer color");
        Require(events[3].color == std::array<float, 3>{1.0f, 0.25f, 0.25f}, "victim color");
        Require(events[1].color == std::array<float, 3>{10.0f, 240.0f, 10.0f}, "teamkill weapon color");
        Require(events[2].color == events[1].color, "teamkill headshot color");
        Require(events[5].color == std::array<float, 3>{255.0f, 80.0f, 0.0f}, "normal weapon color");
        return;
    } else if (scenario == "lifecycle") {
        rgDeathNoticeList[0].flDisplayTime = 9.0f;
        rgDeathNoticeList[1] = Notice();
        rgDeathNoticeList[1].flDisplayTime = 100.0f;
        hud.Draw(10.0f);
        Require(events.size() == 4, "expired item removed without skipping next row");
        CheckHeadshotRow(0, 32, 40, 40);
        Require(rgDeathNoticeList[0].flDisplayTime == 16.0f, "display deadline capped");
        Require(rgDeathNoticeList[1].iId == 0, "list tail cleared by compaction");
        events.clear();
        hud.Draw(16.0f);
        Require(events.size() == 4, "deadline inclusive");
        events.clear();
        hud.Draw(17.0f);
        Require(events.empty() && rgDeathNoticeList[0].iId == 0, "expired list empty");
        return;
    } else if (scenario == "gates") {
        viewport.tutor = true;
        hud.Draw(10.0f);
        Require(events.empty(), "tutor suppresses draws");
        viewport.tutor = false;
        viewport.allowed = false;
        hud.Draw(10.0f);
        Require(events.empty(), "print gate suppresses draws");
        viewport.allowed = true;
        hud.Draw(10.0f);
        Require(events.size() == 4, "printing resumes");
        CheckHeadshotRow(0, 32, 40, 40);
        return;
    } else if (scenario == "spectator") {
        g_iUser1 = 1;
        hud.Draw(10.0f);
        Require(events.size() == 4, "spectator draw count");
        CheckHeadshotRow(0, 80, 88, 88);
        events.clear();
        g_iUser1 = 0;
        spectateOnly = true;
        hud.Draw(10.0f);
        Require(events.size() == 4, "spectate-only draw count");
        CheckHeadshotRow(0, 80, 88, 88);
        return;
    }
    throw std::runtime_error("unknown scenario " + scenario);
}

int main(int argc, char** argv) {
    try {
        Require(argc == 2, "expected scenario argument");
        Run(argv[1]);
        std::cout << "PASS " << argv[1] << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
