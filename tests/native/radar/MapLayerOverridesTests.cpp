#include "Test.h"

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "hud/radar/map/layers/MapLayerOverrides.h"
#include "RadarMapLayerFixtures.h"
#include "hud/radar/map/layers/RadarMapLayers.h"

namespace {
std::vector<std::string> ExampleTokens() {
    return {
        "radar_layers",
        "{",
        "layer",
        "{",
        "name",
        "lower",
        "min_z",
        "-512",
        "max_z",
        "0",
        "}",
        "layer",
        "{",
        "name",
        "upper",
        "min_z",
        "0",
        "max_z",
        "512",
        "}",
        "region",
        "{",
        "layer",
        "upper",
        "mins",
        "128 -256 -64",
        "maxs",
        "512 256 128",
        "}",
        "}",
    };
}

int NamedLayer(const RadarMapLayers& map, const std::string& name) {
    for (const RadarLayerInfo& layer : map.get_layers()) {
        if (layer.name == name) {
            return layer.id;
        }
    }

    return -1;
}
} // namespace

RADAR_TEST(MapLayerOverrides, HalfOpenHeightRanges) {
    RadarLayerOverrides overrides;
    std::string error;
    std::vector<std::string> tokens = ExampleTokens();
    tokens.erase(tokens.begin() + 20, tokens.end() - 1);
    CHECK_TRUE(MapLayerOverrides_Parse(tokens, overrides, error)) << error;

    RadarMapLayerFixture fixture;
    fixture.AddFloor(-32.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    fixture.AddFloor(0.0f, {{128.0f, 0.0f}, {256.0f, 128.0f}});

    RadarMapLayers map;
    CHECK_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    CHECK_TRUE(map.ApplyOverrides(overrides, error)) << error;

    int lower = NamedLayer(map, "lower");
    int upper = NamedLayer(map, "upper");
    CHECK_GE(lower, 0);
    CHECK_GE(upper, 0);
    CHECK_EQ(map.Locate({64.0f, 64.0f, -31.0f}, RadarLayerQuery::Below).layer, lower);
    CHECK_EQ(map.Locate({192.0f, 64.0f, 1.0f}, RadarLayerQuery::Below).layer, upper);
}

RADAR_TEST(MapLayerOverrides, RegionOverridesHeightRange) {
    RadarLayerOverrides overrides;
    std::string error;

    CHECK_TRUE(MapLayerOverrides_Parse(ExampleTokens(), overrides, error)) << error;

    RadarMapLayerFixture fixture;
    fixture.AddFloor(-32.0f, {{0.0f, -320.0f}, {640.0f, 320.0f}});

    RadarMapLayers map;
    CHECK_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    CHECK_TRUE(map.ApplyOverrides(overrides, error)) << error;

    int lower = NamedLayer(map, "lower");
    int upper = NamedLayer(map, "upper");
    CHECK_GE(lower, 0);
    CHECK_GE(upper, 0);
    CHECK_EQ(map.Locate({256.0f, 0.0f, -31.0f}, RadarLayerQuery::Below).layer, upper);
    CHECK_EQ(map.Locate({64.0f, 0.0f, -31.0f}, RadarLayerQuery::Below).layer, lower);

    for (RadarLayerQuery query : {RadarLayerQuery::Below, RadarLayerQuery::Nearest}) {
        CHECK_EQ(map.Locate({128.0f, 0.0f, -31.0f}, query, lower).layer, upper);
        CHECK_EQ(map.Locate({512.0f, 0.0f, -31.0f}, query, upper).layer, lower);
        CHECK_EQ(map.Locate({256.0f, -256.0f, -31.0f}, query, lower).layer, upper);
        CHECK_EQ(map.Locate({256.0f, 256.0f, -31.0f}, query, upper).layer, lower);
    }
}

RADAR_TEST(MapLayerOverrides, RejectsConflictingRegionsAtomically) {
    RadarLayerOverrides overrides;
    std::string error;

    CHECK_TRUE(MapLayerOverrides_Parse(ExampleTokens(), overrides, error)) << error;

    RadarMapLayerFixture fixture;
    fixture.AddFloor(-32.0f, {{0.0f, -320.0f}, {640.0f, 320.0f}});

    RadarMapLayers map;
    CHECK_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    CHECK_TRUE(map.ApplyOverrides(overrides, error)) << error;

    int upper = NamedLayer(map, "upper");
    overrides.regions.push_back({"lower", {{256.0f, -128.0f, -64.0f}, {384.0f, 128.0f, 64.0f}}});
    CHECK_FALSE(map.ApplyOverrides(overrides, error));
    CHECK_FALSE(error.empty());
    CHECK_EQ(map.Locate({256.0f, 0.0f, -31.0f}, RadarLayerQuery::Below).layer, upper);
    CHECK_FLOAT_EQ(map.get_layers()[upper].area, 196608.0f);

    std::vector<std::string> tokens = ExampleTokens();
    tokens.insert(tokens.end() - 1, {
                                        "region",
                                        "{",
                                        "layer",
                                        "lower",
                                        "mins",
                                        "256 -128 -64",
                                        "maxs",
                                        "384 128 64",
                                        "}",
                                    });

    RadarLayerOverrides retained{{{"retained", -1.0f, 1.0f}}, {}};
    CHECK_FALSE(MapLayerOverrides_Parse(tokens, retained, error));
    CHECK_EQ(retained.layers.size(), 1u);
    CHECK_EQ(retained.layers[0].name, "retained");
}

RADAR_TEST(MapLayerOverrides, RejectsInvalidNumbersAndNames) {
    RadarLayerOverrides overrides{{{"retained", -1.0f, 1.0f}}, {}};
    std::string error;

    for (const std::string& invalid : {"nan", "inf", "1e100", "32junk", "", "0 1", "+-1", "-+1", "+"}) {
        std::vector<std::string> tokens = ExampleTokens();
        tokens[7] = invalid;
        CHECK_FALSE(MapLayerOverrides_Parse(tokens, overrides, error)) << invalid;
        CHECK_FALSE(error.empty());
        CHECK_EQ(overrides.layers.size(), 1u);
        CHECK_EQ(overrides.layers[0].name, "retained");
    }

    for (const std::string& invalid : {"", "lower"}) {
        std::vector<std::string> tokens = ExampleTokens();
        tokens[14] = invalid;
        CHECK_FALSE(MapLayerOverrides_Parse(tokens, overrides, error));
        CHECK_EQ(overrides.layers[0].name, "retained");
    }
}

RADAR_TEST(MapLayerOverrides, RejectsMalformedBlocksAtomically) {
    std::vector<std::vector<std::string>> invalid;
    std::vector<std::string> tokens = ExampleTokens();
    tokens.pop_back();
    invalid.push_back(tokens);
    tokens = ExampleTokens();
    tokens.erase(tokens.begin() + 18);
    invalid.push_back(tokens);
    tokens = ExampleTokens();
    tokens.push_back("}");
    invalid.push_back(tokens);
    tokens = ExampleTokens();
    tokens[13] = "min_z";
    invalid.push_back(tokens);
    tokens = ExampleTokens();
    tokens[26] = "unknown";
    invalid.push_back(tokens);

    RadarLayerOverrides overrides{{{"retained", -1.0f, 1.0f}}, {}};
    std::string error;

    for (const std::vector<std::string>& input : invalid) {
        CHECK_FALSE(MapLayerOverrides_Parse(input, overrides, error));
        CHECK_FALSE(error.empty());
        CHECK_EQ(overrides.layers.size(), 1u);
        CHECK_EQ(overrides.layers[0].name, "retained");
    }
}

RADAR_TEST(MapLayerOverrides, RejectsInvalidBoundsReferencesAndIntervals) {
    std::string error;

    for (const std::string& invalid : {"128 -256", "128 -256 nan", "128 -256 -64 0", "512 256 128"}) {
        std::vector<std::string> tokens = ExampleTokens();
        tokens[25] = invalid;

        RadarLayerOverrides overrides;
        CHECK_FALSE(MapLayerOverrides_Parse(tokens, overrides, error)) << invalid;
    }

    std::vector<std::string> tokens = ExampleTokens();
    tokens[23] = "missing";

    RadarLayerOverrides overrides;
    CHECK_FALSE(MapLayerOverrides_Parse(tokens, overrides, error));

    tokens = ExampleTokens();
    tokens[16] = "-1";
    CHECK_FALSE(MapLayerOverrides_Parse(tokens, overrides, error));
}

RADAR_TEST(MapLayerOverrides, AllowsSameLayerOverlapAndDifferentLayerBoundaryContact) {
    RadarLayerOverrides overrides;
    std::string error;

    CHECK_TRUE(MapLayerOverrides_Parse(ExampleTokens(), overrides, error)) << error;

    overrides.regions.push_back({"upper", {{256.0f, -128.0f, -64.0f}, {640.0f, 128.0f, 64.0f}}});
    overrides.regions.push_back({"lower", {{640.0f, -128.0f, -64.0f}, {768.0f, 128.0f, 64.0f}}});

    CHECK_TRUE(MapLayerOverrides_Validate(overrides, error)) << error;

    RadarMapLayerFixture fixture;
    fixture.AddFloor(-32.0f, {{0.0f, -320.0f}, {800.0f, 320.0f}});

    RadarMapLayers map;
    CHECK_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    CHECK_TRUE(map.ApplyOverrides(overrides, error)) << error;

    int lower = NamedLayer(map, "lower");
    int upper = NamedLayer(map, "upper");
    CHECK_EQ(map.Locate({600.0f, 0.0f, -31.0f}, RadarLayerQuery::Below).layer, upper);
    CHECK_EQ(map.Locate({640.0f, 0.0f, -31.0f}, RadarLayerQuery::Below, upper).layer, lower);
    CHECK_FLOAT_EQ(map.get_layers()[upper].area, 229376.0f);
    CHECK_FLOAT_EQ(map.get_layers()[lower].area, 282624.0f);
}

RADAR_TEST(MapLayerOverrides, RegionMaximumZExcludesHorizontalSupport) {
    RadarLayerOverrides overrides{
        {{"lower", -512.0f, 256.0f}, {"upper", 256.0f, 512.0f}},
        {{"upper", {{128.0f, -256.0f, -64.0f}, {512.0f, 256.0f, 128.0f}}}},
    };
    RadarMapLayerFixture fixture;
    fixture.AddFloor(-64.0f, {{0.0f, -320.0f}, {640.0f, 320.0f}});
    fixture.AddFloor(128.0f, {{0.0f, -320.0f}, {640.0f, 320.0f}});

    RadarMapLayers map;
    std::string error;

    CHECK_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    CHECK_TRUE(map.ApplyOverrides(overrides, error)) << error;
    CHECK_EQ(map.Locate({256.0f, 0.0f, -63.0f}, RadarLayerQuery::Below).layer, NamedLayer(map, "upper"));
    CHECK_EQ(map.Locate({256.0f, 0.0f, 129.0f}, RadarLayerQuery::Below).layer, NamedLayer(map, "lower"));
}

RADAR_TEST(MapLayerOverrides, DirectInvalidRecordsPreserveModel) {
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});

    RadarMapLayers map;
    CHECK_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);

    std::string error;
    std::vector<RadarLayerOverrides> invalid{
        {{{"duplicate", -512.0f, 0.0f}, {"duplicate", 0.0f, 512.0f}}, {}},
        {{{"layer", 0.0f, 0.0f}}, {}},
        {{{"layer", 0.0f, std::numeric_limits<float>::infinity()}}, {}},
        {{{"layer", 0.0f, 512.0f}}, {{"missing", {{0.0f, 0.0f, 0.0f}, {32.0f, 32.0f, 32.0f}}}}},
        {{{"layer", 0.0f, 512.0f}}, {{"layer", {{0.0f, 0.0f, 0.0f}, {32.0f, 32.0f, 0.0f}}}}},
    };

    for (const RadarLayerOverrides& overrides : invalid) {
        CHECK_FALSE(map.ApplyOverrides(overrides, error));
        CHECK_FALSE(error.empty());
        CHECK_EQ(map.get_layers().size(), 1u);
        CHECK_EQ(map.get_layers()[0].name, "Layer 1");
        CHECK_FLOAT_EQ(map.get_layers()[0].area, 16384.0f);
        CHECK_EQ(map.Locate({64.0f, 64.0f, 1.0f}, RadarLayerQuery::Below).layer, 0);
    }
}

