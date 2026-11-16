#include "Test.h"

#include <string>
#include <utility>

#include "RadarMapLayerFixtures.h"
#include "NativeSdk.h"
#include "hud/radar/map/layers/RadarLayerContext.h"
#include "hud/radar/map/layers/RadarMapLayers.h"

namespace {
RadarMapLayers StackedMap(float upper_height = 256.0f) {
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {256.0f, 256.0f}});
    fixture.AddFloor(upper_height, {{0.0f, 0.0f}, {256.0f, 256.0f}});

    RadarMapLayers map;
    CHECK_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);

    std::string error;
    CHECK_TRUE(map.ApplyOverrides(
        {{{"lower", -128.0f, upper_height * 0.5f}, {"upper", upper_height * 0.5f, 512.0f}}, {}}, error))
        << error;
    CHECK_EQ(map.get_layers().size(), 2u);

    return map;
}

RadarLayerObservation Grounded(float height, float time = 0.0f) {
    return {{64.0f, 64.0f, height}, ncl_math::Vector3{64.0f, 64.0f, height}, time, 1, true, true, false, false};
}
} // namespace

RADAR_TEST(RadarLayerSelection, ConfirmsAfterPoint15Seconds) {
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;
    RadarLayerObservation observation = Grounded(0.0f);

    CHECK_EQ(selection.Update(map, observation), 0);

    observation = Grounded(144.0f, 0.5f);
    CHECK_EQ(selection.Update(map, observation), 0);
    CHECK_EQ(selection.get_candidate(), -1);

    observation = Grounded(145.0f, 1.0f);
    CHECK_EQ(selection.Update(map, observation), 0);
    CHECK_EQ(selection.get_candidate(), 1);
    CHECK_FLOAT_EQ(selection.get_candidate_since(), 1.0f);
    CHECK_EQ(selection.get_reason(), RadarLayerReason::Pending);

    observation.time = 1.149f;
    CHECK_EQ(selection.Update(map, observation), 0);

    observation.time = 1.151f;
    CHECK_EQ(selection.Update(map, observation), 1);
    CHECK_EQ(selection.get_candidate(), -1);
    CHECK_EQ(selection.get_reason(), RadarLayerReason::Confirmed);
}

RADAR_TEST(RadarLayerSelection, ExactConfirmationThreshold) {
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;

    CHECK_EQ(selection.Update(map, Grounded(0.0f)), 0);

    RadarLayerObservation observation = Grounded(145.0f);
    CHECK_EQ(selection.Update(map, observation), 0);

    observation.time = 0.15f;
    CHECK_EQ(selection.Update(map, observation), 1);
}

RADAR_TEST(RadarLayerSelection, MarginCapsAtQuarterGap) {
    RadarMapLayers map = StackedMap(48.0f);
    RadarLayerSelection selection;

    CHECK_EQ(selection.Update(map, Grounded(0.0f)), 0);

    RadarLayerObservation observation = Grounded(36.0f, 1.0f);
    CHECK_EQ(selection.Update(map, observation), 0);
    CHECK_EQ(selection.get_candidate(), -1);

    observation = Grounded(37.0f, 1.1f);
    CHECK_EQ(selection.Update(map, observation), 0);
    CHECK_EQ(selection.get_candidate(), 1);

    observation.time = 1.251f;
    CHECK_EQ(selection.Update(map, observation), 1);
}

RADAR_TEST(RadarLayerSelection, JumpKeepsFloor) {
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;

    CHECK_EQ(selection.Update(map, Grounded(0.0f)), 0);

    RadarLayerObservation observation = Grounded(200.0f, 1.0f);
    observation.grounded = false;
    observation.support.reset();
    CHECK_EQ(selection.Update(map, observation), 0);
    CHECK_EQ(selection.get_reason(), RadarLayerReason::Airborne);

    observation.time = 1.3f;
    observation.feet.z = 145.0f;
    CHECK_EQ(selection.Update(map, observation), 0);
    CHECK_EQ(selection.get_candidate(), -1);
}

