#include "Test.h"

#include "hud/radar/markers/RadarBroadcast.h"
#include "hud/radar/layers/players/DeathMarks.h"
#include "hud/radar/layers/players/RadarDeathMarkerPolicy.h"

RADAR_TEST(RadarDeathMessage, CarriesExactPositionUntilPlayerReset) {
    RadarBroadcast broadcast;
    broadcast.SetDeathPosition(3, {100.0f, -75.0f, 8.0f}, 10.0f);
    CHECK_TRUE(broadcast.get_death(3).seq != 0);
    CHECK_EQ(broadcast.get_death(3).pos.x, 100.0f);
    CHECK_EQ(broadcast.get_death(3).pos.y, -75.0f);
    CHECK_EQ(broadcast.get_death(3).time, 10.0f);
    broadcast.ResetPlayers();
    CHECK_EQ(broadcast.get_death(3).seq, 0u);
}

RADAR_TEST(RadarDeathMessage, ExactPositionRequiresAllyReveal) {
    RadarBroadcast::Death death{{100.0f, -75.0f, 8.0f}, 10.0f, 1};
    CHECK_FALSE(RadarDeathMarker_UseExactPosition(death, 0, RadarTeam::Terrorist, RadarTeam::Terrorist,
                                                  false, 10.1f));
    CHECK_TRUE(RadarDeathMarker_UseExactPosition(death, 0, RadarTeam::Terrorist, RadarTeam::Terrorist,
                                                 true, 10.1f));
    CHECK_FALSE(RadarDeathMarker_UseExactPosition(death, 0, RadarTeam::CounterTerrorist, RadarTeam::Terrorist,
                                                 true, 10.1f));
    CHECK_FALSE(RadarDeathMarker_UseExactPosition(death, 1, RadarTeam::Terrorist, RadarTeam::Terrorist,
                                                  true, 10.1f));
    CHECK_FALSE(RadarDeathMarker_UseExactPosition(death, 0, RadarTeam::Terrorist, RadarTeam::Terrorist,
                                                  true, 11.1f));
}

RADAR_TEST(RadarDeathMessage, CorrectsLastKnownMarkWithoutDuplicatingIt) {
    DeathMarks marks;
    marks.Add(3, {40.0f, 0.0f, 0.0f}, {255, 0, 0, 255}, 10.0f);
    marks.Correct(3, {100.0f, -75.0f, 8.0f}, {255, 0, 0, 255}, 10.1f);
    DeathMarks::Visible visible[DeathMarks::kMax];
    CHECK_EQ(marks.CollectVisible(visible, 10.1f), 1);
    CHECK_EQ(visible[0].pos.x, 100.0f);
    CHECK_EQ(visible[0].pos.y, -75.0f);
    marks.Add(3, {200.0f, 10.0f, 0.0f}, {255, 0, 0, 255}, 10.2f);
    CHECK_EQ(marks.CollectVisible(visible, 10.2f), 2);
}
