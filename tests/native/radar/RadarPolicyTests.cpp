#include "Test.h"

#include <string>
#include <vector>

#include "hud/radar/policy/RadarPolicy.h"

namespace {
std::vector<std::string> g_Log;

void CaptureLog(const char* message) {
    g_Log.emplace_back(message);
}

bool LogContains(const char* needle) {
    for (const std::string& line : g_Log) {
        if (line.find(needle) != std::string::npos) {
            return true;
        }
    }

    return false;
}

constexpr float kRunSpeed = 320.0f; // stock sv_maxspeed: tolerated broadcast lag is 640 units
} // namespace

class RadarPolicyTest {
  protected:
    RadarPolicy policy;

    void SetUp() {
        g_Log.clear();
        policy.SetLogger(CaptureLog);
        policy.SetLogging(true);
        g_Log.clear(); // drop the snapshot printed by enabling logging
    }

    void Frame(float time) {
        policy.BeginFrame(time, kRunSpeed, true);
    }
};

RADAR_TEST_F(RadarPolicyTest, AllyWithoutAnyBroadcastIsGated) {
    Frame(10.0f);
    policy.ObserveAlly(3, true, {100.0f, 100.0f}, {});

    CHECK_FALSE(policy.IsAllyRevealed(3));
    CHECK_TRUE(LogContains("no radar broadcast"));
}

RADAR_TEST_F(RadarPolicyTest, AllyTrackedByTheServerIsRevealed) {
    Frame(10.0f);
    policy.ObserveAlly(3, true, {100.0f, 100.0f}, {100.0f, 100.0f, 0.0f});

    CHECK_TRUE(policy.IsAllyRevealed(3));
    CHECK_TRUE(LogContains("the server broadcasts it"));
}

RADAR_TEST_F(RadarPolicyTest, BroadcastZeroedMidMapReadsAsFreeForAll) {
    Frame(10.0f);
    policy.ObserveAlly(3, true, {100.0f, 100.0f}, {100.0f, 100.0f, 0.0f});
    CHECK_TRUE(policy.IsAllyRevealed(3));

    Frame(11.0f);
    policy.ObserveAlly(3, true, {120.0f, 100.0f}, {});

    CHECK_FALSE(policy.IsAllyRevealed(3));
    CHECK_TRUE(LogContains("free-for-all"));
}

RADAR_TEST_F(RadarPolicyTest, FrozenBroadcastGatesOnlyAfterTheGraceWindow) {
    Frame(10.0f);
    policy.ObserveAlly(3, true, {1000.0f, 0.0f}, {1000.0f, 0.0f, 0.0f});
    CHECK_TRUE(policy.IsAllyRevealed(3));

    Frame(11.0f);
    policy.ObserveAlly(3, true, {2000.0f, 0.0f}, {1000.0f, 0.0f, 0.0f});
    CHECK_TRUE(policy.IsAllyRevealed(3));

    Frame(11.0f + RadarPolicy::kStaleTime);
    policy.ObserveAlly(3, true, {2000.0f + RadarPolicy::kStaleMoveDist, 0.0f}, {1000.0f, 0.0f, 0.0f});

    CHECK_FALSE(policy.IsAllyRevealed(3));
    CHECK_TRUE(LogContains("broadcast stuck"));
}

RADAR_TEST_F(RadarPolicyTest, StandingAllyWithAStaleBroadcastIsNotGated) {
    // Freezetime: the broadcast still holds last round's position and the server has no
    // reason to refresh it.
    for (int i = 0; i < 10; i++) {
        Frame(10.0f + (float)i);
        policy.ObserveAlly(3, true, {2000.0f, 0.0f}, {1000.0f, 0.0f, 0.0f});
    }

    CHECK_TRUE(policy.IsAllyRevealed(3));
}

