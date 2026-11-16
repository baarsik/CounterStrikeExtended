#include <algorithm>
#include <memory>

#include "Test.h"
#include "mathlib.h"
#include "const.h"
#include <taskcoro/CancellationToken.h>

#include "hud/radar/map/layers/RadarMapLayers.h"
#include "RadarMapLayerFixtures.h"

namespace {
RadarMapLayerFixture RoofFixture() {
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    fixture.AddFloor(96.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    fixture.geometry.entry_points.push_back({32.0f, 32.0f, 18.0f});
    fixture.geometry.collision_hulls.push_back({
        {{{0.0f, 0.0f, 1.0f}, 46.0f}, {{0.0f, 0.0f, 1.0f}, 114.0f}},
        {{0, {1, CONTENTS_EMPTY}}, {1, {CONTENTS_EMPTY, CONTENTS_SOLID}}},
        0,
        {},
    });
    return fixture;
}

bool ContainsSource(const RadarMapLayers& map, int source) {
    for (const RadarLayerInfo& layer : map.get_layers()) {
        std::span<const RadarMapSurface> surfaces = map.get_surfaces(layer.id);
        if (std::any_of(surfaces.begin(), surfaces.end(),
                        [source](const RadarMapSurface& surface) { return surface.surface_id == source; })) {
            return true;
        }
    }
    return false;
}
} // namespace

RADAR_TEST(MapReachability, ExcludesRoofInDisconnectedEmptyCollisionRegion) {
    RadarMapLayerFixture fixture = RoofFixture();
    CHECK_TRUE(MapGeometry_CheckClearance(fixture.geometry, {32.0f, 32.0f, 96.001f}));

    RadarMapLayers map;
    CHECK_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    CHECK_TRUE(ContainsSource(map, 0));
    CHECK_FALSE(ContainsSource(map, 1));
}

RADAR_TEST(MapReachability, ExcludesCompletelyBlockedUpwardRoof) {
    RadarMapLayerFixture fixture = RoofFixture();
    fixture.geometry.collision_hulls[0].nodes[0].children[0] = CONTENTS_SOLID;

    RadarMapLayers map;
    CHECK_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    CHECK_TRUE(ContainsSource(map, 0));
    CHECK_FALSE(ContainsSource(map, 1));
}

RADAR_TEST(MapReachability, SeparateEntryPointKeepsUpperRegion) {
    RadarMapLayerFixture fixture = RoofFixture();
    fixture.geometry.entry_points.push_back({32.0f, 32.0f, 128.0f});

    RadarMapLayers map;
    CHECK_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    CHECK_TRUE(ContainsSource(map, 0));
    CHECK_TRUE(ContainsSource(map, 1));
}

RADAR_TEST(MapReachability, MissingEntryPointsKeepDisconnectedFreeRegions) {
    RadarMapLayerFixture fixture = RoofFixture();
    fixture.geometry.entry_points.clear();

    RadarMapReachability result = MapReachability_Analyze(fixture.geometry);
    CHECK_TRUE(result.available);
    CHECK_TRUE(result.excluded_surfaces.empty());

    RadarMapLayers map;
    CHECK_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    CHECK_TRUE(ContainsSource(map, 0));
    CHECK_TRUE(ContainsSource(map, 1));
}

RADAR_TEST(MapReachability, MissingEntryPointsExcludeBlockedRoof) {
    RadarMapLayerFixture fixture = RoofFixture();
    fixture.geometry.entry_points.clear();
    fixture.geometry.collision_hulls[0].nodes[0].children[0] = CONTENTS_SOLID;

    RadarMapReachability result = MapReachability_Analyze(fixture.geometry);
    CHECK_TRUE(result.available);
    CHECK_EQ(result.excluded_surfaces, (std::vector<int>{1}));

    RadarMapLayers map;
    CHECK_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    CHECK_TRUE(ContainsSource(map, 0));
    CHECK_FALSE(ContainsSource(map, 1));
}

RADAR_TEST(MapReachability, MissingEntryPointsKeepOpenGround) {
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    fixture.AddSupportHull(0);

    RadarMapReachability result = MapReachability_Analyze(fixture.geometry);
    CHECK_TRUE(result.available);
    CHECK_TRUE(result.excluded_surfaces.empty());

    RadarMapLayers map;
    CHECK_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    CHECK_TRUE(ContainsSource(map, 0));
    CHECK_TRUE(map.Locate({64.0f, 64.0f, 1.0f}, RadarLayerQuery::Below).contained);
}

RADAR_TEST(MapReachability, MissingEntryPointsKeepSlopedRaisedContact) {
    for (float rise : {0.0f, 10.0f}) {
        RADAR_TRACE(rise);
        RadarMapLayerFixture fixture;
        fixture.AddRamp({0.0f, 0.0f, 0.0f}, {128.0f, 0.0f, 32.0f}, 128.0f);
        fixture.AddSupportHull(0);
        fixture.geometry.collision_hulls[0].planes[0].dist += rise;

        RadarMapReachability result = MapReachability_Analyze(fixture.geometry);
        CHECK_TRUE(result.available);
        CHECK_TRUE(result.excluded_surfaces.empty());
    }
}

RADAR_TEST(MapReachability, MissingEntryPointsPreserveGeometryWithoutCollisionHull) {
    RadarMapLayerFixture fixture = RoofFixture();
    fixture.geometry.entry_points.clear();
    fixture.geometry.collision_hulls.clear();

    RadarMapReachability result = MapReachability_Analyze(fixture.geometry);
    CHECK_FALSE(result.available);
    CHECK_TRUE(result.excluded_surfaces.empty());

    RadarMapLayers map;
    CHECK_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    CHECK_TRUE(ContainsSource(map, 0));
    CHECK_TRUE(ContainsSource(map, 1));
}

RADAR_TEST(MapReachability, MissingEntryPointsDoNotAcceptContactBelowThePlayerCenter) {
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    fixture.geometry.collision_hulls.push_back({
        {{{0.0f, 0.0f, 1.0f}, 16.0f}},
        {{0, {CONTENTS_SOLID, CONTENTS_EMPTY}}},
        0,
        {},
    });

    RadarMapReachability result = MapReachability_Analyze(fixture.geometry);
    CHECK_TRUE(result.available);
    CHECK_EQ(result.excluded_surfaces, (std::vector<int>{0}));
}

RADAR_TEST(MapReachability, BlockedEntryPointsPreserveGeometry) {
    RadarMapLayerFixture fixture = RoofFixture();
    fixture.geometry.entry_points = {{32.0f, 32.0f, 80.0f}};

    RadarMapLayers map;
    CHECK_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    CHECK_TRUE(ContainsSource(map, 0));
    CHECK_TRUE(ContainsSource(map, 1));
}

RADAR_TEST(MapReachability, NarrowCollisionPortalConnectsUpperRegion) {
    RadarMapLayerFixture fixture = RoofFixture();
    RadarCollisionHull& hull = fixture.geometry.collision_hulls[0];
    hull.planes = {{{1.0f, 0.0f, 0.0f}, 60.0f},
                   {{1.0f, 0.0f, 0.0f}, 62.0f},
                   {{0.0f, 0.0f, 1.0f}, 46.0f},
                   {{0.0f, 0.0f, 1.0f}, 114.0f}};
    hull.nodes = {
        {0, {1, 2}}, {1, {2, CONTENTS_EMPTY}}, {2, {3, CONTENTS_EMPTY}}, {3, {CONTENTS_EMPTY, CONTENTS_SOLID}}};

    RadarMapLayers map;
    CHECK_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    CHECK_TRUE(ContainsSource(map, 0));
    CHECK_TRUE(ContainsSource(map, 1));
}

RADAR_TEST(MapReachability, ConnectedFaceKeepsItsCompleteSourceContour) {
    RadarMapLayerFixture fixture = RoofFixture();
    RadarCollisionHull& hull = fixture.geometry.collision_hulls[0];
    hull.planes.insert(hull.planes.begin(), {{1.0f, 0.0f, 0.0f}, 64.0f});
    hull.nodes = {{0, {CONTENTS_EMPTY, 1}}, {1, {2, CONTENTS_EMPTY}}, {2, {CONTENTS_EMPTY, CONTENTS_SOLID}}};

    RadarMapLayers map;
    CHECK_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    CHECK_TRUE(ContainsSource(map, 1));
    std::span<const RadarMapSurface> surfaces = map.get_surfaces(0);
    auto roof = std::find_if(surfaces.begin(), surfaces.end(),
                             [](const RadarMapSurface& surface) { return surface.surface_id == 1; });
    CHECK_NE(roof, surfaces.end());
    CHECK_EQ(roof->vertices.size(), 4u);
    CHECK_EQ(roof->vertices[0].pos, (ncl_math::Vector3{0.0f, 0.0f, 96.0f}));
    CHECK_EQ(roof->vertices[2].pos, (ncl_math::Vector3{128.0f, 128.0f, 96.0f}));
}

RADAR_TEST(MapReachability, EntryPointsIncludeSpawnsAndTeleportDestinations) {
    std::vector<ncl_math::Vector3> entries;
    CHECK_TRUE(MapReachability_ReadEntryPoints(
        R"( { "classname" "worldspawn" } { "origin" "+1 -2 3.5" "classname" "info_player_start" } { "classname" "info_player_deathmatch" "origin" "4 5 6" } { "classname" "info_player_coop" "origin" "7 8 9" } { "classname" "info_teleport_destination" "origin" "10 11 12" } { "classname" "light" "origin" "13 14 15" }
    )",
        -18.0f, entries));
    CHECK_EQ(entries.size(), 4u);
    CHECK_EQ(entries[0], (ncl_math::Vector3{1.0f, -2.0f, 3.5f}));
    CHECK_EQ(entries[3], (ncl_math::Vector3{10.0f, 11.0f, 31.0f}));
}

