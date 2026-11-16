#include "HudRadarAdvanced.h"

#include <cstdlib>
#include <cstdio>
#include <memory>
#include "../public/game_controls/iviewport.h"
#include <cstring>
#include <algorithm>
#include <chrono>
#include <string>
#include <string_view>
#include <utility>

#include <vector>

#include "main.h"
#include "NativeSdk.h"
#include "pm_movevars.h"
#include "vgui_background_controls.h"

#include "hud/radar/integration/Performance.h"
#include "NativeRadar.h"
#include "hud/radar/integration/graphics_retirement_hooks.h"
#include <SDL3/SDL_video.h>

#include <ncl_gl/context.h>
#include <ncl_gl/loader.h>
#include <ncl_math/vec3.h>
#include <ncl_math/color.h>
#include "radar_cvars.h"

#include "hud/radar/map/Overview.h"
#include "hud/radar/map/MapBspGeometry.h"
#include "hud/radar/map/layers/MapLayerOverrides.h"
#include "hud/radar/integration/RadarUtil.h"
#include "hud/radar/integration/RadarConfig.h"
#include "hud/radar/integration/RadarMapReadiness.h"
#include "hud/radar/RadarLimits.h"
#include "hud/radar/render/RadarDraw.h"
#include "hud/radar/render/text/RadarText.h"
#include "hud/radar/policy/SmokeRegistry.h"
#include "hud/radar/integration/RadarHooks.h"
#include "hud/radar/render/text/HudTextZone.h"
#include "hud/radar/render/RadarFade.h"
#include "hud/radar/integration/RadarDebug.h"
#include "hud/radar/markers/RadarBroadcast.h"
#include "view.h"

namespace {
constexpr cvars::CvarDefinition kScaleCvar = radar::FindCvar("hud_radar_scale");
constexpr cvars::CvarDefinition kMapScaleCvar = radar::FindCvar("hud_radar_map_scale");
static_assert(kScaleCvar.name && kMapScaleCvar.name);

static_assert(static_cast<int>(RadarTeam::Unassigned) == TEAM_UNASSIGNED);
static_assert(static_cast<int>(RadarTeam::Terrorist) == TEAM_TERRORIST);
static_assert(static_cast<int>(RadarTeam::CounterTerrorist) == TEAM_CT);
static_assert(static_cast<int>(RadarTeam::Spectator) == TEAM_SPECTATOR);

constexpr float kRunSpeedFallback = 320.0f;
constexpr int kDuckHull = 1;
constexpr int kPlayerHullCount = 4;

// Set by Draw, read later in the same frame by the classic radar.
bool g_MapUnavailable;

void LogPolicyMessage(const char* message) {
    gEngfuncs.Con_Printf("radar: %s\n", message);
}

// Optional server-set mode hint, broadcast as "serverinfo nc_ffa 1".
const char* GetServerInfoValue(const char* key) {
    const char* value = gEngfuncs.ServerInfo_ValueForKey(key);
    return value ? value : "";
}

// Values: "marker" re-purposes the message, the stock name ("bomb"/"hostage") pins it.
MessageHint ParseMessageHint(const char* value, const char* stock_name) {
    if (stricmp(value, "marker") == 0) {
        return MessageHint::Marker;
    }

    if (stricmp(value, stock_name) == 0) {
        return MessageHint::Stock;
    }

    return MessageHint::None;
}

} // namespace

HudRadarAdvanced::HudRadarAdvanced()
    : draw_(performance_), visibility_(smoke_, performance_), text_(draw_, performance_),
      backdrop_(draw_, performance_, smoke_), players_(draw_, performance_, visibility_, policy_, broadcast_, text_),
      objectives_(draw_, performance_, visibility_, policy_, broadcast_, text_) {
    policy_.SetLogger(LogPolicyMessage);

    RadarSinks sinks{};
    sinks.smoke = &smoke_;
    sinks.broadcast = &broadcast_;
    sinks.players = &players_;
    sinks.map_layers = &map_layers_;
    sinks.layer_selection = &layer_context_.get_selection();
    sinks.layer_cache = &backdrop_.get_overview().get_layer_cache();
    sinks.layer_lifecycle = &layer_lifecycle_;
    sinks.map_name = layer_map_;
    sinks.correction_path = layer_correction_path_;
    sinks.analysis_ms = &layer_analysis_ms_;
    sinks.layer_preparation = &layer_preparation_;
    sinks.layer_image_waiting_reason = &backdrop_.get_overview().get_layer_waiting_reason();
    RadarHooks_SetSinks(sinks);
}

