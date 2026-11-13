#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <string>
#include <algorithm>
#include <vector>
#include "localization_message_snapshot.h"
#include "legacy_text_encoding.h"

struct client_textmessage_t {
    int effect{};
    unsigned char r1{};
    unsigned char g1{};
    unsigned char b1{};
    unsigned char a1{};
    unsigned char r2{};
    unsigned char g2{};
    unsigned char b2{};
    unsigned char a2{};
    float x{};
    float y{};
    float fadein{};
    float fadeout{};
    float holdtime{};
    float fxtime{};
    const char* pName{};
    const char* pMessage{};
};
constexpr int maxHUDMessages = 16;
constexpr int HUD_ACTIVE = 1;
constexpr int ScreenWidth = 640;
constexpr int ScreenHeight = 480;
struct Rectangle {
    int left{};
    int right{};
    int top{};
    int bottom{};
} rectangle;
struct {
    float m_flTime = 10.0f;
    Rectangle& GetSpriteRect(int) {
        return rectangle;
    }
    int GetSprite(int) {
        return 0;
    }
} gHUD;
int clientCommands = 0;
int ClientCommand(char*) {
    ++clientCommands;
    return 1;
}
struct {
    int (*pfnDrawCharacter)(int, int, int, int, int, int) = nullptr;
    int (*pfnClientCmd)(char*) = ClientCommand;
} gEngfuncs;
void SPR_Set(int, float, float, float) {}
void SPR_DrawAdditive(int, int, int, const Rectangle*) {}
namespace cse {
namespace hud_message {
struct CreditsMetrics {
    explicit operator bool() const {
        return true;
    }
};
CreditsMetrics BorrowCreditsMetrics(const void*) {
    return {};
}
#include "deadline-text.inc"
}
}
client_textmessage_t g_pCustomMessage;
char* g_pCustomName = const_cast<char*>("Custom");
char g_pCustomText[1024];
client_textmessage_t title;
client_textmessage_t other;
client_textmessage_t* TextMessageGet(const char* name) {
    if (std::strcmp(name, "TITLE") == 0) {
        return &title;
    }
    return std::strcmp(name, "OTHER") == 0 ? &other : nullptr;
}
bool IsCatalogueMessage(const client_textmessage_t* message) {
    return message == &title || message == &other;
}
class CHudMessage {
public:
    client_textmessage_t* m_pMessages[maxHUDMessages]{};
    cse::localization::MessageSnapshot<client_textmessage_t> m_ownedMessages[maxHUDMessages];
    cse::localization::MessageSnapshot<client_textmessage_t> m_ownedGameTitle;
    float m_startTime[maxHUDMessages]{};
    float m_messageEndTimes[maxHUDMessages]{};
    struct {
        float time{};
        float charTime = 0.75f;
        client_textmessage_t* pMessage{};
    } m_parms;
    client_textmessage_t* m_pGameTitle{};
    float m_gameTitleTime = 0;
    bool m_bEndAfterMessage = false;
    int m_iFlags = HUD_ACTIVE;
    int m_HUD_title_half = 0;
    int m_HUD_title_life = 0;
    int scans = 0;
    float lastScanTime = -1;
    std::string lastScanText;
    float FadeBlend(float, float, float, float) {
        return 0;
    }
    int XPosition(float, int, int) {
        return 0;
    }
    int YPosition(float, int) {
        return 0;
    }
    void MessageDrawScan(client_textmessage_t* message, float elapsed) {
        ++scans;
        lastScanTime = elapsed;
        lastScanText = message->pMessage;
        m_parms.pMessage = message;
    }
    void MessageAdd(const char*, float);
    void MessageAdd(client_textmessage_t*);
    void RefreshLanguage();
    void Reset();
    int Draw(float);
    float MessageEndTime(int) const;
    void ReleaseMessage(int);
};
#include "deadline.inc"

void PrepareTitle(const char* text) {
    title = client_textmessage_t{};
    title.pName = "TITLE";
    title.pMessage = text;
    title.effect = 2;
    title.fadein = 0.5f;
    title.holdtime = 3.0f;
    title.fadeout = 2.0f;
    title.x = 0.25f;
}