RADAR_TEST_F(RadarPolicyTest, LagToleranceScalesWithTheServerRunSpeed) {
    policy.BeginFrame(10.0f, 1000.0f, true);
    policy.ObserveAlly(3, true, {2000.0f, 0.0f}, {1000.0f, 0.0f, 0.0f});

    policy.BeginFrame(20.0f, 1000.0f, true);
    policy.ObserveAlly(3, true, {2128.0f, 0.0f}, {1000.0f, 0.0f, 0.0f});

    CHECK_TRUE(policy.IsAllyRevealed(3));
}

RADAR_TEST_F(RadarPolicyTest, ReturningBroadcastRevealsTheAllyAgain) {
    Frame(10.0f);
    policy.ObserveAlly(3, true, {100.0f, 100.0f}, {});
    CHECK_FALSE(policy.IsAllyRevealed(3));

    Frame(11.0f);
    policy.ObserveAlly(3, true, {100.0f, 100.0f}, {100.0f, 100.0f, 0.0f});

    CHECK_TRUE(policy.IsAllyRevealed(3));
}

RADAR_TEST_F(RadarPolicyTest, AllyOutsideThePvsRidesOnTheBroadcastAlone) {
    Frame(10.0f);
    policy.ObserveAlly(3, false, {}, {1000.0f, 0.0f, 0.0f});

    CHECK_TRUE(policy.IsAllyRevealed(3));
}

RADAR_TEST_F(RadarPolicyTest, SpectatingGatesNothing) {
    policy.BeginFrame(10.0f, kRunSpeed, false);
    policy.ObserveAlly(3, true, {100.0f, 100.0f}, {});

    CHECK_TRUE(policy.IsAllyRevealed(3));
    CHECK_TRUE(g_Log.empty());
}

RADAR_TEST_F(RadarPolicyTest, CvarForcesAllyGating) {
    policy.SetForceAllyGating(true);
    Frame(10.0f);
    policy.ObserveAlly(3, true, {100.0f, 100.0f}, {100.0f, 100.0f, 0.0f});

    CHECK_FALSE(policy.IsAllyRevealed(3));
    CHECK_TRUE(LogContains("hud_radar_allies 0"));
}

RADAR_TEST_F(RadarPolicyTest, ServerHintForcesAllyGating) {
    policy.SetServerFfaHint(true);
    Frame(10.0f);
    policy.ObserveAlly(3, true, {100.0f, 100.0f}, {100.0f, 100.0f, 0.0f});

    CHECK_FALSE(policy.IsAllyRevealed(3));
    CHECK_TRUE(LogContains("serverinfo nc_ffa"));
}

RADAR_TEST_F(RadarPolicyTest, BombMessageOnAMapWithoutTargetsIsAMarker) {
    policy.SetMapObjectives(0, 0);
    policy.ObserveBomb({1, true, {100.0f, 100.0f, 0.0f}, false, 10.0f}, 10.0f);

    CHECK_EQ(policy.get_bomb_intent(), BombIntent::Marker);
    CHECK_TRUE(policy.is_marker_visible());
    CHECK_TRUE(LogContains("no bomb targets"));
}

RADAR_TEST_F(RadarPolicyTest, StationaryBombOnABombMapStaysABomb) {
    policy.SetMapObjectives(2, 0);

    for (int i = 0; i < 5; i++) {
        float time = 10.0f + (float)i;
        policy.ObserveBomb({(std::uint32_t)(i + 1), true, {500.0f, 500.0f, 0.0f}, false, time}, time);
    }

    CHECK_EQ(policy.get_bomb_intent(), BombIntent::Bomb);
    CHECK_FALSE(policy.is_marker_visible());
    CHECK_TRUE(LogContains("treated as the bomb"));
}