HudRadarAdvanced::~HudRadarAdvanced() {
    Shutdown();
}

void* HudRadarAdvanced::CurrentContext() const {
    return SDL_GL_GetCurrentContext();
}

void* HudRadarAdvanced::CurrentWindow() const {
    return SDL_GL_GetCurrentWindow();
}

void HudRadarAdvanced::ResetGraphics(bool release_owned) {
    bool release =
        release_owned && radar::integration::CanReleaseGraphics(graphics_generation_, NativeRadar_GraphicsGeneration(),
                                                                draw_.is_context(CurrentContext(), CurrentWindow()));
    backdrop_.OnVidInit(release);
    draw_.ResetGl(release);
    text_.ResetGl();
    RadarHooks_InvalidateGlyphMetrics();
    graphics_generation_ = 0;
}

bool HudRadarAdvanced::PrepareGraphics() {
    void* context = CurrentContext();
    void* window = CurrentWindow();
    if (!context) {
        return false;
    }
    std::uint64_t generation = NativeRadar_GraphicsGeneration();
    if (graphics_generation_ != generation || !draw_.is_context(context, window)) {
        backdrop_.OnVidInit(false);
        draw_.ResetGl(false);
        text_.ResetGl();
        RadarHooks_InvalidateGlyphMetrics();
        // Windows GL entry points are context-specific. Resolve them before preparing
        // shaders or map textures, and retry after failures with a valid current context.
        if (!ncl_gl::Reload()) {
            return false;
        }
        draw_.Prepare(context, window);
        graphics_generation_ = generation;
    }
    return context != nullptr;
}

void HudRadarAdvanced::Shutdown() {
    if (shutdown_) {
        return;
    }
    shutdown_ = true;
    RadarHooks_ClearSinks();
    RadarHooks_UpdateScoreboard(scoreboard_, nullptr);
    layer_analysis_.Shutdown();
    ResetGraphics();
}

void HudRadarAdvanced::OnPlayerRespawn(PlayersLayer::ResetReason reason, float previous_time, float time) {
    layer_context_.Reset();
    visibility_.Reset();
    broadcast_.ResetPlayers();
    players_.OnPlayerRespawn(reason, previous_time, time);
    objectives_.OnPlayerRespawn();
}

void HudRadarAdvanced::VidInit() {
    // Map changes preserve the context; mode changes advance the engine generation.
    ResetGraphics();
    ResetMapLayers();
    players_.OnVidInit();
    objectives_.OnVidInit();
    RadarHooks_InvalidateGlyphMetrics();
    smoke_.Reset();
    broadcast_.Reset(gEngfuncs.GetClientTime());
    visibility_.Reset();
    policy_.OnLevelChange();
    last_hint_read_ = 0.0f;
    OnPlayerRespawn(PlayersLayer::ResetReason::Video);
}

void HudRadarAdvanced::InitHUDData() {
    PrepareGraphics();
    ResetMapLayers();
    OnPlayerRespawn(PlayersLayer::ResetReason::InitHud);
}

void HudRadarAdvanced::Reset() {
    OnPlayerRespawn(); // gmsgResetHUD: also fires on a mid-round respawn
}

RadarBox HudRadarAdvanced::ComputeBox(bool spectating) {
    int screen_w = 0;
    int screen_h = 0;
    screen_w = ScreenWidth;
    screen_h = ScreenHeight;

    float scale = std::clamp(g_RadarCfg.scale->value, kScaleCvar.min_value, kScaleCvar.max_value);
    bool fullmap = g_RadarCfg.fullmap->value != 0.0f && RadarHooks_IsScoreboardVisible();

    return RadarView_ComputeBox(static_cast<float>(screen_w), static_cast<float>(screen_h), scale, spectating, fullmap,
                                cse::ui::ControlsSmokedGlass());
}

