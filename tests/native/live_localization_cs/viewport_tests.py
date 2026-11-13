#!/usr/bin/env python3
"""The actual viewport language dispatcher must not run spectator gameplay updates."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parents[3]
out = root / 'build/live-localization-menu-crash/viewport-owner'
out.mkdir(parents=True, exist_ok=True)
source = (root / 'src/cstrike/cl_dll/VGUI/counterstrikeviewport.cpp').read_text(encoding='cp1252')
start = source.index('void CounterStrikeViewport::RefreshLanguage()')
opening = source.index('{', start)
end = opening + 1
depth = 1
while depth:
    depth += (source[end] == '{') - (source[end] == '}')
    end += 1
body = source[start:end]
harness = r'''
#include <array>
#include <cassert>
#include <cstdlib>

constexpr int MAX_PLAYERS = 32;
struct Presentation {
    int refreshes = 0;
    void RefreshLanguage() { ++refreshes; }
};
struct {
    Presentation m_Message;
    Presentation m_Menu;
    Presentation m_StatusBar;
    Presentation m_VGUI2Print;
    Presentation m_SayText;
} gHUD;
struct TeamFortressViewport {
    int baseRefreshes = 0;
    void RefreshLanguage() { ++baseRefreshes; }
};
struct CCareerBaseBox {
    static inline int refreshes = 0;
    static void RefreshOpenBoxLanguage() { ++refreshes; }
};
struct { const char* location = "#Literal player location"; } g_PlayerExtraInfo[MAX_PLAYERS + 1];
struct VoiceHud {
    int refreshes = 0;
    void UpdateLocation(int player, const char* value) {
        assert(player > 0 && player <= MAX_PLAYERS);
        assert(value == g_PlayerExtraInfo[player].location);
        ++refreshes;
    }
} voice;
VoiceHud* GetClientVoiceHud() { return &voice; }
struct CounterStrikeViewport : TeamFortressViewport {
    bool m_bInitialized = false;
    bool hasLocalPlayer = false;
    int gameplayUpdates = 0;
    int health = 73;
    int money = 4200;
    int spectatorMode = 4;
    int spectatorTarget = 7;
    float hudDeadline = 30.5f;
    bool popupVisible = true;
    int selectedItem = 2;
    Presentation presentation;
    Presentation* m_pTeamMenu = &presentation;
    Presentation* m_pPrivateCSClientScoreBoard = &presentation;
    Presentation* m_pPrivateSpectatorGUI = &presentation;
    Presentation* m_pBuyMenu = &presentation;
    Presentation* m_pBuyPresetMainMenu = &presentation;
    Presentation* m_pBuyPresetEditMenu = &presentation;
    Presentation* m_pCareerRoundEndMenu = &presentation;
    Presentation* m_pCareerMatchEndMenu = &presentation;
    void RefreshLanguage();
    void UpdateSpectatorPanel() {
        ++gameplayUpdates;
        // The real CheckSettings dereferences the absent local entity here.
        if (!hasLocalPlayer) {
            std::abort();
        }
        spectatorMode = 0;
    }
};
'''
harness += body + r'''
int main() {
    CounterStrikeViewport viewport;
    viewport.RefreshLanguage();
    assert(viewport.baseRefreshes == 0 && viewport.presentation.refreshes == 0);
    viewport.m_bInitialized = true;
    for (bool localPlayer : {false, true, false}) {
        viewport.hasLocalPlayer = localPlayer;
        const int before = viewport.presentation.refreshes;
        viewport.RefreshLanguage();
        assert(viewport.presentation.refreshes == before + 8);
        assert(viewport.gameplayUpdates == 0);
        assert(viewport.health == 73 && viewport.money == 4200);
        assert(viewport.spectatorMode == 4 && viewport.spectatorTarget == 7);
        assert(viewport.hudDeadline == 30.5f && viewport.popupVisible && viewport.selectedItem == 2);
    }
    assert(viewport.baseRefreshes == 3 && CCareerBaseBox::refreshes == 3);
    assert(voice.refreshes == 3 * MAX_PLAYERS);
    viewport.m_pPrivateSpectatorGUI = nullptr;
    viewport.RefreshLanguage();
    assert(viewport.gameplayUpdates == 0);
}
'''
(out / 'viewport.cpp').write_text(harness)
subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                str(out / 'viewport.cpp'), '-o', str(out / 'viewport')], check=True)
subprocess.run([str(out / 'viewport')], check=True,
               env=dict(os.environ, UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
print('PASS: main-menu, active-game and post-disconnect presentation dispatch skips gameplay updates')
