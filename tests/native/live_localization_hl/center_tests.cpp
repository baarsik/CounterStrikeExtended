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
struct Viewport { bool AllowedToPrintText() { return true; } };
Viewport* gViewPort = nullptr;
struct SayText { void SayTextPrint(const char*, int, int = -1, std::shared_ptr<HudTextSource> = {}) {} };
struct { SayText m_SayText; } gHUD;
int indexRead = 0;
const char* packet[5];
#define BEGIN_READ(buffer, size) indexRead = 0
#define READ_BYTE() HUD_PRINTCENTER
#define READ_STRING() packet[indexRead++]
class CHudTextMessage {
public:
    std::string m_centerSources[5];
    bool m_centerTokens[5]{};
    std::string m_centerRendered;
    bool m_hasCenterSource = false;
    char* LookupString(const char*, int* = nullptr);
    int MsgFunc_TextMsg(const char*, int, void*);
    void RefreshLanguage();
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
    puts("PASS actual HL center owner: typed sources, literal parameters, stale rejection and no event replay");
}