RadarTransform HudRadarAdvanced::ComputeTransform(const RadarBox& box, const RadarShape& shape, bool scoreboard) {
    const RadarOverview& overview = backdrop_.get_overview();
    const overview_t& ov = overview.get_data();

    RadarViewParams p;
    p.ov_zoom = ov.zoom;
    p.ov_origin = {ov.origin[0], ov.origin[1]};
    p.ov_rotated = ov.rotated != 0;
    p.map_scale = std::clamp(g_RadarCfg.map_scale->value, kMapScaleCvar.min_value, kMapScaleCvar.max_value);
    p.map_shift = g_RadarCfg.map_shift->value != 0.0f;
    p.rotate = g_RadarCfg.rotate->value != 0.0f;
    p.fullmap_enabled = g_RadarCfg.fullmap->value != 0.0f;
    p.scoreboard_visible = scoreboard;
    p.round = shape.round;
    p.view_origin = {g_MainViewOrigin[0], g_MainViewOrigin[1]};
    p.view_yaw = g_MainViewAngles[1];
    p.sprite_frames = overview.get_sprite() ? overview.get_sprite()->numframes : 0;

    if (overview.is_runtime()) {
        p.map_bounds = overview.get_runtime_map().bounds;
    } else if (model_t* world = gEngfuncs.hudGetModelByIndex(1)) {
        p.map_bounds = ncl_math::Aabb2{{world->mins[0], world->mins[1]}, {world->maxs[0], world->maxs[1]}};
    }

    return RadarView_ComputeTransform(box, p);
}

bool HudRadarAdvanced::ShouldDraw() {
    // Before the observer short-circuit below: 'hideradar' is a standing instruction, not
    // per-life state, so observing must not bring the radar back after it was turned off.
    if (!gHUD.m_Health.m_bDrawRadar) {
        return false;
    }

    bool spectating = g_iUser1 != 0 || gEngfuncs.IsSpectateOnly();

    if (spectating) {
        float inset = gEngfuncs.pfnGetCvarFloat("spec_pip");
        return g_iUser1 != OBS_MAP_FREE && g_iUser1 != OBS_MAP_CHASE && inset != INSET_MAP_FREE &&
               inset != INSET_MAP_CHASE;
    }

    if (gHUD.m_iHideHUDDisplay & HIDEHUD_HEALTH) {
        return false;
    }

    // The suit bit is set on every spawn, mid-round CSDM respawns included. Not m_fPlayerDead:
    // the stock client freezes it while observing in first person and clears it only on the
    // next CurWeapon, so it can stay stale-dead after a respawn and wrongly hide the radar.
    if (!(gHUD.m_iWeaponBits & (1U << WEAPON_SUIT))) {
        return false;
    }

    // On mp_fadetoblack 1 the just-killed player stays on the corpse cam with g_iUser1 still
    // 0, so without this gate the radar paints live positions over the black fade.
    if (gHUD.m_Health.m_iHealth <= 0) {
        return false;
    }

    return true;
}

void HudRadarAdvanced::UpdateFrameContext(bool spectating, float flTime) {
    performance::PerformanceCpuScope scope(performance_.get_monitor(), performance::PerfStage::RadarPolicy);
    ctx_ = FrameContext{};
    ctx_.message_num = NativeRadar_MessageNumber();
    ctx_.spectating = spectating;
    ctx_.free_camera = spectating && RadarView_IsFreeCamera(g_iUser1);

    if (cl_entity_t* local = gEngfuncs.GetLocalPlayer()) {
        ctx_.local = local;
        ctx_.self_idx = local->index;
        ctx_.highlight_idx = local->index;

        if (ctx_.spectating && g_iUser1 != OBS_ROAMING && IsValidClientIndex(g_iUser2)) {
            ctx_.highlight_idx = g_iUser2;
            ctx_.observed_idx = g_iUser2;
        }

        if (IsValidClientIndex(ctx_.highlight_idx)) {
            ctx_.highlight_team = RadarTeam_From(g_PlayerExtraInfo[ctx_.highlight_idx].teamnumber);
        }

        ctx_.local_team = RadarTeam_From(g_PlayerExtraInfo[ctx_.self_idx].teamnumber);
    }

    if (!RadarView_ShouldHighlightPlayer(g_iUser1)) {
        ctx_.highlight_idx = 0;
    }

    // Runs here: the visibility pass below builds its sets from the policy's teammate answer.
    UpdatePolicy(flTime);

    if (ctx_.local) {
        visibility_.Update(ctx_.self_idx, ctx_.highlight_team, ctx_.spectating, flTime, policy_);
    }
}

