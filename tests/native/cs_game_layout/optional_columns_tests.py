#!/usr/bin/env python3
"""Check first-population optional columns using complete production scoreboard owners."""
from pathlib import Path
import hashlib
import json
import os
import subprocess

root = Path(__file__).resolve().parents[3]
out = Path(os.environ.get('CSE_OPTIONAL_COLUMNS_EVIDENCE',
                          root / 'build/smoked-game-layouts-20261115/optional-columns'))
out.mkdir(parents=True, exist_ok=True)
source_path = root / 'src/cstrike/cl_dll/VGUI/cstrikeclientscoreboard.cpp'
source = source_path.read_text(encoding='latin1')


def function(signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end] + '\n'


harness = r'''
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#define _snprintf snprintf
#define _snwprintf swprintf
namespace cse::ui {
bool glass = false;
bool ControlsSmokedGlass() { return glass; }
}
namespace cse::extstats { int DisplayAmount(int value) { return value; } }
struct Color { Color(int, int, int, int) {} };
struct Cvar { float value = 1; };
struct KeyValues {
    std::map<std::string, std::string> strings;
    std::map<std::string, int> integers;
    explicit KeyValues(const char*) {}
    void SetInt(const char* key, int value) { integers[key] = value; }
    void SetString(const char* key, const char* value) { strings[key] = value; }
    void SetWString(const char*, const wchar_t*) {}
    const char* GetString(const char* key, const char* fallback) {
        auto value = strings.find(key);
        return value == strings.end() ? fallback : value->second.c_str();
    }
    void deleteThis() { delete this; }
};
struct extra_player_info_t {
    int teamnumber = 2;
    int deaths = 0;
    int frags = 0;
    int sb_health = -1;
    int sb_account = -1;
    bool dead = true;
    bool has_c4 = false;
    bool vip = false;
    bool has_defuse_kit = false;
} g_PlayerExtraInfo[64];
struct PlayerInfo { const char* name = "dead CT"; int ping = 42; } g_PlayerInfoList[64];
int g_iTeamNumber = 2;
struct Entity { int index = 1; } localPlayer;
struct Engine {
    float pfnGetCvarFloat(const char*) { return 0; }
    const char* PlayerInfo_ValueForKey(int, const char*) { return "0"; }
    Entity* GetLocalPlayer() { return &localPlayer; }
} gEngfuncs;
struct VGuiLibraryPlayer_t {
    const char* name = nullptr;
    int teamnumber = 0;
    bool thisplayer = false;
};
struct Client {
    VGuiLibraryPlayer_t CallEnghudGetPlayerInfo(int index) {
        return index == 1 ? VGuiLibraryPlayer_t{"dead CT", 2, true} : VGuiLibraryPlayer_t{};
    }
    int GetMaxPlayers() { return 2; }
} client;
struct Viewport {
    Client* GetClientDllInterface() { return &client; }
    void MakeSafeName(const char* input, char* output, int capacity) {
        std::snprintf(output, static_cast<std::size_t>(capacity), "%s", input);
    }
    bool IsSpectatorGUIVisible() { return false; }
} viewport;
auto* gViewPortInterface = &viewport;
// Career output is outside this first multiplayer update; retain the complete owner unchanged.
struct ICareerTask {
    const wchar_t* GetLocalizedTaskName() { return nullptr; }
    bool IsComplete() { return false; }
    int GetRepeat() { return 0; }
    int GetPartial() { return 0; }
    bool IsMaxRoundTime() { return false; }
    bool MustSurvive() { return false; }
};
using ITaskVec = std::vector<ICareerTask*>;
using ITaskVecIt = ITaskVec::iterator;
struct Career {
    bool IsPlayingMatch() { return false; }
    ITaskVec* GetCurrentTaskVec() { throw std::runtime_error("unexpected career path"); }
    float GetRoundTimeLength() { return 0; }
} *g_pCareerUI = nullptr;
struct HUD { struct { float m_flTimeEnd = 0; } m_roundTimer; float m_flTime = 0; } gHUD;
struct TaskLabel {
    void SetText(const wchar_t*) {}
    void SizeToContents() {}
    void GetSize(int& width, int& height) { width = height = 0; }
};
void BufWPrintf(wchar_t*, int&, const wchar_t*, const wchar_t*, const wchar_t*) {}
namespace vgui2 {
struct Scheme { int GetProportionalScaledValue(int value) { return value; } } manager;
Scheme* scheme() { return &manager; }
struct Localize {
    void ConstructString(wchar_t*, int, const wchar_t*, int, ...) {}
    const wchar_t* Find(const char*) { return L""; }
    void ConvertANSIToUnicode(const char*, wchar_t*, int) {}
} localization;
Localize* localize() { return &localization; }
namespace smoked { int Pixels(int value) { return value; } }
}
struct PlayerList {
    std::map<int, KeyValues> rows;
    std::map<std::string, int> widths;
    void SetSectionFgColor(int, Color) {}
    void OverrideLineSpacing(int) {}
    int AddItem(int, KeyValues* row) { rows.insert_or_assign(0, *row); return 0; }
    void ModifyItem(int index, int, KeyValues* row) { rows.insert_or_assign(index, *row); }
    void SetItemFgColor(int, Color) {}
    void RemoveItem(int index) { rows.erase(index); }
    void SetSelectedItem(int) {}
    void SetSectionAlwaysVisible(int, bool) {}
    void ModifyColumn(int, const char* name, const wchar_t*, int width) {
        if (width >= 0) { widths[name] = width; }
    }
};
struct CCSClientScoreBoardDialog {
    enum { CSTRIKE_HEALTH_WIDTH = 41, CSTRIKE_ACCOUNT_WIDTH = 57 };
    PlayerList list;
    PlayerList* m_pPlayerList = &list;
    Cvar health, money, avatars;
    Cvar* m_pShowHealth = &health;
    Cvar* m_pShowMoney = &money;
    Cvar* m_pShowAvatars = &avatars;
    bool m_bHealthUpdateReceived = false;
    bool m_bMoneyUpdateReceived = false;
    bool m_bHealthColumnEnabled = false;
    bool m_bMoneyColumnEnabled = false;
    bool m_bAvatarColumnEnabled = true;
    int m_nAvatarColumnWidth = 0;
    int m_iSectionId = 0;
    int m_iPlayersToShow = 0;
    struct { int frags = 0; } m_TeamInfo[3];
    TaskLabel taskLabel;
    TaskLabel* m_pTaskLabel = &taskLabel;
    void UpdatePlayerInfo();
    bool GetPlayerScoreInfo(int, KeyValues*);
    void UpdateOptionalColumns();
    void UpdateColumnInAllSections(const char*, const wchar_t*, int, bool);
    Color GetTeamColor(int) { return Color(0, 0, 0, 255); }
    int GetAvatarHeaderWidth() { return 0; }
    int GetNameHeaderWidth() { return 500; }
    int GetHeaderWidth(int maximum, int minimum) { return std::max(maximum, minimum); }
    const wchar_t* GetHealthHeader() { return L"Health"; }
    const wchar_t* GetMoneyHeader() { return L"Money"; }
    void UpdateHeaders() {}
    void UpdatePlayerAvatar(int, KeyValues*) {}
    int FindItemIDForPlayerIndex(int playerIndex) {
        for (const auto& entry : list.rows) {
            if (entry.second.integers.at("playerIndex") == playerIndex) { return entry.first; }
        }
        return -1;
    }
};
'''
signatures = [
    'void CCSClientScoreBoardDialog::UpdateColumnInAllSections(',
    'void CCSClientScoreBoardDialog::UpdateOptionalColumns()',
    'bool CCSClientScoreBoardDialog::GetPlayerScoreInfo(',
    'void CCSClientScoreBoardDialog::UpdatePlayerInfo()',
]
for signature in signatures:
    harness += function(signature)