RADAR_TEST(MapReachability, InvalidEntryDataClearsAllSeeds) {
    for (
        std::string_view entities : {
            R"({ "classname" "info_player_start" "origin" "1 2 3" } { "classname" "info_player_start" "origin" "1 nan 3" })",
            R"({ "classname" "info_player_start" })",
            R"({ "classname" "info_player_start" "origin" "1 2 3 4" })",
            R"({ "classname" "info_player_start" "origin" "1 2 3" } "unclosed)",
        }) {
        RADAR_TRACE(entities);
        std::vector<ncl_math::Vector3> entries;
        CHECK_FALSE(MapReachability_ReadEntryPoints(entities, -18.0f, entries));
        CHECK_TRUE(entries.empty());
    }
}

RADAR_TEST(MapReachability, TeleportTargetsSeedTheirPlayerContactPosition) {
    std::vector<ncl_math::Vector3> entries;
    CHECK_TRUE(MapReachability_ReadEntryPoints(
        R"( { "classname" "info_target" "targetname" "exit" "origin" "32 32 96" } { "classname" "trigger_teleport" "target" "exit" } { "classname" "info_target" "targetname" "decoration" "origin" "32 32 200" }
    )",
        -18.0f, entries));
    CHECK_EQ(entries.size(), 1u);
    CHECK_EQ(entries[0], (ncl_math::Vector3{32.0f, 32.0f, 115.0f}));

    RadarMapLayerFixture fixture = RoofFixture();
    fixture.geometry.entry_points.insert(fixture.geometry.entry_points.end(), entries.begin(), entries.end());
    RadarMapLayers map;
    CHECK_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    CHECK_TRUE(ContainsSource(map, 1));
}