void HudRadarAdvanced::UpdatePolicy(float flTime) {
    policy_.SetLogging(RadarDebug_IsEnabled());
    policy_.SetForceAllyGating(g_RadarCfg.allies->value == 0.0f);

    const RadarMapObjectives& map = objectives_.get_map_objectives();
    policy_.SetMapObjectives(map.get_site_count(), map.get_hostage_count());

    if (flTime - last_hint_read_ >= kHintInterval || flTime < last_hint_read_) {
        last_hint_read_ = flTime;
        policy_.SetServerFfaHint(std::atoi(GetServerInfoValue("nc_ffa")) != 0);
        policy_.SetBombHint(ParseMessageHint(GetServerInfoValue("nc_bombmsg"), "bomb"));
        policy_.SetHostageHint(ParseMessageHint(GetServerInfoValue("nc_hostagemsg"), "hostage"));
    }

    float run_speed = (pmove && pmove->movevars) ? pmove->movevars->maxspeed : kRunSpeedFallback;
    bool applicable = ctx_.local != nullptr && !ctx_.spectating && ctx_.local_team != RadarTeam::Unassigned;
    policy_.BeginFrame(flTime, run_speed, applicable);

    // Classified for spectators too: what a message means does not depend on who watches.
    const RadarBroadcast::Bomb& bomb = broadcast_.get_bomb();
    policy_.ObserveBomb({bomb.seq, bomb.active, bomb.pos, bomb.planted, bomb.time}, flTime);

    RadarBroadcast::Hostage hostages[RadarPolicy::kMaxHostages];
    int hostage_count = broadcast_.CollectHostages(hostages);

    // HostagePos reaches living Counter-Terrorists only: nobody else may read its absence.
    policy_.ObserveHostageBroadcast(!ctx_.spectating && ctx_.local_team == RadarTeam::CounterTerrorist, hostage_count);

    for (int i = 0; i < hostage_count; i++) {
        policy_.ObserveHostage({hostages[i].index, hostages[i].seq, hostages[i].time}, flTime);
    }

    if (!applicable) {
        return;
    }

    int message_num = ctx_.message_num;
    int max_clients = std::min<int>(MAX_PLAYERS, gEngfuncs.GetMaxClients());

    for (int i = 1; i <= max_clients; i++) {
        if (i == ctx_.self_idx) {
            continue;
        }

        extra_player_info_t& ext = g_PlayerExtraInfo[i];

        if (ext.dead || RadarTeam_From(ext.teamnumber) != ctx_.local_team) {
            continue;
        }

        cl_entity_t* ent = gEngfuncs.GetEntityByIndex(i);
        bool live = ent && ent->player && ent->curstate.messagenum == message_num;
        ncl_math::Vector3 live_world = live ? RadarFrame_GetPlayerPosition(*ent) : ncl_math::Vector3{};
        ncl_math::Vector2 live_pos{live_world.x, live_world.y};

        policy_.ObserveAlly(i, live, live_pos, ncl_math::Vector3::Ref(ext.origin));
    }
}

void HudRadarAdvanced::ReportLayerFailure(RadarMapMode mode, int layer) {
    std::string_view reason = layer_lifecycle_.get_failure_reason();
    gEngfuncs.Con_Printf("radar: layers fallback map=%s mode=%d layer=%d reason=%.*s\n", layer_map_,
                         static_cast<int>(mode), layer, (int)reason.size(), reason.data());
}

void HudRadarAdvanced::ResetMapLayers() {
    layer_analysis_.Reset();
    layer_context_.Reset();
    backdrop_.OnVidInit();
    objectives_.ResetLayerCache();
    map_layers_.Reset();
    layer_lifecycle_.Reset();

    layer_map_[0] = 0;
    layer_correction_path_[0] = 0;
    layer_analysis_ms_ = 0.0f;
    layer_preparation_ = {};
    map_mode_ = RadarMapMode::None;
    has_layer_frame_time_ = false;
}

