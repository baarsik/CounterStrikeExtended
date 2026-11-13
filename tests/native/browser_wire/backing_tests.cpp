#include "../../../GoldSrc-SDK/vgui2/src/native/StandaloneRaster.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "browser_backing_failure=%s\n", message);
        std::exit(1);
    }
}

int main() {
    // Each source row and column differs, so compact source reads cannot match backing coordinates.
    const std::vector<unsigned char> backing = {
        1, 2, 3, 255, 4, 5, 6, 254, 7, 8, 9, 253, 10, 11, 12, 252,
        13, 14, 15, 251, 16, 17, 18, 250, 19, 20, 21, 249, 22, 23, 24, 248,
        25, 26, 27, 247, 28, 29, 30, 246, 31, 32, 33, 245, 34, 35, 36, 244};
    std::vector<unsigned char> initial(backing.size(), 0);
    cse::vgui::RasterImage image;
    Require(image.Assign(initial.data(), 4, 3, false, false), "create CPU backing texture");
    Require(image.UpdateFromBacking(1, 1, backing.data(), 2, 2), "update non-origin dirty backing rectangle");
    const std::vector<unsigned char> expected = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 18, 17, 16, 250, 21, 20, 19, 249, 0, 0, 0, 0,
        0, 0, 0, 0, 30, 29, 28, 246, 33, 32, 31, 245, 0, 0, 0, 0};
    Require(image.pixels == expected, "full backing update uses source x/y and original row stride");
    const auto revision = image.revision;
    Require(image.UpdateFromBacking(0, 0, backing.data(), 0, 0) && image.revision == revision &&
        image.pixels == expected, "popup-only update is a harmless no-op");
    Require(!image.UpdateFromBacking(3, 2, backing.data(), 2, 1) && image.pixels == expected,
        "backing update rejects out-of-bounds rectangle");
    const unsigned char compact[] = {71, 72, 73, 74, 81, 82, 83, 84};
    Require(image.Update(3, 1, compact, 1, 2), "compact-image helper still accepts tightly packed rows");
    Require(image.pixels[28] == 73 && image.pixels[29] == 72 && image.pixels[30] == 71 &&
        image.pixels[31] == 74 && image.pixels[44] == 83 && image.pixels[45] == 82 &&
        image.pixels[46] == 81 && image.pixels[47] == 84, "compact-image helper preserves its source stride");
    std::puts("native_browser_backing_passed=1");
}