RADAR_TEST(MapReachability, CancellationDoesNotPublishAnExclusionList) {
    for (bool with_entries : {true, false}) {
        RADAR_TRACE(with_entries);
        RadarMapLayerFixture fixture = RoofFixture();

        if (!with_entries) {
            fixture.geometry.entry_points.clear();
        }

        std::shared_ptr<taskcoro::CancellationToken> token = taskcoro::CancellationToken::Create();
        token->SetCanceled();

        RadarMapReachability result = MapReachability_Analyze(fixture.geometry, token.get());
        CHECK_FALSE(result.available);
        CHECK_TRUE(result.excluded_surfaces.empty());
    }
}

RADAR_TEST(MapReachability, KeepsGroundWithRaisedCollisionFloor) {
    for (float rise : {4.0f, 6.0f, 10.0f, 18.0f}) {
        RADAR_TRACE(rise);
        RadarMapLayerFixture fixture;
        fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
        fixture.AddSupportHull(0);
        fixture.geometry.collision_hulls[0].planes[0].dist += rise;
        fixture.geometry.entry_points.push_back({32.0f, 32.0f, 64.0f});

        RadarMapReachability result = MapReachability_Analyze(fixture.geometry);
        CHECK_TRUE(result.available);
        CHECK_TRUE(result.excluded_surfaces.empty());
    }
}