RADAR_TEST(RadarLayerSelection, VerticalTeleportSelectsImmediately) {
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;

    CHECK_EQ(selection.Update(map, Grounded(0.0f)), 0);

    RadarLayerObservation observation = Grounded(256.0f, 0.01f);
    observation.discontinuity = true;
    CHECK_EQ(selection.Update(map, observation), 1);
    CHECK_EQ(selection.get_reason(), RadarLayerReason::Discontinuity);
    CHECK_EQ(selection.get_candidate(), -1);
}

RADAR_TEST(RadarLayerSelection, TargetChangeCancelsPendingSwitch) {
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;

    CHECK_EQ(selection.Update(map, Grounded(0.0f)), 0);
    CHECK_EQ(selection.Update(map, Grounded(145.0f, 1.0f)), 0);
    CHECK_EQ(selection.get_candidate(), 1);

    CHECK_EQ(selection.Update(map, Grounded(130.0f, 1.1f)), 0);
    CHECK_EQ(selection.get_candidate(), -1);

    CHECK_EQ(selection.Update(map, Grounded(145.0f, 1.2f)), 0);
    CHECK_FLOAT_EQ(selection.get_candidate_since(), 1.2f);

    CHECK_EQ(selection.Update(map, Grounded(145.0f, 1.349f)), 0);
    CHECK_EQ(selection.Update(map, Grounded(145.0f, 1.351f)), 1);
}

RADAR_TEST(RadarLayerSelection, FreeCameraOutsideBounds) {
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;
    RadarLayerObservation observation{{400.0f, 64.0f, 256.0f}, {}, 0.0f, 2, true, false, true, false};

    CHECK_EQ(selection.Update(map, observation), 1);
    CHECK_EQ(selection.get_reason(), RadarLayerReason::Nearest);

    observation.feet.z = 128.0f;
    observation.time = 1.0f;
    CHECK_EQ(selection.Update(map, observation), 1);
    CHECK_EQ(selection.get_candidate(), -1);
    CHECK_EQ(selection.get_reason(), RadarLayerReason::Nearest);

    observation.feet.z = 111.0f;
    observation.time = 2.0f;
    CHECK_EQ(selection.Update(map, observation), 1);
    CHECK_EQ(selection.get_candidate(), 0);

    observation.time = 2.151f;
    CHECK_EQ(selection.Update(map, observation), 0);
}

RADAR_TEST(RadarLayerSelection, FreeCameraUsesSupportBelowWithinProjection) {
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;
    RadarLayerObservation observation{{64.0f, 64.0f, 200.0f}, {}, 0.0f, 2, true, false, true, false};

    CHECK_EQ(selection.Update(map, observation), 0);

    observation.feet.z = 300.0f;
    observation.time = 1.0f;
    CHECK_EQ(selection.Update(map, observation), 0);
    CHECK_EQ(selection.get_candidate(), 1);

    observation.time = 1.151f;
    CHECK_EQ(selection.Update(map, observation), 1);
}

RADAR_TEST(RadarLayerSelection, LostSupportHoldsSelection) {
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;

    CHECK_EQ(selection.Update(map, Grounded(0.0f)), 0);
    CHECK_EQ(selection.Update(map, Grounded(145.0f, 1.0f)), 0);

    RadarLayerObservation observation = Grounded(256.0f, 1.2f);
    observation.support.reset();
    CHECK_EQ(selection.Update(map, observation), 0);
    CHECK_EQ(selection.get_reason(), RadarLayerReason::Invalid);
    CHECK_EQ(selection.get_candidate(), -1);

    CHECK_EQ(selection.Update(map, Grounded(145.0f, 1.3f)), 0);
    CHECK_FLOAT_EQ(selection.get_candidate_since(), 1.3f);

    CHECK_EQ(selection.Update(map, Grounded(145.0f, 1.451f)), 1);
}

RADAR_TEST(RadarLayerSelection, FallReachesLowerFloor) {
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;

    CHECK_EQ(selection.Update(map, Grounded(256.0f)), 1);

    RadarLayerObservation observation = Grounded(113.0f, 1.0f);
    observation.grounded = false;
    observation.support.reset();
    CHECK_EQ(selection.Update(map, observation), 1);
    CHECK_EQ(selection.get_candidate(), -1);

    observation.feet.z = 112.0f;
    observation.time = 1.1f;
    CHECK_EQ(selection.Update(map, observation), 1);
    CHECK_EQ(selection.get_candidate(), -1);

    observation.feet.z = 111.0f;
    observation.time = 1.2f;
    CHECK_EQ(selection.Update(map, observation), 1);
    CHECK_EQ(selection.get_candidate(), 0);

    observation.feet.z = 80.0f;
    observation.time = 1.349f;
    CHECK_EQ(selection.Update(map, observation), 1);

    observation.time = 1.351f;
    CHECK_EQ(selection.Update(map, observation), 0);
    CHECK_EQ(selection.get_reason(), RadarLayerReason::Confirmed);
}