RadarMapStatus HudRadarAdvanced::EnsureMapLayers(RadarMapMode mode) {
    char map_name[64]{};
    const char* level_name = gEngfuncs.pfnGetLevelName();

    if (level_name) {
        RadarUtil_ExtractMapName(level_name, map_name);
    }

    if (std::strcmp(layer_map_, map_name) != 0) {
        ResetMapLayers();
        std::snprintf(layer_map_, sizeof(layer_map_), "%s", map_name);
        std::snprintf(layer_correction_path_, sizeof(layer_correction_path_), "overviews/%s_layers.txt", map_name);
    }

    if (map_mode_ != mode) {
        layer_context_.Reset();
        backdrop_.ResetLayerImages();

        if (!layer_lifecycle_.is_layout_ready() &&
            !(layer_analysis_.is_pending() && RadarLayerContext_IsLayeredMode(mode) &&
              RadarLayerContext_IsLayeredMode(map_mode_))) {
            layer_analysis_.Reset();
            objectives_.ResetLayerCache();
            map_layers_.Reset();
            layer_lifecycle_.Reset();
            layer_preparation_ = {};
        } else {
            layer_lifecycle_.ResetImage();
        }

        map_mode_ = mode;
    }

    layer_preparation_.mode = static_cast<int>(mode);
    layer_preparation_.waiting_reason.clear();

    if (!RadarLayerContext_IsLayeredMode(mode)) {
        return RadarMapStatus::Ready;
    }

    if (!map_name[0]) {
        layer_preparation_.waiting_reason =
            "stage=map_name level_name=" + std::string(level_name ? level_name : "missing");

        return RadarMapStatus::Waiting;
    }

    if (std::unique_ptr<RadarLayerAnalysisResult> result = layer_analysis_.Poll()) {
        objectives_.ResetLayerCache();
        map_layers_ = std::move(result->map);
        layer_analysis_ms_ = float(result->duration_ns) / 1000000.0f;

        if (!result->warning.empty()) {
            gEngfuncs.Con_Printf("radar: %s: %s\n", layer_correction_path_, result->warning.c_str());
        }

        if (layer_lifecycle_.SetGeometryStatus(result->status, result->failure_reason)) {
            ReportLayerFailure(mode, -1);
        }
    }

    if (layer_analysis_.is_pending()) {
        layer_preparation_.waiting_reason = "stage=analysis_worker state=scheduled";

        return RadarMapStatus::Waiting;
    }

    if (layer_lifecycle_.should_build_geometry()) {
        ++layer_preparation_.attempts;

        model_t* world = gEngfuncs.hudGetModelByIndex(1);
        event_api_s* events = gEngfuncs.pEventAPI;

        if (!world) {
            layer_preparation_.waiting_reason = "stage=world_model missing=model_index_1";

            return RadarMapStatus::Waiting;
        }

        if (!events) {
            layer_preparation_.waiting_reason = "stage=event_api missing=pEventAPI";

            return RadarMapStatus::Waiting;
        }

        if (!events->EV_LocalPlayerBounds) {
            layer_preparation_.waiting_reason = "stage=crouch_bounds missing=EV_LocalPlayerBounds";

            return RadarMapStatus::Waiting;
        }

        ncl_math::Aabb3 crouch_bounds{};
        events->EV_LocalPlayerBounds(kDuckHull, crouch_bounds.min.data(), crouch_bounds.max.data());

        if (!pmove) {
            layer_preparation_.waiting_reason = "stage=crouch_hull missing=playermove";

            return RadarMapStatus::Waiting;
        }

        const hull_t* hull =
            MapBspGeometry_SelectPlayerHull(*pmove, *events, *world, kDuckHull, &layer_preparation_.waiting_reason);

        if (!hull) {
            return RadarMapStatus::Waiting;
        }

        const auto capture_start = std::chrono::steady_clock::now();
        RadarMapGeometry geometry;

        performance::PerformanceCpuScope capture(performance_.get_monitor(), performance::PerfStage::RadarCapture);

        if (!MapBspGeometry_Collect(*world, *hull, crouch_bounds, geometry, &layer_preparation_.waiting_reason)) {
            return RadarMapStatus::Waiting;
        }

        std::optional<RadarLayerOverrides> overrides;
        char* path = layer_correction_path_;
        int length = 0;
        char* file = reinterpret_cast<char*>(gEngfuncs.COM_LoadFile(path, 5, &length));

        if (file) {
            std::vector<std::string> tokens;
            char token[kComParseTokenSize];
            char* cursor = file;

            while (cursor) {
                token[0] = 0;
                cursor = gEngfuncs.COM_ParseFile(cursor, token);

                if (token[0]) {
                    tokens.emplace_back(token);
                }
            }

            gEngfuncs.COM_FreeFile(file);

            RadarLayerOverrides parsed;
            std::string error;

            if (MapLayerOverrides_Parse(tokens, parsed, error)) {
                overrides.emplace(std::move(parsed));
            } else {
                gEngfuncs.Con_Printf("radar: %s: %s\n", path, error.c_str());
            }
        }

        layer_preparation_.capture_ms =
            std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - capture_start).count();
        layer_analysis_.Start(std::move(geometry), std::move(overrides));
        layer_preparation_.waiting_reason = "stage=analysis_worker state=scheduled";
    }

    if (layer_lifecycle_.is_layout_ready()) {
        return RadarMapStatus::Ready;
    }

    return layer_lifecycle_.should_use_fallback() ? RadarMapStatus::Failed : RadarMapStatus::Waiting;
}

