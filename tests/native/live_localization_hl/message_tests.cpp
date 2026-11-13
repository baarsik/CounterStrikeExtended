#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <string>
#include "localization_message_snapshot.h"
#include "legacy_text_encoding.h"
#include <algorithm>
#include <vector>
namespace cse {
namespace hud_message {
struct CreditsMetrics { explicit operator bool() const { return true; } };
CreditsMetrics BorrowCreditsMetrics(const void*) { return {}; }
#include "deadline-text.inc"
}
}
struct { const void* pfnDrawCharacter = nullptr; } gEngfuncs;
struct client_textmessage_t {
    int effect{};
    unsigned char r1{}, g1{}, b1{}, a1{}, r2{}, g2{}, b2{}, a2{};
    float x{}, y{}, fadein{}, fadeout{}, holdtime{}, fxtime{};
    const char* pName{};
    const char* pMessage{};
};
constexpr int maxHUDMessages = 16;
constexpr int HUD_ACTIVE = 1;
struct { float m_flTime = 12.0f; } gHUD;
client_textmessage_t g_pCustomMessage;
char* g_pCustomName = const_cast<char*>("Custom");
char g_pCustomText[1024];
client_textmessage_t title;
client_textmessage_t* TextMessageGet(const char* name) {
    return std::strcmp(name, "TITLE") == 0 ? &title : nullptr;
}
bool IsCatalogueMessage(const client_textmessage_t* message) { return message == &title; }
class CHudMessage {
public:
    client_textmessage_t* m_pMessages[maxHUDMessages]{};
    cse::localization::MessageSnapshot<client_textmessage_t> m_ownedMessages[maxHUDMessages];
    cse::localization::MessageSnapshot<client_textmessage_t> m_ownedGameTitle;
    float m_startTime[maxHUDMessages]{};
    float m_messageEndTimes[maxHUDMessages]{};
    struct { float time{}; client_textmessage_t* pMessage{}; } m_parms;
    client_textmessage_t* m_pGameTitle{};
    float m_gameTitleTime = 10.0f;
    bool m_bEndAfterMessage = true;
    int m_iFlags = HUD_ACTIVE;
    void MessageAdd(const char*, float);
    void MessageAdd(client_textmessage_t*);
    void RefreshLanguage();
    float MessageEndTime(int) const;
    void ReleaseMessage(int);
};
#include "messages.inc"
int main() {
    std::string text = "English";
    title.pName = "TITLE";
    title.pMessage = text.c_str();
    title.holdtime = 8.0f;
    title.x = 0.25f;
    CHudMessage hud;
    hud.MessageAdd("#TITLE", 4.0f);
    text.assign("MUTATED");
    assert(std::strcmp(hud.m_pMessages[0]->pMessage, "English") == 0);
    std::string translated = "Русский";
    title.pMessage = translated.c_str();
    title.holdtime = 1.0f;
    hud.RefreshLanguage();
    assert(std::strcmp(hud.m_pMessages[0]->pMessage, translated.c_str()) == 0);
    assert(hud.m_startTime[0] == 4.0f && hud.m_pMessages[0]->holdtime == 8.0f);
    assert(hud.m_bEndAfterMessage && hud.m_iFlags == HUD_ACTIVE && hud.m_gameTitleTime == 10.0f);
    hud.MessageAdd("#literal", 7.0f);
    client_textmessage_t second = title;
    second.pMessage = "second literal";
    hud.MessageAdd(&second);
    assert(std::strcmp(hud.m_pMessages[1]->pMessage, "#literal") == 0);
    client_textmessage_t director = title;
    std::string server = "#TITLE";
    director.pMessage = server.c_str();
    director.pName = "TITLE";
    hud.MessageAdd(&director);
    server.assign("changed");
    hud.RefreshLanguage();
    assert(std::strcmp(hud.m_pMessages[3]->pMessage, "#TITLE") == 0);
    assert(hud.m_startTime[3] == gHUD.m_flTime);
    std::puts("PASS actual HL message owners: snapshots, typed literal provenance, timing and ending state");
}
