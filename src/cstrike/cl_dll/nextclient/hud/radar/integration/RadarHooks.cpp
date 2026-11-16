#include "RadarHooks.h"

#include <cmath>
#include <cstring>
#include <cstdio>
#include <vector>

#include "main.h"

#include "NativeSdk.h"
#include "../public/game_controls/iviewport.h"
#include <interface.h>
#include <vgui/IPanel.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Controls.h>
#include <vgui/IVGui.h>

#include "hud/radar/markers/RadarBroadcast.h"
#include "hud/radar/integration/RadarScoreboard.h"
#include "hud/radar/render/RadarView.h"
#include "hud/radar/render/text/HudTextBuffer.h"
#include "hud/radar/policy/SmokeRegistry.h"

namespace {
HudTextBuffer g_TextBuffer;
cl_enginefunc_t g_OriginalTextTable;
cl_enginefunc_t* g_PatchedEngTable;

void SetEngineTextColor(int r, int g, int b) {
    if (g_OriginalTextTable.pfnDrawSetTextColor) {
        constexpr float kColorByteScale = 255.0f;
        g_OriginalTextTable.pfnDrawSetTextColor(r / kColorByteScale, g / kColorByteScale, b / kColorByteScale);
    }
}

void Hooked_DrawSetTextColor(float r, float g, float b) {
    constexpr float kColorByteScale = 255.0f;
    g_TextBuffer.SetColor(static_cast<int>(r * kColorByteScale), static_cast<int>(g * kColorByteScale),
                          static_cast<int>(b * kColorByteScale));
    g_OriginalTextTable.pfnDrawSetTextColor(r, g, b);
}

int Hooked_DrawConsoleString(int x, int y, char* string) {
    int result = g_TextBuffer.DrawConsoleString(x, y, string);
    // DrawConsoleString resets the engine ambient text colour to con_color.
    const char* color = gEngfuncs.pfnGetCvarString("con_color");
    int r = 0;
    int g = 0;
    int b = 0;
    if (color && std::sscanf(color, "%d %d %d", &r, &g, &b) == 3) {
        g_TextBuffer.SetColor(r, g, b);
    }
    return result;
}

int Hooked_DrawString(int x, int y, const char* string, int r, int g, int b) {
    return g_TextBuffer.DrawString(x, y, string, r, g, b);
}

int Hooked_DrawStringReverse(int x, int y, const char* string, int r, int g, int b) {
    return g_TextBuffer.DrawStringReverse(x, y, string, r, g, b);
}

int Hooked_DrawCharacter(int x, int y, int ch, int r, int g, int b) {
    return g_TextBuffer.DrawCharacter(x, y, ch, r, g, b);
}

int Hooked_VGUI2DrawCharacter(int x, int y, int ch, unsigned int font) {
    return g_TextBuffer.VGUI2DrawCharacter(x, y, ch, font);
}

int Hooked_VGUI2DrawCharacterAdd(int x, int y, int ch, int r, int g, int b, unsigned int font) {
    return g_TextBuffer.VGUI2DrawCharacterAdd(x, y, ch, r, g, b, font);
}

RadarSinks g_Sinks;
struct ScoreboardUi {
    vgui2::IPanel* panels{};
    vgui2::ISurface* surface{};
    vgui2::IVGui* vgui{};
    vgui2::VPANEL panel{};
    vgui2::VPANEL root{};
};

void SolvePanelTree(vgui2::IPanel& panels, vgui2::VPANEL panel) {
    panels.Solve(panel);
    int child_count = panels.GetChildCount(panel);
    for (int i = 0; i < child_count; i++) {
        SolvePanelTree(panels, panels.GetChild(panel, i));
    }
}

bool FindScoreboard(ScoreboardUi& ui, std::uintptr_t tracked_panel = 0) {
    ui.surface = vgui2::surface();
    ui.panels = vgui2::ipanel();
    ui.vgui = vgui2::ivgui();
    if (!ui.surface || !ui.panels || !ui.vgui) {
        return false;
    }
    ui.root = ui.surface->GetEmbeddedPanel();
    if (!ui.root) {
        return false;
    }
    if (tracked_panel != 0) {
        ui.panel = ui.vgui->HandleToPanel(static_cast<vgui2::HPanel>(tracked_panel));
        if (ui.panel) {
            return true;
        }
    }
    int child_count = ui.panels->GetChildCount(ui.root);
    for (int i = 0; i < child_count; i++) {
        ui.panel = RadarScoreboard_FindVisiblePanel(*ui.panels, ui.panels->GetChild(ui.root, i));
        if (ui.panel) {
            return true;
        }
    }
    return false;
}

} // namespace

