#include <cassert>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <map>
#include <string>
#include "legacy_text_encoding.h"
constexpr int MAX_MENU_STRING = 512;
constexpr int HUD_ACTIVE = 1;
constexpr int TRUE = 1;
constexpr int FALSE = 0;
char g_szMenuString[MAX_MENU_STRING]{};
char g_szPrelocalisedMenuString[MAX_MENU_STRING]{};
char g_szCompletedMenuSource[MAX_MENU_STRING]{};
std::map<std::string, std::string> titles;
struct Message { const char* pMessage; };
Message* TextMessageGet(const char* name) {
    auto found = titles.find(name);
    static Message message;
    if (found == titles.end()) { return nullptr; }
    message.pMessage = found->second.c_str();
    return &message;
}
struct CHudTextMessage {
    static char* LocaliseTextString(const char*, char*, int);
    static char* BufferedLocaliseTextString(const char* source) {
        static char text[1024];
        return LocaliseTextString(source, text, sizeof(text));
    }
};
#include "localize.inc"
struct { float m_flTime = 14; CHudTextMessage m_TextMessage; } gHUD;
int commandCount = 0;
struct { const char* (*Key_LookupBinding)(const char*); } gEngfuncs;
#include "bindings.inc"
int readValue = 0;
int packetValues[3];
const char* packetText;
#define BEGIN_READ(buffer, size) readValue = 0
#define READ_SHORT() packetValues[readValue++]
#define READ_CHAR() packetValues[readValue++]
#define READ_BYTE() packetValues[readValue++]
#define READ_STRING() packetText
struct CHudMenu {
    int m_bitsValidSlots = 7;
    int m_fWaitingForMore = 0;
    int m_fMenuDisplayed = 1;
    int m_iFlags = HUD_ACTIVE;
    float m_flShutoffTime = 88;
    void RefreshLanguage();
    int MsgFunc_ShowMenu(const char*, int, void*);
};
#include "menu-refresh.inc"
void Receive(CHudMenu& menu, const char* text, bool more) {
    packetValues[0] = 7;
    packetValues[1] = 9;
    packetValues[2] = more;
    packetText = text;
    menu.MsgFunc_ShowMenu("ShowMenu", 0, nullptr);
}
int main() {
    gEngfuncs.Key_LookupBinding = [](const char* action) -> const char* {
        return strcmp(action, "jump") == 0 ? "SPACE" : nullptr;
    };
    titles["HELP"] = "English +jump";
    CHudMenu menu;
    Receive(menu, "#HE", true);
    Receive(menu, "LP", false);
    assert(strcmp(g_szMenuString, "English [SPACE]") == 0);
    const float expiry = menu.m_flShutoffTime;
    Receive(menu, "next unfinished", true);
    const std::string assembly = g_szPrelocalisedMenuString;
    titles["HELP"] = "Русский +jump";
    menu.RefreshLanguage();
    assert(strcmp(g_szMenuString, "Русский [SPACE]") == 0);
    assert(g_szPrelocalisedMenuString == assembly);
    assert(menu.m_fWaitingForMore && menu.m_bitsValidSlots == 7 && menu.m_fMenuDisplayed);
    assert(menu.m_flShutoffTime == expiry && menu.m_iFlags == HUD_ACTIVE && commandCount == 0);
    std::string longText(12000, 'x');
    longText += " +jump";
    char* expanded = nullptr;
    assert(KB_ConvertString(&longText[0], &expanded));
    assert(strlen(expanded) == 12008);
    free(expanded);
    puts("PASS actual HL menu/key owners: complete vs multipart source, language, key, flags and expiry");
}
