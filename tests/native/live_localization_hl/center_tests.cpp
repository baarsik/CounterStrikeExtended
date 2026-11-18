#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include "localization_client.h"
#include "legacy_text_encoding.h"
#include <memory>
struct client_textmessage_s {
    const char* pMessage;
    int effect;
};
using client_textmessage_t = client_textmessage_s;
std::map<std::string, std::string> titles;
client_textmessage_t record{};
client_textmessage_t* TextMessageGet(const char* name) {
    auto found = titles.find(name);
    if (found == titles.end()) { return nullptr; }
    record.pMessage = found->second.c_str();
    return &record;
}
bool IsCatalogueMessage(const client_textmessage_t* message) { return message == &record; }
int printed = 0;
int replaced = 0;
std::string center;
float expiry = 22.0f;
void CenterPrint(const char* text) { center = text; ++printed; }
void ConsolePrint(const char*) {}
bool Replace(const char* expected, const char* replacement) {
    if (center != expected) { return false; }
    center = replacement;
    ++replaced;
    return true;
}
cse::localization::EngineApi engine{};
const cse::localization::EngineApi* GetLocalizationEngine() { return &engine; }
constexpr int HUD_PRINTCENTER = 4;
constexpr int HUD_PRINTCONSOLE = 2;
constexpr int HUD_PRINTNOTIFY = 1;
constexpr int HUD_PRINTTALK = 3;
constexpr int FALSE = 0;
struct Viewport { bool allowed = true; bool AllowedToPrintText() { return allowed; } };
Viewport* gViewPort = nullptr;
struct SayText {
    std::string text;
    std::shared_ptr<HudTextSource> source;
    int prints = 0;
    void SayTextPrint(const char* value, int, int = -1, std::shared_ptr<HudTextSource> provenance = {}) {
        text = value;
        source = provenance;
        ++prints;
    }
};
int indexRead = 0;
const char* packet[5];
int destination = HUD_PRINTCENTER;
void BeginRead(void* buffer, int size) {
    indexRead = 0;
    if (!buffer) {
        return;
    }
    const char* cursor = static_cast<const char*>(buffer);
    const char* end = cursor + size;
    destination = static_cast<unsigned char>(*cursor++);
    for (const char*& value : packet) {
        value = cursor < end ? cursor : "";
        if (cursor < end) {
            cursor += strlen(cursor) + 1;
        }
    }
}
#define BEGIN_READ(buffer, size) BeginRead(buffer, size)
#define READ_BYTE() destination
#define READ_STRING() packet[indexRead++]
#define _snprintf snprintf
class CHudTextMessage {
public:
    std::string m_centerSources[5];
    bool m_centerTokens[5]{};
    std::string m_centerRendered;
    bool m_hasCenterSource = false;
    char* LookupString(const char*, int* = nullptr);
    int MsgFunc_TextMsg(const char*, int, void*);
    void RefreshLanguage();
    void PrintLocalVoiceStatus(const char*, bool);
    static char* BufferedLocaliseTextString(const char* key) {
        const auto* message = TextMessageGet(key + 1);
        return const_cast<char*>(message ? message->pMessage : key);
    }
};
struct { SayText m_SayText; CHudTextMessage m_TextMessage; int m_iIntermission = 0; } gHUD;
int g_iUser1 = 2;
using MouseCode = int;
struct Panel {};
struct hud_player_info_t { const char* name = "#Muted"; } g_PlayerInfoList[2];
struct VoiceManager {
    bool blocked = false;
    bool IsInSquelchMode() { return true; }
    void StartSquelchMode() {}
    bool IsPlayerBlocked(int) { return blocked; }
    void SetPlayerBlockedState(int, bool value) { blocked = value; }
} voice;
VoiceManager* GetClientVoiceMgr() { return &voice; }
struct ScorePanel {
    int m_iHighlightRow = 0;
    int m_iSortedRows[1]{1};
    struct HitTest { void setVisible(bool) {} } m_HitTestPanel;
    void mousePressed(MouseCode, Panel*);
};
char* safe_strcpy(char* destination, const char* source, int length) {
    strncpy(destination, source, length);
    destination[length - 1] = 0;
    return destination;
}
int safe_sprintf(char* destination, int length, const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(destination, length, format, args);
    va_end(args);
    return 0;
}
#include "center.inc"
#include "local-notifications.inc"

