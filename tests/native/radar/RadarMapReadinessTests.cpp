#include "Test.h"
#include "radar_cvars.h"
#include "hud/radar/integration/RadarMapReadiness.h"

namespace {
struct MapProvider {
    RadarMapStatus top_status = RadarMapStatus::Waiting;
    int layout_attempts = 0;
    int top_attempts = 0;
    bool top_preparation_only = false;

    RadarMapStatus EnsureMap(RadarMapMode, const RadarMapLayers*, bool prepare_only) {
        CHECK_TRUE(prepare_only);
        ++layout_attempts;
        return RadarMapStatus::Ready;
    }

    RadarMapStatus EnsureTopMap(RadarMapMode, bool prepare_only, const RadarMapLayers*) {
        ++top_attempts;
        top_preparation_only = prepare_only;
        return top_status;
    }
};
} // namespace

RADAR_TEST(MapMode, BoundsConsoleValuesBeforeConvertingToAnInteger) {
    CHECK_EQ(RadarMapMode_FromCvar(std::numeric_limits<float>::max()), RadarMapMode::SliceFlat);
    CHECK_EQ(RadarMapMode_FromCvar(std::numeric_limits<float>::lowest()), RadarMapMode::OverviewTextured);
    CHECK_EQ(RadarMapMode_FromCvar(std::numeric_limits<float>::infinity()), RadarMapMode::SliceTextured);
    CHECK_EQ(RadarMapMode_FromCvar(std::numeric_limits<float>::quiet_NaN()), RadarMapMode::SliceTextured);
    CHECK_EQ(RadarMapMode_FromCvar(4.5f), RadarMapMode::SliceFlat);
}

RADAR_TEST(MapReadiness, PendingAnalysisUsesClassicUntilTopImageBecomesDrawable) {
    MapProvider map;
    constexpr RadarMapMode mode = RadarMapMode::SliceTextured;
    CHECK_EQ(RadarMapReadiness_PrepareLayered(map, mode, nullptr, RadarMapStatus::Waiting, false, true),
             RadarMapStatus::Waiting);
    CHECK_TRUE(map.top_preparation_only);
    CHECK_EQ(RadarMapReadiness_PrepareLayered(map, mode, nullptr, RadarMapStatus::Waiting, false, false),
             RadarMapStatus::Waiting);

    map.top_status = RadarMapStatus::Ready;
    CHECK_EQ(RadarMapReadiness_PrepareLayered(map, mode, nullptr, RadarMapStatus::Waiting, false, false),
             RadarMapStatus::Ready);
    CHECK_EQ(map.layout_attempts, 3);
    CHECK_EQ(map.top_attempts, 3);
}

RADAR_TEST(MapReadiness, CachedTopImageRemainsDrawableDuringLayerFailure) {
    MapProvider map;
    map.top_status = RadarMapStatus::Ready;
    constexpr RadarMapMode mode = RadarMapMode::SliceTextured;
    CHECK_EQ(RadarMapReadiness_PrepareLayered(map, mode, nullptr, RadarMapStatus::Failed, true, true),
             RadarMapStatus::Ready);
    CHECK_EQ(map.top_attempts, 1);

    map.top_status = RadarMapStatus::Failed;
    CHECK_EQ(RadarMapReadiness_PrepareLayered(map, mode, nullptr, RadarMapStatus::Failed, true, false),
             RadarMapStatus::Failed);
}

RADAR_TEST(MapReadiness, ReadyLayerLayoutDoesNotReplaceItWithPendingTopImage) {
    MapProvider map;
    CHECK_EQ(RadarMapReadiness_PrepareLayered(map, RadarMapMode::SliceTextured, nullptr, RadarMapStatus::Ready, false,
                                              false),
             RadarMapStatus::Ready);
    CHECK_EQ(map.layout_attempts, 1);
    CHECK_EQ(map.top_attempts, 0);
}