RADAR_TEST(MapReachability, ExcludesGroundBeyondTheContactBand) {
    for (float rise : {19.0f, 64.0f}) {
        RADAR_TRACE(rise);
        RadarMapLayerFixture fixture;
        fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
        fixture.AddSupportHull(0);
        fixture.geometry.collision_hulls[0].planes[0].dist += rise;
        fixture.geometry.entry_points.push_back({32.0f, 32.0f, 96.0f});

        RadarMapReachability result = MapReachability_Analyze(fixture.geometry);
        CHECK_TRUE(result.available);
        CHECK_EQ(result.excluded_surfaces, (std::vector<int>{0}));
    }
}

RADAR_TEST(MapReachability, KeepsContactInsideANarrowConnectedHeightInterval) {
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    fixture.geometry.entry_points.push_back({32.0f, 32.0f, 22.5f});
    fixture.geometry.collision_hulls.push_back({
        {{{0.0f, 0.0f, 1.0f}, 22.0f}, {{0.0f, 0.0f, 1.0f}, 23.0f}},
        {{0, {1, CONTENTS_SOLID}}, {1, {CONTENTS_SOLID, CONTENTS_EMPTY}}},
        0,
        {},
    });

    RadarMapReachability result = MapReachability_Analyze(fixture.geometry);
    CHECK_TRUE(result.available);
    CHECK_TRUE(result.excluded_surfaces.empty());
}

RADAR_TEST(MapReachability, DoesNotExtendContactBelowTheNominalCenter) {
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    fixture.geometry.entry_points.push_back({32.0f, 32.0f, 0.0f});
    fixture.geometry.collision_hulls.push_back({
        {{{0.0f, 0.0f, 1.0f}, 16.0f}},
        {{0, {CONTENTS_SOLID, CONTENTS_EMPTY}}},
        0,
        {},
    });

    RadarMapReachability result = MapReachability_Analyze(fixture.geometry);
    CHECK_TRUE(result.available);
    CHECK_EQ(result.excluded_surfaces, (std::vector<int>{0}));
}

RADAR_TEST(MapReachability, RaisedContactDoesNotConnectADisconnectedRoof) {
    RadarMapLayerFixture fixture = RoofFixture();
    fixture.geometry.collision_hulls[0].planes[1].dist = 124.0f;

    RadarMapReachability result = MapReachability_Analyze(fixture.geometry);
    CHECK_TRUE(result.available);
    CHECK_EQ(result.excluded_surfaces, (std::vector<int>{1}));
}