RadarMapStatus HudRadarAdvanced::EnsureMapLayout(RadarMapMode mode, bool prepare_only) {
    performance::PerformanceCpuScope scope(performance_.get_monitor(), performance::PerfStage::RadarLayout);
    RadarMapStatus status = EnsureMapLayers(mode);

    if (RadarLayerContext_IsLayeredMode(mode)) {
        return RadarMapReadiness_PrepareLayered(backdrop_, mode, &map_layers_, status,
                                                layer_lifecycle_.should_use_fallback(), prepare_only);
    }

    return backdrop_.EnsureMap(mode, nullptr, prepare_only);
}

void HudRadarAdvanced::UpdateLayerContext(bool spectating, float time) {
    cl_entity_t* local = gEngfuncs.GetLocalPlayer();
    RadarLayerSource source = RadarLayerContext_ChooseSource(g_iUser1, local ? local->index : 0, g_iUser2, spectating);

    RadarLayerObservation observation{};
    observation.time = time;
    observation.source = source.entity;
    observation.free_camera = source.camera;

    float sample_time = time;
    float speed = pmove && pmove->movevars ? std::max(kRunSpeedFallback, pmove->movevars->maxspeed) : kRunSpeedFallback;

    if (source.camera) {
        observation.feet = ncl_math::Vector3::Ref(g_MainViewOrigin);
        observation.valid = true;

        if (pmove && pmove->movevars) {
            speed = std::max(speed, pmove->movevars->spectatormaxspeed);
        }
    } else {
        cl_entity_t* entity = source.entity > 0 ? gEngfuncs.GetEntityByIndex(source.entity) : nullptr;

        if (entity && entity->player && local &&
            (entity == local || entity->curstate.messagenum == NativeRadar_MessageNumber())) {
            int hull = entity == local && !spectating && pmove ? pmove->usehull : entity->curstate.usehull;

            if (hull >= 0 && hull < kPlayerHullCount) {
                ncl_math::Aabb3 bounds{};
                gEngfuncs.pEventAPI->EV_LocalPlayerBounds(hull, bounds.min.data(), bounds.max.data());

                bool predicted = entity == local && !spectating;
                ncl_math::Vector3 origin = predicted ? ncl_math::Vector3::Ref(gHUD.m_vecOrigin)
                                                     : ncl_math::Vector3::Ref(entity->curstate.origin);

                observation.feet = RadarLayerContext_Feet(origin, bounds.min);
                observation.valid = true;
                observation.grounded = predicted && pmove && pmove->onground >= 0;
                observation.discontinuity = (entity->curstate.effects & EF_NOINTERP) != 0;

                if (!predicted) {
                    sample_time = entity->curstate.msg_time;
                }

                ncl_math::Vector3 velocity = predicted && pmove ? ncl_math::Vector3::Ref(pmove->velocity)
                                                                : ncl_math::Vector3::Ref(entity->curstate.velocity);
                ncl_math::Vector3 base_velocity = predicted && pmove
                                                      ? ncl_math::Vector3::Ref(pmove->basevelocity)
                                                      : ncl_math::Vector3::Ref(entity->curstate.basevelocity);
                speed = std::max(speed, (velocity + base_velocity).Length());

                if (pmove && (observation.grounded || !predicted)) {
                    std::optional<RadarLayerContact> contact =
                        RadarLayerContext_TraceSupport(*gEngfuncs.pEventAPI, *pmove, observation.feet, bounds);

                    if (contact) {
                        observation.support = contact->support;
                    }

                    if (!predicted) {
                        observation.grounded = contact && contact->grounded;
                    }
                }
            }
        }
    }

    layer_context_.Update(map_layers_, observation, sample_time, speed);
}