RADAR_TEST(MapLayerOverrides, ReapplicationStartsFromAutomaticGeometry) {
    RadarMapLayerFixture fixture;
    fixture.AddFloor(-32.0f, {{0.0f, -320.0f}, {640.0f, 320.0f}});

    RadarMapLayers map;
    std::string error;
    RadarLayerOverrides overrides;

    CHECK_TRUE(MapLayerOverrides_Parse(ExampleTokens(), overrides, error));
    CHECK_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    CHECK_TRUE(map.ApplyOverrides(overrides, error));
    CHECK_TRUE(map.ApplyOverrides({{{"whole", -512.0f, 512.0f}}, {}}, error));
    CHECK_EQ(map.get_layers().size(), 1u);
    CHECK_EQ(map.get_layers()[0].name, "whole");
    CHECK_FLOAT_EQ(map.get_layers()[0].area, 409600.0f);

    CHECK_TRUE(map.ApplyOverrides({}, error));
    CHECK_EQ(map.get_layers().size(), 1u);
    CHECK_EQ(map.get_layers()[0].name, "Layer 1");
    CHECK_FLOAT_EQ(map.get_layers()[0].area, 409600.0f);
}

RADAR_TEST(MapLayerOverrides, HideBlocksAllowOverlappingBoundsWithoutLayers) {
    std::vector<std::string> tokens{
        "radar_layers", "{", "hide", "{",         "mins", "+0 -32 -64", "maxs", "64 32 128", "}",
        "hide",         "{", "maxs", "96 64 256", "mins", "32 0 0",     "}",    "}",
    };
    RadarLayerOverrides overrides;
    std::string error;

    CHECK_TRUE(MapLayerOverrides_Parse(tokens, overrides, error)) << error;
    CHECK_TRUE(overrides.layers.empty());
    CHECK_TRUE(overrides.regions.empty());
}

