#include "VGui1NativeFontFace.h"
#include "VGui1TextEncoding.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
namespace vgui {
class Font {
public:
    explicit Font(cse_vgui1::legacy::NativeFontFace& face) : face_(face) {}
    void getTextSize(const char*, int&, int&);
    void getCharABCwide(int character, int& a, int& b, int& c) {
        if (character < 0) {
            character += 256;
        }
        Require(face_.RawAbc(static_cast<unsigned int>(character), a, b, c), "actual native glyph ABC");
        a = (std::max)(a, 0);
    }
    int getTall() {
        return face_.Metrics().height;
    }
private:
    cse_vgui1::legacy::NativeFontFace& face_;
};
#include "font.inc"
}
int main(int argc, char** argv) {
    Require(argc == 3, "installed Arial and actual menu-title records");
    std::ifstream fontInput(argv[1], std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(fontInput)), {});
    FT_Library library = nullptr;
    FT_Face inspected = nullptr;
    Require(FT_Init_FreeType(&library) == 0 && FT_New_Memory_Face(library, bytes.data(),
        static_cast<FT_Long>(bytes.size()), 0, &inspected) == 0, "inspect actual Arial program");
    const std::string identity = FT_Get_Postscript_Name(inspected);
    FT_Done_Face(inspected);
    FT_Done_FreeType(library);
    cse_vgui1::FontRequest request;
    request.requestedName = identity;
    request.lookupIdentity = identity;
    // Retained valve/640_textscheme.txt, CommandMenu Text: Arial, 14px, weight0.
    constexpr int CommandFontPixels = 14;
    request.tall = CommandFontPixels;
    request.weight = 0;
    cse_vgui1::legacy::Result result{};
    int error = 0;
    std::string diagnostic;
    auto face = cse_vgui1::legacy::NativeFontFace::CreateOwned(bytes.data(), bytes.size(), 0,
        identity.c_str(), request, result, error, diagnostic);
    Require(face != nullptr, diagnostic.c_str());
    vgui::Font font(*face);
    std::ifstream input(argv[2], std::ios::binary);
    std::string name;
    std::string text;
    bool allFit = true;
    std::puts("token\twidth\tavailable\tresult\ttext");
    while (std::getline(input, name, '\0')) {
        Require(static_cast<bool>(std::getline(input, text, '\0')), "complete menu title");
        const std::string display = "  4  " + text;
        int wide = 0;
        int tall = 0;
        font.getTextSize(display.c_str(), wide, tall);
        constexpr int CameraMenuWidth = 200;
        constexpr int OptionsMenuWidth = 160;
        constexpr int BorderInsets = 4;
        const int available = (name.find("Spec_Mode") == 0 ? CameraMenuWidth : OptionsMenuWidth) - BorderInsets;
        const bool fits = wide <= available;
        allFit = allFit && fits;
        std::printf("%s\t%d\t%d\t%s\t%s\n", name.c_str(), wide, available,
            fits ? "fit" : "overflow", display.c_str());
    }
    return allFit ? 0 : 1;
}