// The native client owns this engine-function table. Only its HUD text calls are
// buffered; radar labels use HudTextZone passthrough and keep their own positions.
void RadarHooks_PatchHudText() {
    cl_enginefunc_t* ef = &gEngfuncs;

    if (!ef || g_PatchedEngTable) {
        return;
    }

    g_OriginalTextTable = *ef;
    g_TextBuffer.Initialize(*ef, SetEngineTextColor);
    ef->pfnDrawSetTextColor = Hooked_DrawSetTextColor;
    ef->pfnDrawConsoleString = Hooked_DrawConsoleString;
    ef->pfnDrawString = Hooked_DrawString;
    ef->pfnDrawStringReverse = Hooked_DrawStringReverse;
    ef->pfnDrawCharacter = Hooked_DrawCharacter;
    ef->pfnVGUI2DrawCharacter = Hooked_VGUI2DrawCharacter;
    ef->pfnVGUI2DrawCharacterAdd = Hooked_VGUI2DrawCharacterAdd;
    g_PatchedEngTable = ef;
}

void RadarHooks_UnpatchHudText() {
    if (g_PatchedEngTable) {
        g_PatchedEngTable->pfnDrawSetTextColor = g_OriginalTextTable.pfnDrawSetTextColor;
        g_PatchedEngTable->pfnDrawConsoleString = g_OriginalTextTable.pfnDrawConsoleString;
        g_PatchedEngTable->pfnDrawString = g_OriginalTextTable.pfnDrawString;
        g_PatchedEngTable->pfnDrawStringReverse = g_OriginalTextTable.pfnDrawStringReverse;
        g_PatchedEngTable->pfnDrawCharacter = g_OriginalTextTable.pfnDrawCharacter;
        g_PatchedEngTable->pfnVGUI2DrawCharacter = g_OriginalTextTable.pfnVGUI2DrawCharacter;
        g_PatchedEngTable->pfnVGUI2DrawCharacterAdd = g_OriginalTextTable.pfnVGUI2DrawCharacterAdd;
    }

    g_TextBuffer.Reset();
    g_PatchedEngTable = nullptr;
}

void RadarHooks_SetSinks(const RadarSinks& sinks) {
    g_Sinks = sinks;
}

const RadarSinks& RadarHooks_Sinks() {
    return g_Sinks;
}

void RadarHooks_ClearSinks() {
    g_Sinks = RadarSinks{};
}

void RadarHooks_FlushText() {
    g_TextBuffer.Flush();
}

void RadarHooks_InvalidateGlyphMetrics() {
    g_TextBuffer.InvalidateGlyphMetrics();
}

bool RadarHooks_IsScoreboardVisible() {
    return gViewPortInterface && gViewPortInterface->IsScoreBoardVisible();
}