harness += r'''
void Require(bool passed, const char* message) {
    if (!passed) { throw std::runtime_error(message); }
}
int Width(PlayerList& list, const char* name) {
    const auto width = list.widths.find(name);
    return width == list.widths.end() ? 0 : width->second;
}
int main() {
    try {
        for (bool glass : {false, true}) {
            cse::ui::glass = glass;
            for (bool dead : {false, true}) {
                for (bool enabled : {false, true}) {
                    CCSClientScoreBoardDialog board;
                    board.health.value = enabled;
                    board.money.value = enabled;
                    auto& player = g_PlayerExtraInfo[1];
                    player.dead = dead;
                    player.sb_health = 100;
                    player.sb_account = 16000;
                    board.UpdatePlayerInfo();
                    Require(board.list.rows.size() == 1, "first update populates the real player row");
                    Require(board.m_bMoneyUpdateReceived, "production owner discovers money during population");
                    Require(board.m_bHealthUpdateReceived == !dead,
                        "dead-player health does not enable a health column");
                    const auto& row = board.list.rows.at(0);
                    Require((row.strings.count("account") != 0) == enabled,
                        "money preference controls account row content");
                    Require((row.strings.count("health") != 0) == (enabled && !dead),
                        "health preference and life state control health content");
                    Require(Width(board.list, "account") == (glass && enabled ? 57 : 0),
                        "first Smoked account value must have a nonzero column before paint");
                    Require(Width(board.list, "health") == (glass && enabled && !dead ? 41 : 0),
                        "first Smoked health value must have a nonzero column before paint");
                    board.UpdatePlayerInfo();
                    Require(Width(board.list, "account") == (enabled ? 57 : 0),
                        "Original keeps its existing second-update discovery; Smoked remains stable");
                    Require(Width(board.list, "health") == (enabled && !dead ? 41 : 0),
                        "health width remains stable on subsequent population");
                }
            }
            CCSClientScoreBoardDialog absent;
            g_PlayerExtraInfo[1].sb_health = -1;
            g_PlayerExtraInfo[1].sb_account = -1;
            absent.UpdatePlayerInfo();
            Require(!absent.m_bMoneyColumnEnabled && !absent.m_bHealthColumnEnabled,
                "missing optional stats keep both columns disabled");
        }
        std::puts("optional columns: first population, dead/live players, preferences, absent stats and Original passed");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
'''
(out / 'optional_columns.cpp').write_text(harness)
(out / 'source-receipt.json').write_text(json.dumps({
    'source': str(source_path.relative_to(root)),
    'sha256': hashlib.sha256(source_path.read_bytes()).hexdigest(),
    'completeFunctions': signatures,
}, indent=2) + '\n')
# The complete owner retains legacy sprintf calls in its unexercised career branch.
subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Wno-deprecated-declarations',
                '-fsanitize=address,undefined',
                str(out / 'optional_columns.cpp'), '-o', str(out / 'optional_columns')], check=True)
subprocess.run([str(out / 'optional_columns')], check=True)