RADAR_TEST(MapReachability, RaisedGroundRemainsASupportInExplicitLayers) {
    for (float rise : {4.0f, 6.0f, 10.0f}) {
        RADAR_TRACE(rise);
        RadarMapLayerFixture fixture = RoofFixture();
        RadarCollisionHull& hull = fixture.geometry.collision_hulls[0];
        hull.planes.insert(hull.planes.begin(), {{0.0f, 0.0f, 1.0f}, 18.0f + rise});
        hull.nodes = {
            {0, {1, CONTENTS_SOLID}},
            {1, {2, CONTENTS_EMPTY}},
            {2, {CONTENTS_EMPTY, CONTENTS_SOLID}},
        };
        fixture.geometry.entry_points = {{32.0f, 32.0f, 32.0f}, {32.0f, 32.0f, 128.0f}};

        RadarMapLayers map;
        CHECK_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
        std::string error;
        CHECK_TRUE(map.ApplyOverrides({{{"lower", -1.0f, 48.0f}, {"upper", 48.0f, 160.0f}}, {}}, error)) << error;
        CHECK_EQ(map.get_layers().size(), 2u);
        RadarLayerMatch match = map.Locate({64.0f, 64.0f, rise + 1.0f}, RadarLayerQuery::Below);
        CHECK_EQ(match.layer, 0);
        CHECK_EQ(match.surface_id, 0);
        CHECK_TRUE(match.contained);
        CHECK_TRUE(std::any_of(map.get_surfaces(0).begin(), map.get_surfaces(0).end(),
                               [](const RadarMapSurface& surface) { return surface.surface_id == 0; }));
    }
}

RADAR_TEST(MapReachability, RaisedGroundRetainsSignificantAutomaticOverlap) {
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
    fixture.AddFloor(96.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
    fixture.geometry.entry_points = {{32.0f, 32.0f, 32.0f}, {32.0f, 32.0f, 128.0f}};
    fixture.geometry.collision_hulls.push_back({
        {{{0.0f, 0.0f, 1.0f}, 28.0f}, {{0.0f, 0.0f, 1.0f}, 46.0f}, {{0.0f, 0.0f, 1.0f}, 114.0f}},
        {{0, {1, CONTENTS_SOLID}}, {1, {2, CONTENTS_EMPTY}}, {2, {CONTENTS_EMPTY, CONTENTS_SOLID}}},
        0,
        {},
    });

    RadarMapLayers map;
    CHECK_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    CHECK_EQ(map.get_layers().size(), 2u);
    CHECK_EQ(map.Locate({64.0f, 64.0f, 11.0f}, RadarLayerQuery::Below).layer, 0);
    CHECK_EQ(map.Locate({64.0f, 64.0f, 97.0f}, RadarLayerQuery::Below).layer, 1);
    CHECK_FLOAT_EQ(map.get_analysis().largest_overlap_area, 262144.0f);
}

RADAR_TEST(MapReachability, SkyCappedSupportsDoNotCreateSelectableLayers) {
    for (bool raised_collision : {false, true}) {
        RADAR_TRACE(raised_collision);
        RadarMapLayerFixture fixture = RoofFixture();
        fixture.geometry.surfaces.clear();
        fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
        fixture.AddFloor(96.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
        fixture.AddFloor(128.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
        fixture.geometry.surfaces.back().sky = true;
        fixture.geometry.collision_hulls[0].planes[1].dist = raised_collision ? 124.0f : 114.0f;
        fixture.geometry.entry_points.push_back({32.0f, 32.0f, 128.0f});

        RadarMapLayers map;
        CHECK_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
        CHECK_EQ(map.get_layers().size(), 1u);
        CHECK_EQ(map.Locate({64.0f, 64.0f, 97.0f}, RadarLayerQuery::Below).surface_id, 0);
        CHECK_EQ(map.get_analysis().significant_overlap_regions, 0u);
        CHECK_TRUE(ContainsSource(map, 0));
        CHECK_FALSE(ContainsSource(map, 1));
    }
}