void RadarHooks_UpdateScoreboard(RadarScoreboard& placement, const RadarBox* radar) {
    if (!radar && !placement.is_shifted()) {
        return;
    }
    ScoreboardUi ui;
    // The stock client has multiple scoreboard panels with the same name.
    // Restore the exact panel we moved, even after it becomes hidden.
    if (!FindScoreboard(ui, placement.is_shifted() ? placement.get_panel() : 0)) {
        placement.Reset();
        return;
    }

    for (vgui2::VPANEL parent = ui.panel; parent && parent != ui.root; parent = ui.panels->GetParent(parent)) {
        if (!ui.panels->IsVisible(parent)) {
            radar = nullptr;
            break;
        }
    }

    int screen_w;
    int screen_h;
    int x;
    int y;
    int wide;
    int tall;
    int local_x;
    int local_y;
    ui.surface->GetScreenSize(screen_w, screen_h);
    ui.panels->Solve(ui.panel);
    ui.panels->GetAbsPos(ui.panel, x, y);
    ui.panels->GetSize(ui.panel, wide, tall);
    ui.panels->GetPos(ui.panel, local_x, local_y);

    ncl_math::Aabb2 radar_bounds{};
    if (radar) {
        // HUD coordinates can differ from VGUI pixels when the engine scales the HUD.
        float scale_x = (float)screen_w / radar->screen_w;
        float scale_y = (float)screen_h / radar->screen_h;
        radar_bounds = {{radar->x * scale_x, radar->y * scale_y},
                        {(radar->x + radar->size) * scale_x, (radar->y + radar->size) * scale_y}};
    }
    ncl_math::Vector2 pos = placement.Update(ui.vgui->PanelToHandle(ui.panel),
                                             {{(float)x, (float)y}, {(float)(x + wide), (float)(y + tall)}},
                                             {(float)screen_w, (float)screen_h}, radar ? &radar_bounds : nullptr);
    if (pos.x != (float)x || pos.y != (float)y) {
        ui.panels->SetPos(ui.panel, local_x + (int)pos.x - x, local_y + (int)pos.y - y);
        // Child clip rectangles and hit regions must follow in this frame as well.
        SolvePanelTree(*ui.panels, ui.panel);
    }
}

void RadarHooks_OnPlayer(int index, const float* position) {
    if (g_Sinks.broadcast) {
        g_Sinks.broadcast->SetPlayer(index, ncl_math::Vector3::Ref(position), gEngfuncs.GetClientTime());
    }
}

void RadarHooks_OnBombDrop(const float* position, bool planted) {
    if (g_Sinks.broadcast) {
        ncl_math::Vector3 pos = ncl_math::Vector3::Ref(position);
        float time = gEngfuncs.GetClientTime();
        if (pos == ncl_math::Vector3{}) {
            g_Sinks.broadcast->ClearBomb(time);
        } else {
            g_Sinks.broadcast->SetBomb(pos, planted, time);
        }
    }
}

void RadarHooks_OnBombPickup() {
    if (g_Sinks.broadcast) {
        g_Sinks.broadcast->ClearBomb(gEngfuncs.GetClientTime());
    }
}

void RadarHooks_OnHostage(int index, const float* position) {
    if (g_Sinks.broadcast) {
        g_Sinks.broadcast->SetHostage(index, ncl_math::Vector3::Ref(position), gEngfuncs.GetClientTime());
    }
}

void RadarHooks_OnHostageKilled(int index) {
    if (g_Sinks.broadcast) {
        g_Sinks.broadcast->KillHostage(index);
    }
}

void RadarHooks_OnSmoke(const float* origin, const float* angles, int detonation) {
    if (g_Sinks.smoke) {
        constexpr float kSmokeCenterThreshold = 1.0f;
        bool has_center = std::fabs(angles[0]) > kSmokeCenterThreshold ||
                          std::fabs(angles[1]) > kSmokeCenterThreshold || std::fabs(angles[2]) > kSmokeCenterThreshold;
        const float* exact = has_center ? angles : origin;
        g_Sinks.smoke->OnSmoke(ncl_math::Vector3::Ref(exact), has_center || detonation == 1, gEngfuncs.GetClientTime());
    }
}
