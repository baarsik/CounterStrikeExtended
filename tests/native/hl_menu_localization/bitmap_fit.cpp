#include "VGUI_Font.h"
#include "VGui1FontPlatform.h"
#include "VGui1FontServices.h"
#include "VGui1NativeFontFace.h"
#include "VGui1FixedSysFontFace.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {
std::vector<unsigned char> fontBytes;
std::string identity;
void Require(bool value, const char* text) {
    if (!value) {
        std::fprintf(stderr, "FAILED actual bitmap Unicode owner: %s\n", text);
        std::exit(1);
    }
}
std::vector<unsigned char> Read(const char* path) {
    std::ifstream stream(path, std::ios::binary);
    return {(std::istreambuf_iterator<char>(stream)), {}};
}
void* Create() noexcept {
    return new int(0);
}
void Destroy(void* context) noexcept {
    delete static_cast<int*>(context);
}
int Register(void*, const void*, const char*) noexcept {
    return 0;
}
int Release(void*, const void*) noexcept {
    return 0;
}
std::unique_ptr<cse_vgui1::RealizedFace> Realize(void*, const cse_vgui1::FontRequest& request,
    const std::vector<std::string>&, int& error, std::string& diagnostic) noexcept {
    cse_vgui1::legacy::Result result{};
    return cse_vgui1::legacy::NativeFontFace::CreateOwned(fontBytes.data(), fontBytes.size(), 0,
        identity.c_str(), request, result, error, diagnostic);
}
}
namespace cse_vgui1 {
const FontProviderV1& CseVgui1QualifiedFontProviderV1() noexcept {
    // Fixture discovery only; glyph metrics/rasterization use the actual native
    // face on the installed Arial program, including the bitmap fallback path.
    static const FontProviderV1 provider{Create, Destroy, Register, Release, Realize};
    return provider;
}
}
namespace vgui {
void CseVgui1SelectFontForSurfaceV1(SurfacePlat*, BaseFontPlat*) {
    std::abort();
}
}

int main(int argc, char** argv) {
    Require(argc == 4, "actual Arial, retained CommandMenu bitmap, and menu titles");
    fontBytes = Read(argv[1]);
    auto bitmap = Read(argv[2]);
    FT_Library library = nullptr;
    FT_Face face = nullptr;
    Require(FT_Init_FreeType(&library) == 0 && FT_New_Memory_Face(library, fontBytes.data(),
        static_cast<FT_Long>(fontBytes.size()), 0, &face) == 0, "actual donor identity");
    identity = FT_Get_Postscript_Name(face);
    FT_Done_Face(face);
    FT_Done_FreeType(library);
    const char* files[]{"/owned/utf8-command-font-donor"};
    Require(CseVgui1FontsBindFilesV1(files, 1, nullptr, 0) == CseVgui1FontResultV1::FilesBound,
        "bind actual native font services");
    constexpr int CommandFontPixels = 14;
    vgui::Font font("Arial", bitmap.data(), static_cast<int>(bitmap.size()), CommandFontPixels, 0, 0.0f,
        0, false, false, false, false);
    std::printf("# actual retained bitmap cell height: %d\n", font.getTall());
    std::ifstream input(argv[3], std::ios::binary);
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
