#!/usr/bin/env python3
"""Exercise real spectator caption startup, late player info and disconnected language replay."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parents[3]
out = Path(os.environ.get('CSE_SPECTATOR_EVIDENCE', root / 'build/live-localization-menu-crash/spectator-owner'))
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
#include "vgui_smoked_glass.h"
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
using HFont = int;
constexpr HFont OriginalFont = 1;
constexpr HFont ThemedFont = 2;
constexpr int OriginalFontTall = 14;
constexpr int ThemedFontTall = 20;
struct IScheme {} testScheme;
struct SchemeManager {
    IScheme* GetIScheme(int) { return &testScheme; }
} testSchemeManager;
SchemeManager* scheme() { return &testSchemeManager; }
struct Color { Color(int, int, int, int) {} };
struct KeyValues {
    std::string player;
    KeyValues(const char*, const char*, const char* text) : player(text) {}
    const char* GetString(const char*, const char*) { return player.c_str(); }
};
struct Panel {
    bool visible = false;
    int width = 0;
    int height = 0;
    int x = 0;
    int y = 0;
    int minimumHeight = 0;
    virtual ~Panel() = default;
    void SetVisible(bool value) {
        visible = value;
    }
    bool IsVisible() const {
        return visible;
    }
    void MoveToFront() {}
    void SetSize(int wide, int tall) {
        width = wide;
        height = tall < minimumHeight ? minimumHeight : tall;
    }
    void SetPos(int left, int top) {
        x = left;
        y = top;
    }
    void SetBounds(int left, int top, int wide, int tall) {
        x = left;
        y = top;
        SetSize(wide, tall);
    }
    int GetWide() { return width; }
    int GetTall() { return height; }
    int GetScheme() { return 1; }
};
struct Frame : Panel {
    void PerformLayout() {}
    void ApplySchemeSettings(IScheme*) {}
    void Activate() {
        SetVisible(true);
    }
};
struct Surface {
    int GetFontTall(HFont font) {
        return font ? (font == OriginalFont ? OriginalFontTall : ThemedFontTall) : 0;
    }
    int width = 1280;
    int height = 720;
    void GetScreenSize(int& wide, int& tall) {
        wide = width;
        tall = height;
    }
} testSurface;
Surface* surface() {
    return &testSurface;
}
struct Label : Panel {
    std::wstring text;
    HFont font = 0;
    bool needsScheme = true;
    HFont GetFont() { return font; }
    void SetFont(HFont value) { font = value; }
    // Like forced SolveTraverse, this applies a pending scheme even while hidden.
    void MakeReadyForUse();
    void SetText(const wchar_t* value) { text = value ? value : L""; }
    void SetText(const char* value) {
        const wchar_t* translated = value && *value == '#' ? localize()->Find(value) : nullptr;
        if (translated) { text = translated; } else { wchar_t buffer[1024];
            localize()->ConvertANSIToUnicode(value, buffer, sizeof(buffer)); text = buffer; }
    }
    void SetFgColor(Color) {}
};
struct TextEntry : Label { using Label::SetText; };
struct Menu {
    int GetMenuID(int row) { return row; }
    void MakeReadyForUse() {}
    void SetBgColor(Color) {}
};
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
namespace { struct SpectatorPlayerText; }
namespace cse { namespace ui { bool glass = false; bool ControlsSmokedGlass() { return glass; } } }
void Label::MakeReadyForUse() {
    if (needsScheme) {
        font = cse::ui::glass ? ThemedFont : OriginalFont;
        needsScheme = false;
    }
}
namespace smoked {
    HFont TextFont(Panel*, IScheme*, HFont font) {
        return font ? (cse::ui::glass ? ThemedFont : OriginalFont) : 0;
    }
    Color ColorOf(cse::ui::Rgba value) { return Color(value.r, value.g, value.b, value.a); }
}
struct CBottomBar : Frame {
    using CommandComboBox = ComboBox;
    CBottomBar() {
        // Frame's constructor imposes a minimum height before spectator layout runs.
        minimumHeight = 66;
    }
    ComboBox players, views, configs;
    Label health;
    Panel previous;
    Panel next;
    Panel* m_pLeftButton = &previous;
    Panel* m_pRightButton = &next;
    cse::ui::BackdropRect m_originalControlBounds[5]{};
    bool m_layoutReady = true;
    void PerformLayout();
    void ApplySchemeSettings(IScheme*);
    int m_teamNumber = 0;
    Label* m_pHealthLabel = &health;
    void SetTarget(const SpectatorPlayerText&, const wchar_t*);
    ComboBox* m_pPlayerList = &players;
    ComboBox* m_pViewOptions = &views;
    ComboBox* m_pConfigSettings = &configs;
    bool visible = true;
    bool IsVisible() { return visible; }
    void SetVisible(bool value) {
        visible = value;
    }
    using Panel::SetSize;
    using Panel::SetPos;
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
    mutable int calls = 0;
    VGuiLibraryPlayer_t info;
    int mode = 4;
    int target = 1;
    void Query() const {
        ++calls;
        if (offline) {
            std::abort();
        }
    }
    int TeamNumber() { Query(); return 2; }
    int GetMaxPlayers() const {
        Query();
        return 2;
    }
    int SpectatorMode() const {
        Query();
        return mode;
    }
    int SpectatorTarget() const {
        Query();
        return target;
    }
    int PipInsetOff() {
        return 1;
    }
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
struct CSpectatorGUI : Frame {
    CBottomBar bottom;
    CBottomBar* m_pBottomBar = &bottom;
    Label player, extra, timer;
    Label* m_pPlayerLabel = &player;
    Label* m_pExtraInfoLabel = &extra;
    Panel topBar;
    Panel bottomBarBlank;
    Panel banner;
    Panel* m_pTopBar = &topBar;
    Panel* m_pBottomBarBlank = &bottomBarBlank;
    Panel* m_pBannerImage = &banner;
    bool m_bHelpShown = true;
    bool m_bInsetVisible = true;
    int invalidations = 0;
    int controlScale = 1;
    ~CSpectatorGUI();
    void Activate();
    void PerformLayout();
    void OnThink();
    void UpdatePlayerLabel(int player);
    void EnableInsetView(int) {}
    int GetProportionalScaledValue(int value) {
        return value * controlScale;
    }
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
if "void CBottomBar::SetTarget(" in source:
    harness += "\n" + function("void CBottomBar::SetTarget(") + "\n"
for signature in ['void CBottomBar::PerformLayout()', 'void CBottomBar::ApplySchemeSettings(']:
    harness += '\n' + function(signature) + '\n'

for signature in ['CSpectatorGUI::~CSpectatorGUI()',
                  'void CSpectatorGUI::Activate()',
                  'void CSpectatorGUI::PerformLayout()',
                  'void CSpectatorGUI::OnThink()',
                  'void CSpectatorGUI::SetLabelText( const char* name, const char* text )',
                  'void CSpectatorGUI::SetLabelText( const char* name, wchar_t* text )',
                  'void CSpectatorGUI::GetPlayerLabelText(',
                  'void CSpectatorGUI::UpdateSpectatorPlayerList()',
                  'void CSpectatorGUI::Update( const char*,',
                  'void CSpectatorGUI::RefreshLanguage()']:
    harness += '\n' + function(signature) + '\n'
if 'void CSpectatorGUI::UpdatePlayerLabel(' in source:
    harness += '\n' + function('void CSpectatorGUI::UpdatePlayerLabel(') + '\n'
harness += r'''
#ifndef RED_CASE
#define RED_CASE 0
#endif

// Removing hidden-label readiness from the production owner's scheme pass must fail this test.
void CheckFirstOrdinaryRailLayout() {
    constexpr int MinimumCaptionWidth = 100;
    constexpr int CompactPlayerWidth = 80;
    constexpr int MinimumNavigationWidth = 24;
    CBottomBar bottom;
    const cse::ui::BackdropRect resourceBounds[]{
        {5, 30, 135, 22}, {465, 30, 20, 22}, {490, 30, 300, 22}, {795, 30, 20, 22}, {1140, 30, 135, 22}};
    for (std::size_t index = 0; index < std::size(resourceBounds); ++index) {
        bottom.m_originalControlBounds[index] = resourceBounds[index];
    }
    bottom.Frame::SetSize(1280, 144);
    bottom.health.SetVisible(false);
    cse::ui::glass = true;
    // Ordinary SurfaceSDL scheme traversal visits visible children before their parent.
    // The new hidden health label consequently still has its constructor's zero font.
    for (Label* child : {static_cast<Label*>(&bottom.players), static_cast<Label*>(&bottom.views),
             static_cast<Label*>(&bottom.configs), &bottom.health}) {
        if (child != &bottom.health) {
            child->SetVisible(true);
        }
        if (child->IsVisible()) {
            child->MakeReadyForUse();
        }
    }
    bottom.ApplySchemeSettings(&testScheme);
    bottom.PerformLayout();
    std::fprintf(stderr, "ordinary spectator layout: health font=%d, settings=%d, player=%d, previous=%d\n",
        bottom.health.GetFont(), bottom.configs.GetWide(), bottom.players.GetWide(), bottom.previous.GetWide());
    assert(bottom.configs.GetWide() >= MinimumCaptionWidth);
    assert(bottom.players.GetWide() >= MinimumCaptionWidth);
    assert(bottom.previous.GetWide() >= MinimumNavigationWidth && bottom.next.GetWide() >= MinimumNavigationWidth);
    assert(!bottom.health.IsVisible());
    const int initialPlayerWidth = bottom.players.GetWide();
    SpectatorPlayerText target;
    target.formatted = true;
    target.withDetail = true;
    target.name = L"Late player";
    target.detail = L"73";
    bottom.SetTarget(target, L"Late player (73)");
    // Health first appears after parent layout, without invalidating the parent's geometry.
    bottom.health.MakeReadyForUse();
    assert(bottom.players.GetWide() == initialPlayerWidth);
    assert(bottom.health.GetWide() >= MinimumCaptionWidth && bottom.health.text == L"Health 73");

    // Scheme/style replay must also initialize the health label while hidden.
    bottom.health.SetVisible(false);
    bottom.health.needsScheme = true;
    cse::ui::glass = false;
    bottom.ApplySchemeSettings(&testScheme);
    bottom.PerformLayout();
    Panel* controls[]{&bottom.configs, &bottom.previous, &bottom.players, &bottom.next, &bottom.views};
    for (std::size_t index = 0; index < std::size(controls); ++index) {
        const auto& expected = resourceBounds[index];
        assert(controls[index]->x == expected.x && controls[index]->y == expected.y);
        assert(controls[index]->GetWide() == expected.width && controls[index]->GetTall() == expected.height);
    }
    assert(!bottom.health.IsVisible());
    bottom.health.needsScheme = true;
    cse::ui::glass = true;
    bottom.ApplySchemeSettings(&testScheme);
    bottom.Frame::SetSize(640, 96);
    bottom.PerformLayout();
    assert(bottom.configs.GetWide() >= MinimumCaptionWidth && bottom.players.GetWide() >= CompactPlayerWidth);
    assert(bottom.previous.GetWide() >= MinimumNavigationWidth && bottom.next.GetWide() >= MinimumNavigationWidth);
    assert(!bottom.health.IsVisible());
    cse::ui::glass = false;
}

void CheckJoinLifecycle() {
    client.offline = false;
    localization.language = 0;
    for (bool lateInfo : {false, true}) {
        CSpectatorGUI gui;
        client.info.name = lateInfo ? nullptr : "Joined player";
        client.info.health = lateInfo ? 0 : 73;
        gui.Update("server", 1, 4, false, 0, "de_dust2");
        gui.Activate();
        assert(gui.IsVisible() && gui.player.IsVisible());
        assert(gui.player.text.empty());
        if (lateInfo) {
            gui.OnThink();
            client.info.name = "Joined player";
            client.info.health = 73;
        }
        gui.OnThink();
        assert(gui.player.text == L"RU Joined player (73)");

        gui.bottom.SetVisible(true);
        gui.UpdateSpectatorPlayerList();
        const int deletes = gui.bottom.players.deletes;
        auto* rowData = gui.bottom.players.rows.at(0).data;
        const int help = ducks;
        client.info.health = 61;
        gui.OnThink();
        assert(gui.player.text == L"RU Joined player (61)");
        assert(gui.bottom.players.text == L"RU Joined player (61)");
        assert(gui.bottom.players.deletes == deletes);
        assert(gui.bottom.players.rows.at(0).data == rowData);
        assert(gui.bottom.players.selected == 1 && gui.bottom.players.popup);
        assert(ducks == help);

        client.target = 2;
        client.info.name = "#literal";
        client.info.health = 42;
        gui.OnThink();
        assert(gui.player.text == L"RU #literal (42)");
        assert(gui.bottom.players.text == gui.player.text);
        assert(gui.bottom.players.deletes == deletes);
        client.target = 1;

        gui.SetVisible(false);
        client.offline = true;
        const int queries = client.calls;
        gui.OnThink();
        assert(client.calls == queries);
        client.offline = false;
    }
    client.info = VGuiLibraryPlayer_t{};
}


void CheckSmokedTarget() {
    CSpectatorGUI gui;
    client.offline = false;
    client.info.name = "#literal";
    client.info.health = 73;
    cse::ui::glass = true;
    gui.Update("server", 1, 4, false, 0, "de_dust2");
    gui.OnThink();
    assert(gui.bottom.players.text == L"#literal");
    assert(gui.bottom.health.text == L"Health 73" && gui.bottom.health.IsVisible());
    client.offline = true;
    gui.RefreshLanguage();
    assert(gui.bottom.players.text == L"#literal" && gui.bottom.health.text == L"Health 73");
    cse::ui::glass = false;
    gui.RefreshLanguage();
    assert(gui.bottom.players.text.find(L"#literal") != std::wstring::npos);
    assert(gui.bottom.players.text.find(L"73") != std::wstring::npos && !gui.bottom.health.IsVisible());
    client.offline = false;
}

// Resource/control density must not determine the collapsed caption's screen-space Y.
void CheckCollapsedCaptionAnchor() {
    CSpectatorGUI gui;
    cse::ui::glass = true;
    gui.player.SetBounds(407, 588, 467, 35);
    gui.topBar.SetBounds(220, 0, 1060, 64);
    testSurface.width = 1280;
    testSurface.height = 720;
    gui.PerformLayout();
    assert(gui.player.x == 0 && gui.player.y == 656);
    assert(gui.player.GetWide() == 1280 && gui.player.GetTall() == 64);
    // An active inset moves the top bar to its right; bottom caption layout must leave that anchor alone.
    assert(gui.topBar.x == 220 && gui.topBar.y == 0);
    assert(gui.topBar.GetWide() == 1060 && gui.topBar.GetTall() == 64);
    assert(gui.bottomBarBlank.x == 0 && gui.bottomBarBlank.y == 656);
    assert(gui.bottomBarBlank.GetWide() == 1280 && gui.bottomBarBlank.GetTall() == 64);
    assert(gui.bottom.x == 0 && gui.bottom.y == 654);
    assert(gui.bottom.GetWide() == 1280 && gui.bottom.GetTall() == 66);
    assert(gui.bottom.y + gui.bottom.GetTall() == 720);
    testSurface.width = 2560;
    testSurface.height = 1440;
    // Resolution changes first shrink the root to a bar in OnThink, then request layout.
    gui.SetVisible(true);
    client.offline = false;
    gui.OnThink();
    assert(gui.GetTall() == 64);
    gui.PerformLayout();
    assert(gui.GetWide() == 2560 && gui.GetTall() == 1440);
    assert(gui.topBar.x == 220);
    assert(gui.player.y == 1376 && gui.player.GetWide() == 2560);
    assert(gui.bottomBarBlank.y == 1376 && gui.bottomBarBlank.GetWide() == 2560);
    assert(gui.bottom.y == 1374 && gui.bottom.GetWide() == 2560);
    assert(gui.bottom.y + gui.bottom.GetTall() == 1440);
    // Density changes at one resolution must resize the bar while keeping its bottom edge fixed.
    gui.controlScale = 2;
    gui.bottom.minimumHeight = 132;
    gui.PerformLayout();
    assert(gui.player.y == 1312 && gui.player.GetTall() == 128);
    assert(gui.bottomBarBlank.y == 1312 && gui.bottomBarBlank.GetTall() == 128);
    assert(gui.bottom.y == 1308 && gui.bottom.GetTall() == 132);
    assert(gui.bottom.y + gui.bottom.GetTall() == 1440);
    cse::ui::glass = false;
    gui.player.SetBounds(548, 1323, 1050, 78);
    gui.bottomBarBlank.SetBounds(0, 1287, 2560, 156);
    gui.PerformLayout();
    assert(gui.player.x == 548 && gui.player.y == 1323 && gui.player.GetTall() == 78);
    assert(gui.bottomBarBlank.y == 1287 && gui.bottomBarBlank.GetTall() == 156);
    cse::ui::glass = true;
    gui.controlScale = 1;
    gui.bottom.minimumHeight = 66;
    gui.PerformLayout();
    assert(gui.player.x == 0 && gui.player.y == 1376 && gui.player.GetTall() == 64);
    cse::ui::glass = false;
    testSurface.width = 1280;
    testSurface.height = 720;
}

int main() {
    CheckCollapsedCaptionAnchor();
    CheckFirstOrdinaryRailLayout();
    CheckSmokedTarget();
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
    CheckJoinLifecycle();
}
'''
(out / 'spectator.cpp').write_text(harness)
subprocess.run(['c++', '-DRED_CASE=' + os.environ.get('CSE_SPECTATOR_RED_CASE', '0'), '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                '-I', str(root / 'src/public'),
                str(out / 'spectator.cpp'), '-o', str(out / 'spectator')], check=True)
subprocess.run([str(out / 'spectator')], check=True,
               env=dict(os.environ, UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
print('PASS: spectator startup/refresh layout stays readable; captions and disconnected language replay are preserved')
