#include "VGUI_Font.h"
#include "VGui1FontServices.h"
#include <array>
#include <climits>
#include <cstring>

int main(int argc, char** argv) {
    if (argc != 3) {
        return 2;
    }
    const bool pending = std::strcmp(argv[1], "pending") == 0;
    const bool unavailable = std::strcmp(argv[1], "unavailable") == 0;
    const char* paths[]{argv[2]};
    if (!pending && CseVgui1FontsBindFilesV1(paths, unavailable ? 0 : 1, nullptr, 0)
        != CseVgui1FontResultV1::FilesBound) {
        return 3;
    }
    vgui::Font font("Owned Face", 20, 0, 0.0f, 400, false, false, false, false);
    if (std::strcmp(argv[1], "released") == 0) {
        if (font.getTall() != 5
            || CseVgui1FontsReleaseFilesV1(nullptr, 0) != CseVgui1FontResultV1::FilesReleased) {
            return 4;
        }
    }
    if (std::strcmp(argv[1], "glyph") == 0) {
        int a = 0;
        int b = 0;
        int c = 0;
        font.getCharABCwide(256, a, b, c);
    } else if (std::strcmp(argv[1], "canvas") == 0) {
        std::array<unsigned char, 4> pixel{};
        font.getCharRGBA('A', 0, 0, INT_MAX, INT_MAX, pixel.data());
    } else {
        font.getTall();
    }
    return 5; // A guard that accidentally returns is a failed control.
}