RADAR_TEST(MapLayerOverrides, InvalidHideBlocksPreserveParsedCorrections) {
    std::vector<std::vector<std::string>> invalid{
        {"hide", "{", "mins", "0 0 0", "}"},
        {"hide", "{", "maxs", "1 1 1", "}"},
        {"hide", "{", "mins", "0 0 0", "maxs", "1 1 1", "layer", "lower", "}"},
        {"hide", "{", "mins", "0 0 0", "mins", "1 1 1", "}"},
    };

    for (const std::string& maximum :
         {"0 1 1", "1 0 1", "1 1 0", "-1 1 1", "nan 1 1", "1 inf 1", "1 1 1e100", "1 1", "1 1 1 1"}) {
        invalid.push_back({"hide", "{", "mins", "0 0 0", "maxs", maximum, "}"});
    }

    RadarLayerOverrides overrides;
    std::string error;

    CHECK_TRUE(MapLayerOverrides_Parse(ExampleTokens(), overrides, error)) << error;

    for (const std::vector<std::string>& block : invalid) {
        std::vector<std::string> tokens = ExampleTokens();
        tokens.insert(tokens.end() - 1, block.begin(), block.end());
        CHECK_FALSE(MapLayerOverrides_Parse(tokens, overrides, error));
        CHECK_FALSE(error.empty());
        CHECK_EQ(overrides.layers.size(), 2u);
        CHECK_EQ(overrides.regions.size(), 1u);
        CHECK_EQ(overrides.layers[0].name, "lower");
    }
}