RADAR_TEST_F(RadarPolicyTest, PlantedBombThatMovesIsAMarker) {
    policy.SetMapObjectives(2, 0);
    policy.ObserveBomb({1, true, {500.0f, 500.0f, 0.0f}, true, 10.0f}, 10.0f);
    CHECK_EQ(policy.get_bomb_intent(), BombIntent::Bomb);

    policy.ObserveBomb({2, true, {700.0f, 500.0f, 0.0f}, true, 11.0f}, 11.0f);

    CHECK_EQ(policy.get_bomb_intent(), BombIntent::Marker);
    CHECK_TRUE(LogContains("planted bomb moved"));
}

RADAR_TEST_F(RadarPolicyTest, BombTravellingTooFarForTooLongIsAMarker) {
    policy.SetMapObjectives(2, 0);

    for (int i = 0; i < 5; i++) {
        float time = 10.0f + (float)i;
        policy.ObserveBomb({(std::uint32_t)(i + 1), true, {500.0f + 100.0f * i, 500.0f, 0.0f}, false, time}, time);
    }

    CHECK_EQ(policy.get_bomb_intent(), BombIntent::Marker);
    CHECK_TRUE(LogContains("travelled"));
}

RADAR_TEST_F(RadarPolicyTest, ThrownBombThatComesToRestStaysABomb) {
    policy.SetMapObjectives(2, 0);

    // The drop reports the thrower's origin, then the landing spot repeats once a second.
    policy.ObserveBomb({1, true, {0.0f, 0.0f, 0.0f}, false, 10.0f}, 10.0f);

    for (int i = 1; i <= 4; i++) {
        float time = 10.0f + (float)i;
        policy.ObserveBomb({(std::uint32_t)(i + 1), true, {230.0f, 0.0f, 0.0f}, false, time}, time);
    }

    CHECK_EQ(policy.get_bomb_intent(), BombIntent::Bomb);
}

RADAR_TEST_F(RadarPolicyTest, BombFallingThroughOneReportStaysABomb) {
    policy.SetMapObjectives(2, 0);

    policy.ObserveBomb({1, true, {0.0f, 0.0f, 0.0f}, false, 10.0f}, 10.0f);
    policy.ObserveBomb({2, true, {150.0f, 0.0f, 0.0f}, false, 11.0f}, 11.0f);

    for (int i = 2; i <= 4; i++) {
        float time = 10.0f + (float)i;
        policy.ObserveBomb({(std::uint32_t)(i + 1), true, {230.0f, 0.0f, 0.0f}, false, time}, time);
    }

    CHECK_EQ(policy.get_bomb_intent(), BombIntent::Bomb);
}

RADAR_TEST_F(RadarPolicyTest, MarkerClassificationHoldsForTheRestOfTheMap) {
    policy.SetMapObjectives(0, 0);
    policy.ObserveBomb({1, true, {100.0f, 100.0f, 0.0f}, false, 10.0f}, 10.0f);
    CHECK_EQ(policy.get_bomb_intent(), BombIntent::Marker);

    policy.SetMapObjectives(2, 0);
    policy.ObserveBomb({2, true, {100.0f, 100.0f, 0.0f}, false, 11.0f}, 11.0f);

    CHECK_EQ(policy.get_bomb_intent(), BombIntent::Marker);
}

RADAR_TEST_F(RadarPolicyTest, MarkerHidesWhenTheServerStopsRefreshingIt) {
    policy.SetMapObjectives(0, 0);
    policy.ObserveBomb({1, true, {100.0f, 100.0f, 0.0f}, false, 10.0f}, 10.0f);
    CHECK_TRUE(policy.is_marker_visible());

    policy.ObserveBomb({1, true, {100.0f, 100.0f, 0.0f}, false, 10.0f}, 10.0f + RadarPolicy::kMarkerTimeout + 0.1f);

    CHECK_FALSE(policy.is_marker_visible());
    CHECK_TRUE(LogContains("no update for"));
}

