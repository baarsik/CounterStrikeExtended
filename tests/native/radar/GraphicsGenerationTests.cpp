#include "Test.h"
#include "hud/radar/integration/graphics_retirement_hooks.h"

RADAR_TEST(GraphicsGeneration, RepeatedMapChangesReleaseOwnedResources) {
    constexpr std::uint64_t context_generation = 7;
    int live_resources = 0;
    int releases = 0;
    constexpr int map_changes = 12;

    for (int map = 0; map < map_changes; ++map) {
        ++live_resources;
        if (radar::integration::CanReleaseGraphics(context_generation, context_generation, true)) {
            --live_resources;
            ++releases;
        }
    }

    CHECK_EQ(live_resources, 0);
    CHECK_EQ(releases, map_changes);
}

RADAR_TEST(GraphicsGeneration, ReusedContextAddressesCannotReleaseNewContextObjects) {
    constexpr std::uint64_t retired_generation = 7;
    constexpr std::uint64_t replacement_generation = retired_generation + 1;
    CHECK_FALSE(radar::integration::CanReleaseGraphics(retired_generation, replacement_generation, true));
    CHECK_TRUE(radar::integration::CanReleaseGraphics(replacement_generation, replacement_generation, true));
}

RADAR_TEST(GraphicsGeneration, MissingProviderAndUnmatchedContextDiscardNames) {
    CHECK_FALSE(radar::integration::CanReleaseGraphics(0, 0, true));
    CHECK_FALSE(radar::integration::CanReleaseGraphics(7, 0, true));
    CHECK_FALSE(radar::integration::CanReleaseGraphics(7, 7, false));
}