RADAR_TEST(RadarLayerSelection, SameHeightRegionTransition) {
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {256.0f, 256.0f}});

    RadarMapLayers map;
    CHECK_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);

    RadarLayerOverrides overrides;
    overrides.layers = {{"left", -64.0f, 64.0f}, {"right", 64.0f, 128.0f}};
    overrides.regions.push_back({"right", {{128.0f, 0.0f, -64.0f}, {256.0f, 256.0f, 64.0f}}});
    std::string error;

    CHECK_TRUE(map.ApplyOverrides(overrides, error)) << error;

    int left = -1;
    int right = -1;

    for (const RadarLayerInfo& layer : map.get_layers()) {
        if (layer.name == "left") {
            left = layer.id;
        }

        if (layer.name == "right") {
            right = layer.id;
        }
    }

    CHECK_GE(left, 0);
    CHECK_GE(right, 0);
    CHECK_FLOAT_EQ(map.get_layers()[left].height, 0.0f);
    CHECK_FLOAT_EQ(map.get_layers()[right].height, 0.0f);

    RadarLayerSelection selection;
    CHECK_EQ(selection.Update(map, Grounded(0.0f)), left);

    RadarLayerObservation observation = Grounded(0.0f, 1.0f);
    observation.feet.x = 128.0f;
    observation.support->x = 128.0f;
    CHECK_EQ(selection.Update(map, observation), left);
    CHECK_EQ(selection.get_candidate(), right);

    observation.time = 1.149f;
    CHECK_EQ(selection.Update(map, observation), left);

    observation.time = 1.151f;
    CHECK_EQ(selection.Update(map, observation), right);

    observation.feet.x = 127.99f;
    observation.support->x = 127.99f;
    observation.time = 2.0f;
    CHECK_EQ(selection.Update(map, observation), right);
    CHECK_EQ(selection.get_candidate(), left);
}

RADAR_TEST(RadarLayerSelection, LandingOnOtherSupportSelectsImmediately) {
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;

    CHECK_EQ(selection.Update(map, Grounded(0.0f)), 0);

    RadarLayerObservation observation = Grounded(256.0f, 1.0f);
    observation.grounded = false;
    observation.support.reset();
    CHECK_EQ(selection.Update(map, observation), 0);

    observation = Grounded(256.0f, 1.01f);
    CHECK_EQ(selection.Update(map, observation), 1);
    CHECK_EQ(selection.get_reason(), RadarLayerReason::Grounded);
    CHECK_EQ(selection.get_candidate(), -1);
}

RADAR_TEST(RadarLayerSelection, LandingOnMovingPlatformStillConfirms) {
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;

    CHECK_EQ(selection.Update(map, Grounded(0.0f)), 0);

    RadarLayerObservation observation = Grounded(145.0f, 1.0f);
    observation.grounded = false;
    observation.support.reset();
    CHECK_EQ(selection.Update(map, observation), 0);

    observation = Grounded(145.0f, 1.01f);
    CHECK_EQ(selection.Update(map, observation), 0);
    CHECK_EQ(selection.get_candidate(), 1);

    observation.time = 1.161f;
    CHECK_EQ(selection.Update(map, observation), 1);
}

