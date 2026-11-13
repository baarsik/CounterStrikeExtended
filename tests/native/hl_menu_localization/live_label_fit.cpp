#include "VGUI_Font.h"
#include "VGui1MacFontProvider.h"
#include "VGui1FontServices.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace cse_vgui1 {
const FontProviderV1& CseVgui1QualifiedFontProviderV1() noexcept {
    return CseVgui1MacFontProviderCandidateV1();
}
}
namespace vgui {
void CseVgui1SelectFontForSurfaceV1(SurfacePlat*, BaseFontPlat*) { std::abort(); }
}
struct MetricFont final : vgui::Font { using vgui::Font::Font; };
struct OwnedFont {
    std::vector<unsigned char> bytes;
    std::unique_ptr<MetricFont> font;
};
int main(int argc, char** argv) {
    assert(argc == 2);
    assert(CseVgui1FontsBindFilesV1(nullptr, 0, nullptr, 0) == CseVgui1FontResultV1::FilesBound);
    std::ifstream input(argv[1], std::ios::binary);
    std::map<std::string, OwnedFont> fonts;
    std::string fields[9];
    std::puts("language\tresolution\tcontext\ttoken\tprovider\twidth\tbound_width\theight\tbound_height\tresult\ttext");
    while (std::getline(input, fields[0], '\0')) {
        for (int index = 1; index < 9; ++index) {
            assert(std::getline(input, fields[index], '\0'));
        }
        for (bool bitmap : {true, false}) {
            const std::string key = fields[4] + (bitmap ? "bitmap" : "native");
            auto& owned = fonts[key];
            if (!owned.font) {
                if (bitmap) {
                    std::ifstream image(fields[4], std::ios::binary);
                    owned.bytes.assign(std::istreambuf_iterator<char>(image), {});
                    assert(!owned.bytes.empty());
                }
                owned.font.reset(new MetricFont("Arial", owned.bytes.empty() ? nullptr : owned.bytes.data(),
                    static_cast<int>(owned.bytes.size()), std::stoi(fields[5]), 0, 0.0f, 0,
                    false, false, false, false));
            }
            int wide = 0;
            int tall = 0;
            owned.font->getTextSize(fields[8].c_str(), wide, tall);
            const int width = std::stoi(fields[6]);
            const int height = std::stoi(fields[7]);
            const bool fits = wide <= width && tall <= height;
            std::printf("%s\t%s\t%s\t%s\t%s\t%d\t%d\t%d\t%d\t%s\t%s\n",
                fields[0].c_str(), fields[1].c_str(), fields[2].c_str(), fields[3].c_str(),
                bitmap ? "retained-bitmap+native-unicode" : "native", wide, width, tall, height,
                fits ? "fit" : "overflow", fields[8].c_str());
        }
    }
    return 0; // Receipt reports all overflows; Python determines the assertion policy.
}
