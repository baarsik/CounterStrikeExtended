#include <cassert>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "vgui_language.h"
#include "resource_text.h"
#include "legacy_text_encoding.h"
std::string root;
std::string locale = "valve";
std::map<std::string, std::string> files;
std::map<std::string, std::string> titles;
struct Message { const char* pMessage; };
Message* TextMessageGet(const char* name) {
    const auto found = titles.find(name);
    static Message message;
    if (found == titles.end()) { return nullptr; }
    message.pMessage = found->second.c_str();
    return &message;
}
struct CHudTextMessage { static char* LocaliseTextString(const char*, char*, int); };
#include "localize.inc"
int generations = 0;
class CSchemeManager {
public:
    struct CScheme { vgui::Font font; };
    CScheme* m_pSchemeList = nullptr;
    int m_iNumSchemes = 0;
    int m_xRes = 640;
    std::string m_language = "english";
    std::map<std::string, std::pair<CScheme*, int>> m_languageSchemes;
    CSchemeManager() { Load(640); }
    ~CSchemeManager() {
        std::set<CScheme*> owned{m_pSchemeList};
        for (auto& cached : m_languageSchemes) { owned.insert(cached.second.first); }
        for (auto* schemes : owned) { delete[] schemes; }
    }
    void Load(int) {
        m_pSchemeList = new CScheme[1];
        m_pSchemeList->font.generation = ++generations;
        m_iNumSchemes = 1;
    }
    void ReloadLanguage(const char*);
    int getSchemeHandle(const char*) { return 0; }
    vgui::Font* getFont(int) { return &m_pSchemeList->font; }
};
#include "schemes.inc"
struct Viewport {
    CSchemeManager schemes;
    CSchemeManager* GetSchemeManager() { return &schemes; }
} viewport;
Viewport* gViewPort = &viewport;
char* LoadFile(char* name, int, int* length) {
    std::string data;
    auto found = files.find(name);
    if (found != files.end()) {
        data = found->second;
    } else {
        std::ifstream input(root + "/src/deploy/localization/" + locale + "/" + name);
        if (!input) { input.open(root + "/src/deploy/localization/valve/" + name); }
        if (!input) { return nullptr; }
        data.assign(std::istreambuf_iterator<char>(input), {});
    }
    auto* memory = static_cast<char*>(malloc(data.size() + 1));
    memcpy(memory, data.c_str(), data.size() + 1);
    *length = static_cast<int>(data.size());
    return memory;
}
char* ParseFile(char* text, char* token) {
    *token = 0;
    if (!text) { return nullptr; }
    for (;;) {
        while (*text && isspace(static_cast<unsigned char>(*text))) { ++text; }
        if (text[0] == '/' && text[1] == '/') {
            while (*text && *text != '\n') { ++text; }
        } else { break; }
    }
    if (!*text) { return nullptr; }
    if (*text == '{' || *text == '}') {
        *token++ = *text++;
    } else if (*text == '"') {
        ++text;
        while (*text && *text != '"') { *token++ = *text++; }
        if (*text) { ++text; }
    } else {
        while (*text && !isspace(static_cast<unsigned char>(*text))) { *token++ = *text++; }
    }
    *token = 0;
    return text;
}
struct {
    char* (*COM_LoadFile)(char*, int, int*) = LoadFile;
    void (*COM_FreeFile)(void*) = free;
    char* (*COM_ParseFile)(char*, char*) = ParseFile;
} gEngfuncs;
#include "views.inc"
int main(int argc, char** argv) {
    assert(argc == 2);
    root = argv[1];
    titles["TITLE"] = "English";
    titles["Valve_Close"] = "Close";
    {
        LocalizedLabel token("English");
        token.BindLanguage("#TITLE", "Title Font");
        LocalizedLabel player("#TITLE");
        player.BindLanguage(nullptr, "Title Font");
        vgui::Font* original = token.font;
        LocalizedTextPanel body("MOTD #TITLE", 0, 0, 300, 200);
        vgui::ScrollPanel scroll;
        body.SetLanguageScroll(&scroll);
        body.BindLanguage(nullptr, "Briefing Text");
        assert(body.text == "MOTD #TITLE");
        titles["TITLE"] = "Русский";
        viewport.schemes.ReloadLanguage("russian");
        RefreshLanguageViews();
        assert(token.text == "Русский" && player.text == "#TITLE");
        assert(token.font != original && token.font == player.font);
        assert(body.text == "MOTD #TITLE" && scroll.horizontal == 13 && scroll.vertical == 57);
        files["maps/test.txt"] = "Карта";
        body.BindFile("maps/test.txt", "#TITLE");
        assert(body.text == "Карта");
        files["maps/test.txt"] = "Mapa";
        RefreshLanguageViews();
        assert(body.text == "Mapa" && scroll.vertical == 57);
        viewport.schemes.ReloadLanguage("english");
        RefreshLanguageViews();
        assert(token.font == original && generations == 2);
    }
    RefreshLanguageViews(); // Destroyed views do not leave dangling callbacks.
    {
        LocalizedLabel closed("closed");
        closed.BindLanguage("#TITLE", "Title Font");
        closed.StopWatchingLanguage();
        titles["TITLE"] = "Closed literal stays";
        RefreshLanguageViews();
        assert(closed.text == "Русский");
        closed.WatchLanguage();
        RefreshLanguageViews();
        assert(closed.text == "Closed literal stays");
        ResetLanguageViews();
        titles["TITLE"] = "Русский";
    }
    for (int repeat = 0; repeat < 1000; ++repeat) {
        viewport.schemes.ReloadLanguage(repeat % 2 ? "english" : "russian");
    }
    assert(generations == 2);
    locale = "valve_french";
    assert(ResolveCommandLabel("spectatormenu.txt", "spec_menu 0", 0, "#Valve_Close") == "OK");
    locale = "valve";
    assert(ResolveCommandLabel("spectatormenu.txt", "spec_menu 0", 0, "#Valve_Close") == "Close");
    files["custom.txt"] = "MAP test 1 First first TEAM1 2 Second second 3 Sub { TOGGLE 4 On toggle } 5 Again first";
    assert(ResolveCommandLabel("custom.txt", "first", 1, "fallback") == "Again");
    assert(ResolveCommandLabel("custom.txt", "second", 0, "fallback") == "Second");
    assert(ResolveCommandLabel("custom.txt", "{", 0, "fallback") == "Sub");
    assert(ResolveCommandLabel("missing.txt", "first", 0, "#TITLE") == "Русский");
    puts("PASS actual HL view/cache owners: typed literals, file labels, scroll/font rebind, bounded locale cache and lifetime");
}
