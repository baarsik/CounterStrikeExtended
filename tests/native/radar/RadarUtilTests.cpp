#include "Test.h"

#include <string>
#include <vector>

#include "hud/radar/integration/RadarUtil.h"

namespace {
std::string ExtractMapName(const char* level) {
    char out[64];
    RadarUtil_ExtractMapName(level, out);
    return out;
}
} // namespace

RADAR_TEST(RadarUtilExtractMapName, StripsPathAndExtension) {
    CHECK_EQ(ExtractMapName("maps/de_dust2.bsp"), "de_dust2");
    CHECK_EQ(ExtractMapName("de_dust2.bsp"), "de_dust2");
    CHECK_EQ(ExtractMapName("maps/cs_office.bsp"), "cs_office");
    CHECK_EQ(ExtractMapName("maps/sub/dir/foo.bsp"), "foo");
    CHECK_EQ(ExtractMapName("noext"), "noext");
    CHECK_EQ(ExtractMapName(""), "");
}

RADAR_TEST(RadarUtilExtractMapName, RespectsOutputSize) {
    char out[4];
    RadarUtil_ExtractMapName("maps/abcdef.bsp", out);
    CHECK_STREQ(out, "abc");
}

RADAR_TEST(RadarUtilSiteLetter, UnderscoreSuffix) {
    CHECK_EQ(RadarUtil_DetectSiteLetter("tgt_a"), 'A');
    CHECK_EQ(RadarUtil_DetectSiteLetter("bomb_B"), 'B');
}

RADAR_TEST(RadarUtilSiteLetter, UnderscorePrefix) {
    CHECK_EQ(RadarUtil_DetectSiteLetter("a_crates"), 'A');
    CHECK_EQ(RadarUtil_DetectSiteLetter("b_boxes"), 'B');
}

RADAR_TEST(RadarUtilSiteLetter, TrailingLetter) {
    CHECK_EQ(RadarUtil_DetectSiteLetter("targeta"), 'A');
    CHECK_EQ(RadarUtil_DetectSiteLetter("cratesb"), 'B');
}

RADAR_TEST(RadarUtilSiteLetter, WholeName) {
    CHECK_EQ(RadarUtil_DetectSiteLetter("a"), 'A');
    CHECK_EQ(RadarUtil_DetectSiteLetter("B"), 'B');
}

RADAR_TEST(RadarUtilHostageModel, AModelTheMapGivesItsHostagesIsAHostage) {
    // cs_italy and cs_militia give every hostage the Half-Life scientist.
    std::vector<std::string> map_models{"models/scientist.mdl"};

    CHECK_TRUE(RadarUtil_IsHostageModel("models/scientist.mdl", map_models));
}

RADAR_TEST(RadarUtilHostageModel, TheEngineMayKeepAnotherSpelling) {
    std::vector<std::string> map_models{"models/Scientist.mdl"};

    CHECK_TRUE(RadarUtil_IsHostageModel("models/scientist.mdl", map_models));
}

RADAR_TEST(RadarUtilHostageModel, AModelSharingAPrefixIsNotAHostage) {
    std::vector<std::string> map_models{"models/scientist.mdl"};

    CHECK_FALSE(RadarUtil_IsHostageModel("models/scientist2.mdl", map_models));
    CHECK_FALSE(RadarUtil_IsHostageModel("models/scient.mdl", map_models));
}

RADAR_TEST(RadarUtilHostageModel, AHostageNamedModelNeedsNoMapEntry) {
    // A server running CZ hostages swaps in models/hostageA.mdl..hostageD.mdl whatever the map says.
    CHECK_TRUE(RadarUtil_IsHostageModel("models/hostageA.mdl", {}));
}

RADAR_TEST(RadarUtilHostageModel, AScientistOffTheMapsListIsAProp) {
    CHECK_FALSE(RadarUtil_IsHostageModel("models/scientist.mdl", {}));
}

RADAR_TEST(RadarUtilSiteLetter, NoneWhenAbsent) {
    CHECK_EQ(RadarUtil_DetectSiteLetter(""), 0);
    CHECK_EQ(RadarUtil_DetectSiteLetter("xyz"), 0);
    CHECK_EQ(RadarUtil_DetectSiteLetter("bombsite"), 0); // ends in 'e'
    CHECK_EQ(RadarUtil_DetectSiteLetter("c"), 0);
}