RADAR_TEST(MapLayerOverrides, CombinedBuildMatchesAutomaticThenOverrides) {
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
    fixture.AddFloor(128.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
    std::array<RadarLayerOverrides, 4> configurations{};
    configurations[1].hides.push_back({{0.0f, 0.0f, -1.0f}, {128.0f, 128.0f, 1.0f}});
    configurations[2].layers = {{"invalid", 1.0f, 0.0f}};
    configurations[3].layers = {{"lower", -16.0f, 64.0f}, {"upper", 64.0f, 256.0f}};
    for (const RadarLayerOverrides& overrides : configurations) {
        RadarMapLayers separate;
        RadarMapLayers combined;
        std::string error;
        std::string warning;
        CHECK_EQ(separate.Build(fixture.geometry), RadarMapStatus::Ready);
        bool applied = separate.ApplyOverrides(overrides, error);
        CHECK_EQ(combined.Build(fixture.geometry, overrides, warning), RadarMapStatus::Ready);
        CHECK_EQ(warning.empty(), applied);
        CHECK_EQ(combined.get_layers().size(), separate.get_layers().size());
        CHECK_EQ(combined.get_bounds().min, separate.get_bounds().min);
        CHECK_EQ(combined.get_bounds().max, separate.get_bounds().max);
        for (const RadarLayerInfo& layer : separate.get_layers()) {
            CHECK_EQ(combined.get_layers()[layer.id].name, layer.name);
            CHECK_EQ(combined.get_layers()[layer.id].height, layer.height);
            CHECK_EQ(combined.get_layers()[layer.id].area, layer.area);
            std::span<const RadarMapSurface> expected = separate.get_surfaces(layer.id);
            std::span<const RadarMapSurface> actual = combined.get_surfaces(layer.id);
            CHECK_EQ(actual.size(), expected.size());
            for (size_t i = 0; i < expected.size(); i++) {
                CHECK_EQ(actual[i].surface_id, expected[i].surface_id);
                CHECK_EQ(actual[i].vertices.size(), expected[i].vertices.size());
                for (size_t j = 0; j < expected[i].vertices.size(); j++) {
                    CHECK_EQ(actual[i].vertices[j].pos, expected[i].vertices[j].pos);
                    CHECK_EQ(actual[i].vertices[j].uv, expected[i].vertices[j].uv);
                    CHECK_EQ(actual[i].vertices[j].lightmap_uv, expected[i].vertices[j].lightmap_uv);
                }
            }
        }
        for (ncl_math::Vector3 point : {ncl_math::Vector3{32.0f, 32.0f, 1.0f}, {256.0f, 256.0f, 129.0f}}) {
            CHECK_EQ(combined.Locate(point, RadarLayerQuery::Below).layer,
                     separate.Locate(point, RadarLayerQuery::Below).layer);
        }
    }
}