RADAR_TEST(RadarLayerSelection, GroundedRampKeepsConfirmation) {
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{-256.0f, -128.0f}, {0.0f, 128.0f}});
    fixture.AddFloor(256.0f, {{512.0f, -128.0f}, {768.0f, 128.0f}});
    fixture.AddRamp({0.0f, 0.0f, 0.0f}, {512.0f, 0.0f, 256.0f}, 64.0f);
    fixture.AddOverlapRoom(0, 1);

    RadarMapLayers map;
    CHECK_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);

    RadarLayerSelection selection;
    RadarLayerObservation observation = Grounded(0.0f);
    observation.feet.x = -64.0f;
    observation.feet.y = 0.0f;
    observation.support = observation.feet;
    CHECK_EQ(selection.Update(map, observation), 0);

    observation.feet = {280.0f, 0.0f, 140.0f};
    observation.support = observation.feet;
    observation.time = 0.5f;
    CHECK_EQ(selection.Update(map, observation), 0);
    CHECK_EQ(selection.get_candidate(), -1);

    observation.feet = {300.0f, 0.0f, 150.0f};
    observation.support = observation.feet;
    observation.time = 1.0f;
    CHECK_EQ(selection.Update(map, observation), 0);
    CHECK_EQ(selection.get_candidate(), 1);

    observation.time = 1.151f;
    CHECK_EQ(selection.Update(map, observation), 1);
}

RADAR_TEST(RadarLayerSelection, SourceChangeCancelsPendingSwitch) {
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;

    CHECK_EQ(selection.Update(map, Grounded(0.0f)), 0);
    CHECK_EQ(selection.Update(map, Grounded(145.0f, 1.0f)), 0);

    RadarLayerObservation observation = Grounded(256.0f, 1.01f);
    observation.source = 2;
    CHECK_EQ(selection.Update(map, observation), 1);
    CHECK_EQ(selection.get_candidate(), -1);
    CHECK_EQ(selection.get_reason(), RadarLayerReason::Discontinuity);
}

RADAR_TEST(RadarLayerSelection, TimeRollbackCancelsPendingSwitch) {
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;

    CHECK_EQ(selection.Update(map, Grounded(0.0f)), 0);
    CHECK_EQ(selection.Update(map, Grounded(145.0f, 1.0f)), 0);
    CHECK_EQ(selection.Update(map, Grounded(256.0f, 0.5f)), 1);
    CHECK_EQ(selection.get_candidate(), -1);
    CHECK_EQ(selection.get_reason(), RadarLayerReason::Discontinuity);
}

RADAR_TEST(RadarLayerSelection, InvalidObservationHoldsAndCancelsCandidate) {
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;

    CHECK_EQ(selection.Update(map, Grounded(0.0f)), 0);
    CHECK_EQ(selection.Update(map, Grounded(145.0f, 1.0f)), 0);

    RadarLayerObservation observation = Grounded(256.0f, 1.2f);
    observation.valid = false;
    CHECK_EQ(selection.Update(map, observation), 0);
    CHECK_EQ(selection.get_active(), 0);
    CHECK_EQ(selection.get_reason(), RadarLayerReason::Invalid);
    CHECK_EQ(selection.get_candidate(), -1);
}

RADAR_TEST(RadarLayerSelection, NextValidObservationInitializesAfterInvalidInput) {
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;
    RadarLayerObservation observation = Grounded(256.0f);
    observation.valid = false;
    CHECK_EQ(selection.Update(map, observation), -1);

    observation.valid = true;
    observation.support.reset();
    observation.time = 1.0f;
    CHECK_EQ(selection.Update(map, observation), 1);
    CHECK_EQ(selection.get_reason(), RadarLayerReason::Initial);
}

RADAR_TEST(RadarLayerSelection, InitialPositionOutsideProjectionUsesNearest) {
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;
    RadarLayerObservation observation{{400.0f, 64.0f, 256.0f}, {}, 0.0f, 1, true, false, false, false};

    CHECK_EQ(selection.Update(map, observation), 1);
    CHECK_EQ(selection.get_reason(), RadarLayerReason::Nearest);
}

RADAR_TEST(RadarLayerSelection, CrouchingUsesNormalizedFeet) {
    RadarMapLayers map = StackedMap(48.0f);
    RadarLayerSelection selection;
    RadarLayerObservation observation = Grounded(0.0f);
    observation.feet = RadarLayerContext_Feet({64.0f, 64.0f, 36.0f}, {-16.0f, -16.0f, -36.0f});
    CHECK_EQ(observation.feet, (ncl_math::Vector3{64.0f, 64.0f, 0.0f}));

    CHECK_EQ(selection.Update(map, observation), 0);

    observation.feet = RadarLayerContext_Feet({64.0f, 64.0f, 18.0f}, {-16.0f, -16.0f, -18.0f});
    CHECK_EQ(observation.feet, (ncl_math::Vector3{64.0f, 64.0f, 0.0f}));
    observation.time = 1.0f;
    CHECK_EQ(selection.Update(map, observation), 0);
    CHECK_EQ(selection.get_candidate(), -1);
    CHECK_EQ(selection.get_reason(), RadarLayerReason::Unchanged);
}