void HudRadarAdvanced::PrepareHudTextZone() {
    g_MapUnavailable = false;

    bool should_draw = RadarConfig_IsAdvancedSelected() && ShouldDraw();
    radar::integration::CpuScope scope(performance_.get_monitor(), performance::PerfStage::RadarPrepare, should_draw);

    if (!PrepareGraphics() || !RadarConfig_CheckAdvancedActive(draw_) || !should_draw) {
        return;
    }

    if (EnsureMapLayout(RadarMapMode_FromCvar(g_RadarCfg.map_mode->value), true) != RadarMapStatus::Ready) {
        g_MapUnavailable = true;
        return;
    }

    RadarBox box = ComputeBox(g_iUser1 != 0 || gEngfuncs.IsSpectateOnly());
    HudTextZone_SetZone(box.x, box.y, box.size);
}

void HudRadarAdvanced::UpdateScoreboard(bool hud_visible) {
    if (hud_visible && PrepareGraphics() && RadarConfig_CheckAdvancedActive(draw_) && ShouldDraw() &&
        !g_MapUnavailable && g_RadarCfg.shift_scoreboard->value != 0.0f) {
        RadarBox box = ComputeBox(g_iUser1 != 0 || gEngfuncs.IsSpectateOnly());
        RadarHooks_UpdateScoreboard(scoreboard_, &box);
    } else {
        RadarHooks_UpdateScoreboard(scoreboard_, nullptr);
    }
}

void HudRadarAdvanced::Draw(float flTime) {
    g_MapUnavailable = false;

    bool should_draw = RadarConfig_IsAdvancedSelected() && ShouldDraw();
    radar::integration::CpuScope scope(performance_.get_monitor(), performance::PerfStage::RadarDraw, should_draw);

    if (!PrepareGraphics() || !RadarConfig_CheckAdvancedActive(draw_)) {
        return;
    }

    if (!should_draw) {
        return;
    }

    draw_.ResetStats();

    if (has_layer_frame_time_) {
        flTime = RadarFrame_NormalizeTime(flTime, layer_frame_time_);
    }

    if (gViewPortInterface) {
        gViewPortInterface->GetAllPlayersInfo();
    }
    players_.BeginFrame(flTime);

    ++frame_count_;

    if (has_layer_frame_time_ && flTime < layer_frame_time_) {
        OnPlayerRespawn(PlayersLayer::ResetReason::RadarClock, layer_frame_time_, flTime);
    }

    layer_frame_time_ = flTime;
    has_layer_frame_time_ = true;
    RadarMapMode mode = RadarMapMode_FromCvar(g_RadarCfg.map_mode->value);

    if (EnsureMapLayout(mode) != RadarMapStatus::Ready) {
        g_MapUnavailable = true;
        return;
    }

    bool spectating = g_iUser1 != 0 || gEngfuncs.IsSpectateOnly();
    bool scoreboard = RadarHooks_IsScoreboardVisible();
    bool fullmap = scoreboard && g_RadarCfg.fullmap->value != 0.0f;

    if (RadarLayerContext_IsLayeredMode(mode) && layer_lifecycle_.is_layout_ready() &&
        !layer_lifecycle_.should_use_fallback()) {
        UpdateLayerContext(spectating, flTime);

        if (layer_lifecycle_.should_render_image()) {
            int layer = layer_context_.get_view().active;
            RadarMapStatus image_status = backdrop_.EnsureLayerImage(map_layers_, layer, frame_count_);

            if (layer_lifecycle_.SetImageStatus(image_status, backdrop_.get_overview().get_layer_failure_reason())) {
                ReportLayerFailure(mode, layer);
            }

            if (image_status != RadarMapStatus::Ready &&
                backdrop_.EnsureTopMap(mode, false, &map_layers_) != RadarMapStatus::Ready) {
                g_MapUnavailable = true;
                return;
            }
        }
    }

    RadarBox box = ComputeBox(spectating);
    RadarShape shape = RadarView_ComputeShape(box, !fullmap && g_RadarCfg.round->value != 0.0f);
    HudTextZone_SetZone(box.x, box.y, box.size);
    RadarTransform transform = ComputeTransform(box, shape, scoreboard);

    UpdateFrameContext(spectating, flTime);

    // Every primitive is tinted toward the fade colour as it is drawn rather than covered
    // by an overlay fill, which would leave a fading halo around the radar.
    ncl_math::Color flash_col;
    flash_alpha_ = ComputeFlashAlpha(flash_col);

    if (flash_alpha_ > 0) {
        draw_.SetFade(flash_col.WithAlpha(flash_alpha_));
    } else {
        draw_.ClearFade();
    }

    RadarFrame frame{draw_, box, transform, flTime, (float)gHUD.m_flTimeDelta, ctx_, flash_alpha_, shape};
    frame.layer_view = layer_context_.get_view();
    frame.layer_view.enabled =
        frame.layer_view.enabled && RadarLayerContext_IsLayeredMode(mode) && layer_lifecycle_.is_image_ready();

    backdrop_.Draw(frame);
    objectives_.CollectEntities(frame);
    // Player-prediction trace context, live for the objective line-of-sight tests below.
    visibility_.BeginObjectiveTests();
    objectives_.Draw(frame);
    visibility_.EndObjectiveTests();
    players_.Draw(frame);

    draw_.EndFrame();
}