int main() {
    {
        PrepareTitle("AB");
        CHudMessage hud;
        hud.MessageAdd("#TITLE", 10.0f); // Original deadline: 10 + 2*0.5 + 3 + 2 = 16.
        hud.m_parms.time = 12.0f;
        title.pMessage = "A substantially longer localized typewriter message";
        hud.RefreshLanguage();
        assert(hud.m_startTime[0] == 10.0f && hud.m_parms.time == 12.0f && hud.m_parms.charTime == 0.75f);
        gHUD.m_flTime = 15.0f;
        hud.Draw(15.0f);
        assert(hud.m_pMessages[0] && hud.lastScanTime == 5.0f && hud.scans == 1);
        assert(hud.lastScanText == title.pMessage);
        gHUD.m_flTime = 16.25f;
        hud.Draw(16.25f);
        assert(hud.m_pMessages[0] == nullptr && "longer translation must not extend original expiry");
        assert(hud.scans == 1 && clientCommands == 0);
        title.pMessage = "X";
        hud.RefreshLanguage();
        hud.Draw(17.0f);
        assert(hud.m_pMessages[0] == nullptr && hud.scans == 1 && "expired message must not reappear");
    }
    {
        PrepareTitle("ABCDEFGHIJKL");
        CHudMessage hud;
        hud.MessageAdd("#TITLE", 10.0f); // Original deadline is 21 despite the shorter replacement.
        title.pMessage = "Z";
        hud.RefreshLanguage();
        gHUD.m_flTime = 19.0f;
        hud.Draw(19.0f);
        assert(hud.m_pMessages[0] && hud.lastScanTime == 9.0f && "shorter translation must not expire early");
        gHUD.m_flTime = 21.0f;
        hud.Draw(21.0f);
        assert(hud.m_pMessages[0] && "the original inclusive deadline still renders");
        gHUD.m_flTime = 21.25f;
        hud.Draw(21.25f);
        assert(hud.m_pMessages[0] == nullptr);
    }
    {
        PrepareTitle("Ж\n中");
        CHudMessage hud;
        hud.MessageAdd("#TITLE", 10.0f); // Two visible decoded cells; newline does not add reveal time.
        title.pMessage = "More decoded cells than the original";
        hud.RefreshLanguage();
        gHUD.m_flTime = 16.25f;
        hud.Draw(16.25f);
        assert(hud.m_pMessages[0] == nullptr && "cached deadline must use production Unicode decoding");
    }
    {
        PrepareTitle("AB");
        CHudMessage hud;
        hud.MessageAdd("#TITLE", 100.0f);
        hud.m_parms.time = 100.0f;
        gHUD.m_flTime = 1.0f;
        hud.Draw(1.0f); // Existing level-clock fixup moves start and deadline together.
        const float adjustedEnd = hud.m_startTime[0] + 6.0f;
        title.pMessage = "Very long localized content after the level clock correction";
        hud.RefreshLanguage();
        gHUD.m_flTime = adjustedEnd + 0.25f;
        hud.Draw(gHUD.m_flTime);
        assert(hud.m_pMessages[0] == nullptr && "level-clock fixup must preserve remaining original duration");
    }
    {
        PrepareTitle("AB");
        CHudMessage hud;
        hud.MessageAdd("#TITLE", 10.0f);
        other = title;
        other.pName = "OTHER";
        other.pMessage = "XYZ";
        hud.MessageAdd("#OTHER", 12.0f);
        assert(hud.m_pMessages[0] == nullptr && hud.m_messageEndTimes[0] == 0);
        assert(hud.m_pMessages[1] && hud.m_messageEndTimes[1] == 18.5f);
        hud.Reset();
        assert(hud.m_pMessages[1] == nullptr && hud.m_messageEndTimes[1] == 0);
        assert(hud.m_ownedMessages[1].Get() == nullptr && hud.m_parms.pMessage == nullptr);
        gHUD.m_flTime = 30.0f;
        client_textmessage_t literal = title;
        literal.pMessage = "1234";
        hud.MessageAdd(&literal);
        gHUD.m_flTime = 36.75f;
        hud.Draw(gHUD.m_flTime);
        assert(hud.m_pMessages[0] && "reused literal slot receives its own deadline");
        gHUD.m_flTime = 37.25f;
        hud.Draw(gHUD.m_flTime);
        assert(hud.m_pMessages[0] == nullptr && clientCommands == 0);
    }
    std::puts("PASS actual HL Draw: typewriter deadline survives long/short Unicode translations, clock fixup and slot reuse");
}
