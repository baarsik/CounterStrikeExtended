#pragma once

#include <cstdint>

#include "hud/radar/RadarTeam.h"
#include "hud/radar/markers/RadarBroadcast.h"

inline bool RadarDeathMarker_UseExactPosition(const RadarBroadcast::Death& death, std::uint32_t consumedSequence,
                                             RadarTeam victimTeam, RadarTeam localTeam, bool allyRevealed, float time) {
    constexpr float kDeathMessageAge = 1.0f;
    return death.seq != 0 && death.seq != consumedSequence &&
           (victimTeam == RadarTeam::Terrorist || victimTeam == RadarTeam::CounterTerrorist) &&
           victimTeam == localTeam && allyRevealed && time >= death.time && time - death.time <= kDeathMessageAge;
}
