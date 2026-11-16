#pragma once

#include <cstdint>
#include <string>

#include "NativeSdk.h"

#include "hud/radar/map/MapGeometry.h"

struct RadarMapPreparationDiagnostic {
    std::string waiting_reason;
    uint64_t attempts{};
    int mode{};
    float capture_ms{};
};

const hull_t* MapBspGeometry_SelectPlayerHull(playermove_t& movement, const event_api_s& events, model_t& world,
                                              int player_hull, std::string* waiting_reason = nullptr);

// False leaves out empty so incomplete world data can be retried.
bool MapBspGeometry_Collect(const model_t& world, const hull_t& hull, const ncl_math::Aabb3& crouch_bounds,
                            RadarMapGeometry& out, std::string* waiting_reason = nullptr);