int HudRadarAdvanced::ComputeFlashAlpha(ncl_math::Color& color) {
    // The engine blends the screen fade onto the 3D scene before the HUD is drawn, so a
    // flashbang leaves the radar fully readable unless it is washed here too.
    screenfade_t fade;
    std::memset(&fade, 0, sizeof(fade));
    gEngfuncs.pfnGetScreenFade(&fade);

    // Near-white only, so death and round-transition fades do not wash the radar. Shared
    // with the enemy line-of-sight gating so the two agree on what counts as a flash.
    if (!RadarFade_IsNearWhite(fade)) {
        return 0;
    }

    int a = RadarFade_ComputeAlpha(fade, gEngfuncs.GetClientTime());

    if (a <= 0) {
        return 0;
    }

    color.r = fade.fader;
    color.g = fade.fadeg;
    color.b = fade.fadeb;

    return a;
}

bool HudRadarAdvanced::is_map_unavailable() {
    return g_MapUnavailable;
}

namespace {
std::unique_ptr<HudRadarAdvanced> g_NativeRadar;
}

void NativeRadar_Init() {
    NativeRadar_Shutdown();
    RadarConfig_Init();
    RadarDebug_Init();
    g_NativeRadar = std::make_unique<HudRadarAdvanced>();
    RadarHooks_PatchHudText();
}

void NativeRadar_VidInit() {
    if (g_NativeRadar) {
        g_NativeRadar->VidInit();
    }
}

void NativeRadar_Reset() {
    if (g_NativeRadar) {
        g_NativeRadar->Reset();
    }
}

void NativeRadar_InitHud() {
    if (g_NativeRadar) {
        g_NativeRadar->InitHUDData();
    }
}

void NativeRadar_Draw(float time, bool hud_visible) {
    HudTextZone_BeginFrame(ScreenWidth, ScreenHeight);
    if (g_NativeRadar) {
        if (hud_visible) {
            g_NativeRadar->Draw(time);
        }
        g_NativeRadar->UpdateScoreboard(hud_visible);
    }
}

void NativeRadar_FlushHudText() {
    RadarHooks_FlushText();
    HudTextZone_ClearZone();
}

bool NativeRadar_IsClassicRequired() {
    return !g_NativeRadar || !RadarConfig_IsAdvancedSelected() ||
           !RadarConfig_CheckAdvancedActive(g_NativeRadar->get_draw_context()) ||
           HudRadarAdvanced::is_map_unavailable();
}

void NativeRadar_ResetGraphics() {
    if (g_NativeRadar) {
        g_NativeRadar->ResetGraphics();
    }
}

void NativeRadar_Shutdown() {
    if (g_NativeRadar) {
        g_NativeRadar->Shutdown();
        g_NativeRadar.reset();
    }
    RadarHooks_UnpatchHudText();
    HudTextZone_ClearZone();
}

void NativeRadar_SetView(const float* origin, const float* angles) {
    constexpr int kVectorComponents = 3;
    std::copy_n(origin, kVectorComponents, g_MainViewOrigin);
    std::copy_n(angles, kVectorComponents, g_MainViewAngles);
}
