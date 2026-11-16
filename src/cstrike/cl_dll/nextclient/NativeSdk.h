#pragma once

// Use the owning hardware renderer definitions: the legacy SDK com_model.h
// describes software surfaces and cannot read this runtime's brush models.
#define NormalizeAngles NativeRadar_UnusedEngineNormalizeAngles
#include "../../../engine/gl_model.h"
#undef NormalizeAngles
#undef VectorCopy
#undef VectorClear
#undef VectorAdd
#undef VectorSubtract
#undef DotProduct
#include "hud.h"

#include "cl_entity.h"
#include "event_api.h"
#include "triangleapi.h"
#include "screenfade.h"
#include "../../../pm_shared/pm_defs.h"
#include "pm_movevars.h"
#include "pmtrace.h"
#include "pm_shared.h"

#undef clamp
#undef min
#undef max
#undef fabs
#include "event_args.h"
#include "cvardef.h"

extern playermove_t* pmove;

inline constexpr int MAX_HOSTAGES = 24;
inline constexpr int TEAM_UNASSIGNED = 0;
inline constexpr int TEAM_TERRORIST = 1;
inline constexpr int TEAM_CT = 2;
inline constexpr int TEAM_SPECTATOR = 3;

int NativeRadar_MessageNumber();
const int (*NativeRadar_Lightmaps())[64];
extern float g_MainViewOrigin[3];
extern float g_MainViewAngles[3];

inline constexpr float PM_VEC_VIEW = 17.0f;
inline constexpr float PM_VEC_DUCK_VIEW = 12.0f;

#define ScreenWidth (gHUD.m_scrinfo.iWidth)
#define ScreenHeight (gHUD.m_scrinfo.iHeight)

inline bool IsValidClientIndex(int player) {
    return player >= 1 && player <= MAX_PLAYERS && player <= gEngfuncs.GetMaxClients();
}

std::uint64_t NativeRadar_GraphicsGeneration();

namespace cse::radar {
struct Glyph;
}
bool NativeRadar_GetGlyph(std::uint32_t font, std::uint32_t character, cse::radar::Glyph& out);

#include "../../../public/radar_renderer_api.h"
const CseRadarRendererApi* NativeRadar_Renderer();
CseRadarState NativeRadar_RendererState();
void NativeRadar_ReportRenderResult(const char* operation, CseRadarResult result);

CseRadarHudSpace NativeRadar_HudSpace();
