#!/usr/bin/env python3
"""Execute the production spectator formatter and replay with disconnected client inputs."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parents[3]
out = root / 'build/live-localization-menu-crash/spectator-owner'
out.mkdir(parents=True, exist_ok=True)
source_path = Path(os.environ.get('CSE_SPECTATOR_SOURCE', root / 'src/cstrike/vgui2/game_controls/SpectatorGUI.cpp'))
source = source_path.read_text(encoding='cp1252')
def function(signature):
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

helpers = ''
if '// Owned spectator presentation inputs' in source:
    start = source.index('// Owned spectator presentation inputs')
    end = source.index('} // namespace', start) + len('} // namespace')
    helpers = source[start:end]
harness = r'''
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <map>
#include <new>
#include <string>
#include <vector>
#define _snprintf snprintf
#define _snwprintf swprintf
struct CSpectatorGUI;
struct Localize {
    int language = 0;
    std::map<std::string, std::wstring> tokens;
    wchar_t* Find(const char* key) {
        tokens["#Spec_PlayerItem"] = language ? L"PL %s1" : L"RU %s1";
        tokens["#Spec_PlayerItem_Team"] = language ? L"PL %s1 [%s2]" : L"RU %s1 (%s2)";
        tokens["#Spec_Map"] = language ? L"Mapa %s1" : L"Karta %s1";
        tokens["#Spectators"] = language ? L"Widzowie" : L"Zriteli";
        tokens["#OBS_IN_EYE"] = language ? L"Oczy" : L"Glaza";
        tokens["#CAM_OPTIONS"] = language ? L"Kamera PL" : L"Kamera RU";
        tokens["#SPECT_OPTIONS"] = language ? L"Opcje PL" : L"Opcje RU";
        tokens["#CT"] = language ? L"Anty" : L"Kontr";
        tokens["#TERRORIST"] = language ? L"Terr PL" : L"Terr RU";
        tokens["#literal"] = L"MUST NOT TRANSLATE";
        auto found = tokens.find(key ? key : "");
        return found == tokens.end() ? nullptr : found->second.data();
    }
    void ConvertANSIToUnicode(const char* input, wchar_t* output, int bytes) {
        int cells = bytes / sizeof(wchar_t);
        int n = 0;
        while (input && input[n] && n + 1 < cells) { output[n] = (unsigned char) input[n]; ++n; }
        output[n] = 0;
    }
    void ConstructString(wchar_t* output, int bytes, wchar_t* format, int count, ...) {
        std::wstring text = format ? format : L"";
        va_list args;
        va_start(args, count);
        for (int i = 0; i < count; ++i) {
            const wchar_t* value = va_arg(args, const wchar_t*);
            const std::wstring key = L"%s" + std::to_wstring(i + 1);
            const auto position = text.find(key);
            if (position != std::wstring::npos) { text.replace(position, key.size(), value); }
        }
        va_end(args);
        std::wcsncpy(output, text.c_str(), bytes / sizeof(wchar_t) - 1);
        output[bytes / sizeof(wchar_t) - 1] = 0;
    }
} localization;
Localize* localize() { return &localization; }
struct Color { Color(int, int, int, int) {} };
struct KeyValues {
    std::string player;
    KeyValues(const char*, const char*, const char* text) : player(text) {}
    const char* GetString(const char*, const char*) { return player.c_str(); }
};
struct Panel { virtual ~Panel() = default; };
struct Label : Panel {
    std::wstring text;
    void SetText(const wchar_t* value) { text = value ? value : L""; }
    void SetText(const char* value) {
        const wchar_t* translated = value && *value == '#' ? localize()->Find(value) : nullptr;
        if (translated) { text = translated; } else { wchar_t buffer[1024];
            localize()->ConvertANSIToUnicode(value, buffer, sizeof(buffer)); text = buffer; }
    }
    void SetFgColor(Color) {}
};
struct TextEntry : Label { using Label::SetText; };
struct Menu { int GetMenuID(int row) { return row; } };
struct ComboBox : TextEntry {
    struct Row { std::wstring text; KeyValues* data; };
    std::map<int, Row> rows;
    int selected = 1;
    bool popup = true;
    int deletes = 0;
    int revisions = 0;
    Menu menu;
    ~ComboBox() override { for (auto& entry : rows) { delete entry.second.data; } }
    void DeleteAllItems() { ++deletes; for (auto& entry : rows) { delete entry.second.data; } rows.clear(); }
    bool IsItemIDValid(int id) { return rows.count(id); }
    int GetItemCount() { return static_cast<int>(rows.size()); }
    KeyValues* GetItemUserData(int id) { return rows.at(id).data; }
    Menu* GetMenu() { return &menu; }
    int AddItem(const wchar_t* value, KeyValues* data) {
        int id = static_cast<int>(rows.size()); rows[id] = {value, data}; return id;
    }
    void UpdateItem(int id, const wchar_t* value, KeyValues* data) {
        rows.at(id).text = value;
        if (data) { delete rows.at(id).data; rows.at(id).data = data; }
    }
    void RefreshLanguageText() { ++revisions; text = L"stale selected menu item"; }
};
namespace vgui2 { using ::ComboBox; using ::TextEntry; }
struct CBottomBar {
    using CommandComboBox = ComboBox;
    ComboBox players, views, configs;
    ComboBox* m_pPlayerList = &players;
    ComboBox* m_pViewOptions = &views;
    ComboBox* m_pConfigSettings = &configs;
    bool visible = true;
    bool IsVisible() { return visible; }
    void SetPlayerText(const wchar_t* value) { players.SetText(value); }
};
template<class T> struct CUtlVector {
    std::vector<T> values;
    void AddMultipleToTail(int count) { values.resize(count); }
    T* Base() { return values.data(); }
};
struct VGuiLibraryPlayer_t {
    const char* name = "#literal";
    int health = 73;
    bool thisplayer = false;
    bool dead = false;
    int teamnumber = 2;
    const char* teamname = "CT";
};
struct Client {
    bool offline = false;
    int calls = 0;
    VGuiLibraryPlayer_t info;
    void Query() { ++calls; if (offline) { std::abort(); } }
    int TeamNumber() { Query(); return 2; }
    int GetMaxPlayers() { Query(); return 2; }
    VGuiLibraryPlayer_t CallEnghudGetPlayerInfo(int) { Query(); return info; }
} client;
struct Viewport {
    Client* GetClientDllInterface() { return &client; }
    void MakeSafeName(const char* input, char* output, int bytes) { std::snprintf(output, bytes, "%s", input); }
} viewport;
Viewport* gViewPortInterface = &viewport;
struct CommandBuilder { Menu* CreateCommandMenu(char*, int, ComboBox*) { return new Menu; } } builder;
CommandBuilder* CreateCommandCombo() { return &builder; }
void RefreshCommandMenuText(Menu*, Menu*) {}
int ducks = 0;
void DuckMessage(const char*) { ++ducks; }
const char* GetSpectatorLabel(int) { return "#OBS_IN_EYE"; }
struct CSpectatorGUI {
    CBottomBar bottom;
    CBottomBar* m_pBottomBar = &bottom;
    Label player, extra, timer;
    Label* m_pPlayerLabel = &player;
    Label* m_pExtraInfoLabel = &extra;
    bool m_bHelpShown = true;
    bool m_bInsetVisible = true;
    int invalidations = 0;
    ~CSpectatorGUI();
    Panel* FindChildByName(const char* name, bool) { return std::strcmp(name, "extrainfo") == 0 ? &extra : &timer; }
    bool CanSpectateTeam(int) { return true; }
    void GetPlayerLabelColor(int, Color&) { client.Query(); }
    void InvalidateLayout(bool, bool) { ++invalidations; }
    void SetLabelText(const char*, const char*);
    void SetLabelText(const char*, wchar_t*);
    void GetPlayerLabelText(int, wchar_t*, int);
    void UpdateSpectatorPlayerList();
    void Update(const char*, int, int, bool, int, const char*);
    void RefreshLanguage();
};
'''
harness += helpers
for signature in ['CSpectatorGUI::~CSpectatorGUI()',
                  'void CSpectatorGUI::SetLabelText( const char* name, const char* text )',
                  'void CSpectatorGUI::SetLabelText( const char* name, wchar_t* text )',
                  'void CSpectatorGUI::GetPlayerLabelText(',
                  'void CSpectatorGUI::UpdateSpectatorPlayerList()',
                  'void CSpectatorGUI::Update( const char*,',
                  'void CSpectatorGUI::RefreshLanguage()']:
    harness += '\n' + function(signature) + '\n'
harness += r'''
#ifndef RED_CASE
#define RED_CASE 0
#endif
int main() {
    if (RED_CASE == 0) {
        CSpectatorGUI cold;
        cold.bottom.views.SetText("#CAM_OPTIONS");
        cold.bottom.configs.SetText("#SPECT_OPTIONS");
        cold.bottom.players.TextEntry::SetText(L"cold literal player");
        client.offline = true;
        localization.language = 1;
        cold.RefreshLanguage();
        assert(cold.bottom.views.text == L"Kamera PL" && cold.bottom.configs.text == L"Opcje PL");
        assert(cold.bottom.players.text == L"cold literal player");
        assert(cold.bottom.players.selected == 1 && cold.bottom.players.popup);
        wchar_t overrideText[] = L"$900";
        cold.SetLabelText("extrainfo", overrideText);
        localization.language = 0;
        cold.RefreshLanguage();
        assert(cold.bottom.views.text == L"Kamera RU" && cold.bottom.configs.text == L"Opcje RU");
        assert(cold.bottom.players.text == L"cold literal player" && cold.extra.text == L"$900");
    }
    client.offline = false;
    CSpectatorGUI gui;
    gui.timer.SetText(L"1:23");
    char map[] = "#literal";
    char playerName[] = "#literal";
    client.info.name = playerName;
    gui.Update("server", 1, 4, RED_CASE == 3, 8, map);
    gui.UpdateSpectatorPlayerList();
    assert(gui.player.text == L"RU #literal (73)");
    assert(gui.extra.text == (RED_CASE == 3 ? L"Zriteli 8" : L"Karta #literal"));
    map[0] = 'X';
    playerName[0] = 'X';
    auto* userData = gui.bottom.players.rows.at(0).data;
    const int queries = client.calls;
    const int help = ducks;
    const int deletes = gui.bottom.players.deletes;
    client.offline = RED_CASE == 0;
    localization.language = 1;
    gui.RefreshLanguage();
    if (RED_CASE == 1) { assert(gui.extra.text == L"Mapa #literal"); return 0; }
    if (RED_CASE == 2) { assert(gui.bottom.views.text == L"Oczy"); return 0; }
    if (RED_CASE == 3) { assert(gui.extra.text == L"Widzowie 8"); return 0; }
    if (RED_CASE == 4) { assert(gui.player.text == L"PL #literal [73]"); return 0; }
    if (RED_CASE == 5) { assert(gui.bottom.configs.text == L"Opcje PL"); return 0; }
    assert(client.calls == queries && ducks == help);
    assert(gui.player.text == L"PL #literal [73]");
    assert(gui.bottom.players.text == gui.player.text);
    assert(gui.bottom.views.text == L"Oczy" && gui.bottom.configs.text == L"Opcje PL");
    assert(gui.extra.text == L"Mapa #literal");
    assert(gui.bottom.players.rows.at(0).text == L"PL #literal [Anty]");
    assert(gui.bottom.players.rows.at(0).data == userData);
    assert(gui.bottom.players.selected == 1 && gui.bottom.players.popup);
    assert(gui.bottom.views.selected == 1 && gui.bottom.views.popup);
    assert(gui.bottom.configs.selected == 1 && gui.bottom.configs.popup);
    assert(gui.bottom.players.deletes == deletes);
    assert(gui.timer.text == L"1:23" && gui.m_bHelpShown && gui.m_bInsetVisible);
    client.offline = false;
    playerName[0] = '#';
    gui.Update("server", 1, 4, true, 19, "other-map");
    client.offline = true;
    localization.language = 0;
    gui.RefreshLanguage();
    assert(gui.extra.text == L"Zriteli 19");
    wchar_t balance[] = L"$4200";
    gui.SetLabelText("extrainfo", balance);
    localization.language = 1;
    gui.RefreshLanguage();
    assert(gui.extra.text == L"$4200" && gui.player.text == L"PL #literal [73]");
    wchar_t identity[] = L"#literal";
    gui.SetLabelText("extrainfo", identity);
    localization.language = 0;
    gui.RefreshLanguage();
    assert(gui.extra.text == L"#literal");
    assert(client.calls == queries + 2 && ducks == help);
    // A destroyed object's snapshot must not be inherited by a fresh object at the same address.
    alignas(CSpectatorGUI) unsigned char storage[sizeof(CSpectatorGUI)];
    client.offline = false;
    auto* prior = new (storage) CSpectatorGUI;
    prior->Update("server", 1, 4, false, 0, "owned-map");
    prior->~CSpectatorGUI();
    client.offline = true;
    auto* reused = new (storage) CSpectatorGUI;
    reused->RefreshLanguage();
    assert(reused->player.text.empty() && reused->extra.text.empty());
    assert(reused->bottom.views.text == L"Kamera RU" && reused->bottom.configs.text == L"Opcje RU");
    reused->~CSpectatorGUI();
}
'''
(out / 'spectator.cpp').write_text(harness)
subprocess.run(['c++', '-DRED_CASE=' + os.environ.get('CSE_SPECTATOR_RED_CASE', '0'), '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                str(out / 'spectator.cpp'), '-o', str(out / 'spectator')], check=True)
subprocess.run([str(out / 'spectator')], check=True,
               env=dict(os.environ, UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
print('PASS: actual spectator formatter replays owned inputs without client queries or gameplay effects')