RADAR_TEST(RadarLayerSelection, ResetClearsPendingStateAndInitializesImmediately) {
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;

    CHECK_EQ(selection.Update(map, Grounded(0.0f)), 0);
    CHECK_EQ(selection.Update(map, Grounded(145.0f, 1.0f)), 0);
    CHECK_EQ(selection.get_candidate(), 1);

    selection.Reset();

    CHECK_EQ(selection.get_active(), -1);
    CHECK_EQ(selection.get_candidate(), -1);
    CHECK_FLOAT_EQ(selection.get_candidate_since(), 0.0f);

    CHECK_EQ(selection.Update(map, Grounded(256.0f, 0.01f)), 1);
    CHECK_EQ(selection.get_reason(), RadarLayerReason::Initial);
}

RADAR_TEST(RadarLayerSelection, InvalidAirborneObservationDoesNotConfirmLanding) {
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;

    CHECK_EQ(selection.Update(map, Grounded(0.0f)), 0);

    RadarLayerObservation observation = Grounded(256.0f, 1.0f);
    observation.valid = false;
    observation.grounded = false;
    observation.support.reset();
    CHECK_EQ(selection.Update(map, observation), 0);

    observation = Grounded(256.0f, 1.1f);
    CHECK_EQ(selection.Update(map, observation), 0);
    CHECK_EQ(selection.get_reason(), RadarLayerReason::Pending);

    observation.time = 1.251f;
    CHECK_EQ(selection.Update(map, observation), 1);
}

RADAR_TEST(RadarLayerSelection, AirborneOutsideProjectionHoldsSelection) {
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;

    CHECK_EQ(selection.Update(map, Grounded(256.0f)), 1);

    RadarLayerObservation observation{{400.0f, 64.0f, 0.0f}, {}, 1.0f, 1, true, false, false, false};
    CHECK_EQ(selection.Update(map, observation), 1);
    CHECK_EQ(selection.get_candidate(), -1);

    observation.time = 1.3f;
    CHECK_EQ(selection.Update(map, observation), 1);
    CHECK_EQ(selection.get_reason(), RadarLayerReason::Airborne);
}

RADAR_TEST(RadarLayerSelection, FreeCameraNearestTieUsesLowestIdInitially) {
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;
    RadarLayerObservation observation{{400.0f, 64.0f, 128.0f}, {}, 0.0f, 2, true, false, true, false};

    CHECK_EQ(selection.Update(map, observation), 0);
    CHECK_EQ(selection.get_reason(), RadarLayerReason::Nearest);
}

RADAR_TEST(RadarLayerSelection, InvalidDiscontinuitySelectsAfterRecovery) {
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;

    CHECK_EQ(selection.Update(map, Grounded(0.0f)), 0);
    CHECK_EQ(selection.Update(map, Grounded(145.0f, 1.0f)), 0);

    RadarLayerObservation observation = Grounded(256.0f, 1.1f);
    observation.valid = false;
    observation.discontinuity = true;
    CHECK_EQ(selection.Update(map, observation), 0);
    CHECK_EQ(selection.get_candidate(), -1);
    CHECK_EQ(selection.get_reason(), RadarLayerReason::Invalid);

    observation.valid = true;
    observation.discontinuity = false;
    observation.time = 1.2f;
    CHECK_EQ(selection.Update(map, observation), 1);
    CHECK_EQ(selection.get_reason(), RadarLayerReason::Discontinuity);
}

RADAR_TEST(RadarLayerSelection, InvalidTimeRollbackSelectsAfterRecovery) {
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;

    CHECK_EQ(selection.Update(map, Grounded(0.0f)), 0);
    CHECK_EQ(selection.Update(map, Grounded(145.0f, 1.0f)), 0);

    RadarLayerObservation observation = Grounded(256.0f, 0.5f);
    observation.valid = false;
    CHECK_EQ(selection.Update(map, observation), 0);

    observation.valid = true;
    observation.time = 1.2f;
    CHECK_EQ(selection.Update(map, observation), 1);
    CHECK_EQ(selection.get_reason(), RadarLayerReason::Discontinuity);
}