RADAR_TEST_F(RadarPolicyTest, ServerHintOutranksTheHeuristics) {
    policy.SetMapObjectives(2, 0);
    policy.SetBombHint(MessageHint::Marker);

    CHECK_EQ(policy.get_bomb_intent(), BombIntent::Marker);
    CHECK_TRUE(LogContains("serverinfo nc_bombmsg"));

    policy.ObserveBomb({1, true, {500.0f, 500.0f, 0.0f}, false, 10.0f}, 10.0f);
    CHECK_EQ(policy.get_bomb_intent(), BombIntent::Marker);

    policy.SetBombHint(MessageHint::Stock);
    policy.SetMapObjectives(0, 0);
    CHECK_EQ(policy.get_bomb_intent(), BombIntent::Bomb);

    policy.ObserveBomb({2, true, {500.0f, 500.0f, 0.0f}, false, 11.0f}, 11.0f);
    CHECK_EQ(policy.get_bomb_intent(), BombIntent::Bomb);
    CHECK_FALSE(policy.is_marker_visible());
}

RADAR_TEST_F(RadarPolicyTest, HostageMessageOnAMapWithoutHostagesIsAMarker) {
    policy.SetMapObjectives(2, 0);
    policy.ObserveHostage({1, 1, 10.0f}, 10.0f);

    CHECK_EQ(policy.get_hostage_intent(), HostageIntent::Marker);
    CHECK_TRUE(policy.IsHostageMarkerVisible(1));
    CHECK_TRUE(LogContains("the map has no hostages"));
}

RADAR_TEST_F(RadarPolicyTest, HostageOnARescueMapStaysAHostage) {
    policy.SetMapObjectives(0, 4);

    // CHostage's cadence: one position per second while the hostage walks.
    for (int i = 0; i < 6; i++) {
        float time = 10.0f + (float)i;
        policy.ObserveHostage({2, (std::uint32_t)(i + 1), time}, time);
    }

    CHECK_EQ(policy.get_hostage_intent(), HostageIntent::Hostage);
    CHECK_FALSE(policy.IsHostageMarkerVisible(2));
    CHECK_TRUE(LogContains("treated as a hostage"));
}

RADAR_TEST_F(RadarPolicyTest, HostageIndexPastTheMapsHostagesIsAMarker) {
    policy.SetMapObjectives(0, 4);
    policy.ObserveHostage({9, 1, 10.0f}, 10.0f);

    CHECK_EQ(policy.get_hostage_intent(), HostageIntent::Marker);
    CHECK_TRUE(LogContains("past the map's 4 hostage(s)"));
}

RADAR_TEST_F(RadarPolicyTest, HostageUpdatesFasterThanTheServerCanSendAreAMarker) {
    policy.SetMapObjectives(0, 4);

    for (int i = 0; i < 12; i++) {
        float time = 10.0f + 0.3f * (float)i;
        policy.ObserveHostage({1, (std::uint32_t)(i + 1), time}, time);
    }

    CHECK_EQ(policy.get_hostage_intent(), HostageIntent::Marker);
    CHECK_TRUE(LogContains("above the rate CHostage can send"));
}

RADAR_TEST_F(RadarPolicyTest, HostageMarkerHidesWhenUpdatesStop) {
    policy.SetMapObjectives(2, 0);
    policy.ObserveHostage({1, 1, 10.0f}, 10.0f);
    CHECK_TRUE(policy.IsHostageMarkerVisible(1));

    policy.ObserveHostage({1, 1, 10.0f}, 10.0f + RadarPolicy::kMarkerTimeout + 0.1f);

    CHECK_FALSE(policy.IsHostageMarkerVisible(1));
    CHECK_TRUE(LogContains("server marker 1 hidden"));
}

RADAR_TEST_F(RadarPolicyTest, HostagesAreGatedWhenTheServerSendsNoBroadcast) {
    policy.SetMapObjectives(0, 4);
    policy.ObserveHostageBroadcast(true, 0);

    CHECK_FALSE(policy.are_hostages_revealed());
    CHECK_TRUE(LogContains("sends no hostage broadcast"));

    policy.ObserveHostageBroadcast(true, 2);

    CHECK_TRUE(policy.are_hostages_revealed());
}

