#pragma once

#include <string>

class RadarBroadcast;
class SmokeRegistry;
class RadarScoreboard;
class PlayersLayer;
struct RadarBox;
class RadarMapLayers;
class RadarLayerSelection;
class MapLayerCache;
class RadarLayerLifecycle;
struct RadarMapPreparationDiagnostic;

struct RadarSinks {
    SmokeRegistry* smoke{};
    RadarBroadcast* broadcast{};
    const PlayersLayer* players{};
    const RadarMapLayers* map_layers{};
    const RadarLayerSelection* layer_selection{};
    const MapLayerCache* layer_cache{};
    const RadarLayerLifecycle* layer_lifecycle{};
    const RadarMapPreparationDiagnostic* layer_preparation{};
    const std::string* layer_image_waiting_reason{};
    const char* map_name{};
    const char* correction_path{};
    const float* analysis_ms{};
};

void RadarHooks_SetSinks(const RadarSinks& sinks);
void RadarHooks_ClearSinks();
const RadarSinks& RadarHooks_Sinks();

// Call once the client table exists (HUD_Init); no-op if already patched.
void RadarHooks_PatchHudText();

void RadarHooks_UnpatchHudText();

// Call at the end of the stock HUD pass, after the hooked glyph draws.
void RadarHooks_FlushText();

// Call on VidInit: a video mode or UI scale change rebuilds the fonts.
void RadarHooks_InvalidateGlyphMetrics();

bool RadarHooks_IsScoreboardVisible();
void RadarHooks_UpdateScoreboard(RadarScoreboard& placement, const RadarBox* radar);

// Called after native message parsing, preserving the stock message path.
void RadarHooks_OnPlayer(int index, const float* position);
void RadarHooks_OnPlayerDeath(int index, const float* position);
void RadarHooks_OnBombDrop(const float* position, bool planted);
void RadarHooks_OnBombPickup();
void RadarHooks_OnHostage(int index, const float* position);
void RadarHooks_OnHostageKilled(int index);
void RadarHooks_OnSmoke(const float* origin, const float* angles, int detonation);