RADAR_TEST(RadarLayerSelection, DistantStacksUseLocalSupportForConfirmation) {
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
    fixture.AddFloor(256.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
    fixture.AddFloor(2048.0f, {{2048.0f, 0.0f}, {2560.0f, 512.0f}});
    fixture.AddFloor(2304.0f, {{2048.0f, 0.0f}, {2560.0f, 512.0f}});

    RadarMapLayers map;
    CHECK_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    CHECK_EQ(map.get_layers().size(), 2u);

    RadarLayerSelection selection;
    CHECK_EQ(selection.Update(map, Grounded(0.0f)), 0);
    CHECK_EQ(selection.Update(map, Grounded(256.0f, 1.0f)), 0);
    CHECK_EQ(selection.get_candidate(), 1);

    CHECK_EQ(selection.Update(map, Grounded(256.0f, 1.151f)), 1);

    RadarLayerObservation observation = Grounded(2304.0f, 2.0f);
    observation.feet.x = 2112.0f;
    observation.support = observation.feet;
    observation.discontinuity = true;
    CHECK_EQ(selection.Update(map, observation), 1);

    observation.discontinuity = false;
    observation.time = 3.0f;
    observation.feet.z = 2048.0f;
    observation.support = observation.feet;
    CHECK_EQ(selection.Update(map, observation), 1);
    CHECK_EQ(selection.get_candidate(), 0);

    observation.time = 3.151f;
    CHECK_EQ(selection.Update(map, observation), 0);
}

RADAR_TEST(RadarLayerSelection, DistantRampsWithNearbyHeightsKeepLocalTransition) {
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{-256.0f, -128.0f}, {0.0f, 128.0f}});
    fixture.AddFloor(256.0f, {{512.0f, -128.0f}, {768.0f, 128.0f}});
    fixture.AddFloor(64.0f, {{1792.0f, -128.0f}, {2048.0f, 128.0f}});
    fixture.AddFloor(320.0f, {{2560.0f, -128.0f}, {2816.0f, 128.0f}});
    fixture.AddRamp({0.0f, 0.0f, 0.0f}, {512.0f, 0.0f, 256.0f}, 64.0f);
    fixture.AddRamp({2048.0f, 0.0f, 64.0f}, {2560.0f, 0.0f, 320.0f}, 64.0f);
    fixture.AddOverlapRoom(0, 1);
    fixture.AddOverlapRoom(2, 3);

    RadarMapLayers map;
    CHECK_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    CHECK_EQ(map.get_layers().size(), 2u);
    CHECK_EQ(map.get_analysis().transitions.size(), 2u);

    for (int ramp = 0; ramp < 2; ramp++) {
        for (bool rising : {true, false}) {
            RADAR_TRACE(ramp);
            RADAR_TRACE(rising);
            float x_offset = (float)ramp * 2048.0f;
            float z_offset = (float)ramp * 64.0f;
            auto observation = [&](float x, float time) {
                RadarLayerObservation result = Grounded(z_offset + x * 0.5f, time);
                result.feet.x = x_offset + x;
                result.feet.y = 0.0f;
                result.support = result.feet;

                return result;
            };
            int initial = rising ? 0 : 1;
            int target = rising ? 1 : 0;
            RadarLayerSelection selection;

            CHECK_EQ(selection.Update(map, observation(rising ? 200.0f : 312.0f, 0.0f)), initial);
            CHECK_EQ(selection.Update(map, observation(rising ? 290.0f : 222.0f, 1.0f)), initial);
            CHECK_EQ(selection.get_candidate(), target);

            CHECK_EQ(selection.Update(map, observation(rising ? 322.0f : 190.0f, 1.14f)), initial);
            CHECK_EQ(selection.get_candidate(), target);
            CHECK_FLOAT_EQ(selection.get_candidate_since(), 1.0f);

            CHECK_EQ(selection.Update(map, observation(rising ? 386.0f : 126.0f, 1.151f)), target);
            CHECK_EQ(selection.get_reason(), RadarLayerReason::Confirmed);
        }
    }
}