RADAR_TEST_F(RadarPolicyTest, HostageGatingNeverAppliesOffTheCtSide) {
    // Terrorists are never sent HostagePos, so its absence says nothing.
    policy.SetMapObjectives(0, 4);
    policy.ObserveHostageBroadcast(false, 0);

    CHECK_TRUE(policy.are_hostages_revealed());
    CHECK_TRUE(g_Log.empty());
}

RADAR_TEST_F(RadarPolicyTest, HostageGatingNeverAppliesOnAMapWithoutHostages) {
    policy.SetMapObjectives(2, 0);
    policy.ObserveHostageBroadcast(true, 0);

    CHECK_TRUE(policy.are_hostages_revealed());
    CHECK_TRUE(g_Log.empty());
}

RADAR_TEST_F(RadarPolicyTest, HostageHintOutranksTheHeuristics) {
    policy.SetMapObjectives(0, 4);
    policy.SetHostageHint(MessageHint::Marker);

    CHECK_EQ(policy.get_hostage_intent(), HostageIntent::Marker);
    CHECK_TRUE(LogContains("serverinfo nc_hostagemsg"));

    policy.ObserveHostage({1, 1, 10.0f}, 10.0f);
    CHECK_EQ(policy.get_hostage_intent(), HostageIntent::Marker);

    policy.SetHostageHint(MessageHint::Stock);
    policy.SetMapObjectives(0, 0);
    CHECK_EQ(policy.get_hostage_intent(), HostageIntent::Hostage);

    policy.ObserveHostage({1, 2, 11.0f}, 11.0f);
    CHECK_EQ(policy.get_hostage_intent(), HostageIntent::Hostage);
    CHECK_FALSE(policy.IsHostageMarkerVisible(1));
}

RADAR_TEST_F(RadarPolicyTest, LevelChangeForgetsTheDecisions) {
    policy.SetMapObjectives(0, 0);
    policy.ObserveBomb({1, true, {100.0f, 100.0f, 0.0f}, false, 10.0f}, 10.0f);
    policy.ObserveHostage({1, 1, 10.0f}, 10.0f);
    Frame(10.0f);
    policy.ObserveAlly(3, true, {100.0f, 100.0f}, {});
    CHECK_EQ(policy.get_bomb_intent(), BombIntent::Marker);
    CHECK_EQ(policy.get_hostage_intent(), HostageIntent::Marker);
    CHECK_FALSE(policy.IsAllyRevealed(3));

    policy.OnLevelChange();
    Frame(11.0f);
    policy.ObserveAlly(3, true, {100.0f, 100.0f}, {100.0f, 100.0f, 0.0f});

    CHECK_EQ(policy.get_bomb_intent(), BombIntent::Unknown);
    CHECK_EQ(policy.get_hostage_intent(), HostageIntent::Unknown);
    CHECK_TRUE(policy.IsAllyRevealed(3));
}

RADAR_TEST(RadarPolicyLogging, DecisionsAreSilentUntilDebugIsEnabled) {
    g_Log.clear();
    RadarPolicy policy;
    policy.SetLogger(CaptureLog);

    policy.SetMapObjectives(0, 0);
    policy.ObserveBomb({1, true, {100.0f, 100.0f, 0.0f}, false, 10.0f}, 10.0f);
    policy.BeginFrame(10.0f, kRunSpeed, true);
    policy.ObserveAlly(3, true, {100.0f, 100.0f}, {});

    CHECK_TRUE(g_Log.empty());

    policy.SetLogging(true);

    CHECK_TRUE(LogContains("1 ally(ies) gated"));
    CHECK_TRUE(LogContains("bomb message = a server marker"));
}