void CheckLocalCenter(void (*produce)(), const char* title) {
    titles[title] = "English local hint";
    produce();
    assert(center == "English local hint");
    assert(gHUD.m_TextMessage.m_hasCenterSource);
    const int initialPrints = printed;
    titles[title] = "Polska wskazowka";
    gHUD.m_TextMessage.RefreshLanguage();
    assert(center == "Polska wskazowka");
    assert(printed == initialPrints && expiry == 22.0f);
}
int main() {
    engine.ReplaceCenterText = Replace;
    CHudTextMessage hud;
    packet[0] = "#FORMAT";
    packet[1] = "#unknown_player";
    packet[2] = "#PARAM";
    packet[3] = packet[4] = "";
    titles["FORMAT"] = "English %s %s";
    titles["PARAM"] = "One\n";
    hud.MsgFunc_TextMsg("TextMsg", 0, nullptr);
    assert(center == "English #unknown_player One" && printed == 1);
    titles["FORMAT"] = "Русский %s %s";
    titles["PARAM"] = "Один\n";
    titles["unknown_player"] = "must stay literal";
    hud.RefreshLanguage();
    assert(center == "Русский #unknown_player Один");
    assert(printed == 1 && replaced == 1 && expiry == 22.0f);
    char clipped[3]{static_cast<char>(0xd0), static_cast<char>(0x90), 0};
    const char* unicodeSources[]{"А", "", "", "", ""};
    clipped[1] = 0;
    FinishHudText(clipped, unicodeSources);
    assert(clipped[0] == 0);
    char cp1252[]{static_cast<char>(0xe9), 0};
    const char* byteSources[]{cp1252, "", "", "", ""};
    FinishHudText(cp1252, byteSources);
    assert(static_cast<unsigned char>(cp1252[0]) == 0xe9);
    center = "new server literal";
    hud.RefreshLanguage();
    assert(center == "new server literal" && printed == 1 && replaced == 1);
    packet[0] = "literal %s";
    packet[1] = "#literal";
    packet[2] = "";
    hud.MsgFunc_TextMsg("TextMsg", 0, nullptr);
    const std::string literal = center;
    titles["literal"] = "must stay literal";
    hud.RefreshLanguage();
    assert(center == literal && replaced == 1);
    CheckLocalCenter(LocalDuckOnHide, "Spec_Duck");
    CheckLocalCenter(LocalDuckOnJoin, "Spec_Duck");
    CheckLocalCenter(LocalSpectatorMode, "Spec_Mode2");

    titles["Muted"] = "You have muted %s.\r";
    titles["Unmuted"] = "You have unmuted %s.\r";
    titles["No_longer_hear_that_player"] = "You will no longer hear that player speak.\r";
    ScorePanel scoreboard;
    scoreboard.mousePressed(0, nullptr);
    assert(voice.blocked);
    assert(gHUD.m_SayText.text == "** You have muted #Muted.\n You will no longer hear that player speak.\n\n");
    assert(gHUD.m_SayText.source);
    titles["Muted"] = "Wyciszono %s.\r";
    titles["Unmuted"] = "Wlaczono glos %s.\r";
    titles["No_longer_hear_that_player"] = "Nie uslyszysz tego gracza.\r";
    assert(gHUD.m_SayText.source->Render() == "** Wyciszono #Muted.\n Nie uslyszysz tego gracza.\n\n");
    assert(gHUD.m_SayText.prints == 1);
    scoreboard.mousePressed(0, nullptr);
    assert(!voice.blocked && gHUD.m_SayText.text == "** Wlaczono glos #Muted.\n\n");
    assert(gHUD.m_SayText.source);
    titles["Unmuted"] = "You have unmuted %s.\r";
    assert(gHUD.m_SayText.source->Render() == "** You have unmuted #Muted.\n\n");
    assert(gHUD.m_SayText.prints == 2);
    Viewport viewport;
    viewport.allowed = false;
    gViewPort = &viewport;
    gHUD.m_TextMessage.PrintLocalVoiceStatus("hidden", true);
    assert(gHUD.m_SayText.prints == 2);
    gViewPort = nullptr;

    constexpr std::size_t LastCompleteByteCount = 126;
    const std::string prefix(LastCompleteByteCount, 'a');
    for (const char*& field : packet) {
        field = "";
    }
    packet[0] = "#LONG_FORMAT";
    titles["LONG_FORMAT"] = prefix + "ą";
    hud.MsgFunc_TextMsg("TextMsg", 0, nullptr);
    assert(center == prefix);
    titles["LONG_FORMAT"] = "%s";
    packet[1] = "#LONG_PARAMETER";
    titles["LONG_PARAMETER"] = prefix + "ą";
    hud.MsgFunc_TextMsg("TextMsg", 0, nullptr);
    assert(center == prefix);
    titles["LONG_PARAMETER"] = prefix + static_cast<char>(0xe9) + "x";
    hud.MsgFunc_TextMsg("TextMsg", 0, nullptr);
    assert(center == prefix + static_cast<char>(0xe9));
    puts("PASS actual HL center owner: typed sources, literal parameters, stale rejection and no event replay");
}
