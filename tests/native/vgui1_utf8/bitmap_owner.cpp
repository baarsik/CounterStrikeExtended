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
std::vector<unsigned char> courierBytes;
std::string courierIdentity;
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
    if (request.resolutionProfile == cse_vgui1::legacy::FixedSysRoleProfileV1) {
        return cse_vgui1::legacy::FixedSysFontFace::CreateOwned(courierBytes.data(), courierBytes.size(), 0,
            courierIdentity.c_str(), request, result, error, diagnostic);
    }
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
    Require(argc == 4, "ordinary font, official bitmap and FixedSys donor inputs");
    fontBytes = Read(argv[1]);
    auto bitmap = Read(argv[2]);
    Require(!fontBytes.empty() && !bitmap.empty(), "actual font/bitmap resource bytes");
    FT_Library library = nullptr;
    FT_Face face = nullptr;
    Require(FT_Init_FreeType(&library) == 0 && FT_New_Memory_Face(library, fontBytes.data(),
        static_cast<FT_Long>(fontBytes.size()), 0, &face) == 0, "real donor identity");
    identity = FT_Get_Postscript_Name(face);
    FT_Done_Face(face);
    FT_Done_FreeType(library);
    courierBytes = Read(argv[3]);
    Require(!courierBytes.empty() && FT_Init_FreeType(&library) == 0 && FT_New_Memory_Face(library,
        courierBytes.data(), static_cast<FT_Long>(courierBytes.size()), 0, &face) == 0, "actual Courier donor input");
    courierIdentity = FT_Get_Postscript_Name(face);
    FT_Done_Face(face);
    FT_Done_FreeType(library);
    vgui::Font font("Arial", bitmap.data(), static_cast<int>(bitmap.size()), 20, 0, 0.0f,
        400, false, false, false, false);
    constexpr int Side = 64;
    constexpr int Channels = 4;
    std::vector<unsigned char> before(Side * Side * Channels, 0);
    font.getCharRGBA('A', 0, 0, Side, Side, before.data());
    const int bitmapHeight = font.getTall();
    const char* files[]{"/owned/utf8-real-font-donor"};
    Require(CseVgui1FontsBindFilesV1(files, 1, nullptr, 0) == CseVgui1FontResultV1::FilesBound,
        "actual owner resource admission");
    int width = 0;
    int height = 0;
    font.getTextSize(u8"Zażółć Привет", width, height);
    Require(width > 0 && height == bitmapHeight, "actual bitmap Font measures Unicode using retained cell height");
    for (int character : {0x416, 0x17c}) {
        std::vector<unsigned char> pixels(Side * Side * Channels, 0);
        font.getCharRGBA(character, 0, 0, Side, Side, pixels.data());
        Require(std::any_of(pixels.begin(), pixels.end(), [](unsigned char value) { return value != 0; }),
            "actual bitmap owner routes Unicode through native donor raster");
    }
    std::vector<unsigned char> after(before.size(), 0);
    font.getCharRGBA('A', 0, 0, Side, Side, after.data());
    Require(before == after && font.getTall() == bitmapHeight, "legacy TGA glyph pixels/height remain byte-exact");
    vgui::Font fixed("FixedSys", 18, 0, 0.0f, 400, false, false, false, false);
    int a = 0;
    int b = 0;
    int c = 0;
    fixed.getCharABCwide('A', a, b, c);
    Require(a + b + c == cse_vgui1::legacy::FixedSysCellWidthV1
        && fixed.getTall() == cse_vgui1::legacy::FixedSysCellHeightV1, "actual FixedSys byte cell metrics");
    for (int character : {0x416, 0x17c}) {
        fixed.getCharABCwide(character, a, b, c);
        std::vector<unsigned char> pixels(Side * Side * Channels, 0);
        fixed.getCharRGBA(character, 0, 0, Side, Side, pixels.data());
        Require(a + b + c > 0 && std::any_of(pixels.begin(), pixels.end(),
            [](unsigned char value) { return value != 0; }), "actual FixedSys owner uses a Unicode-capable donor");
    }
    fixed.getCharABCwide('A', a, b, c);
    Require(a + b + c == cse_vgui1::legacy::FixedSysCellWidthV1,
        "Cyrillic fallback preserves the original FixedSys byte advance");
    fixed.getCharABCwide(0xffff, a, b, c);
    std::vector<unsigned char> missing(Side * Side * Channels, 0);
    fixed.getCharRGBA(0xffff, 0, 0, Side, Side, missing.data());
    Require(b >= 0, "a missing Unicode glyph uses font notdef instead of aborting the process");
    Require(CseVgui1FontsReleaseFilesV1(nullptr, 0) == CseVgui1FontResultV1::FilesReleased,
        "actual owner releases donor provider cleanly");
    std::puts("PASS actual bitmap Font owner: Cyrillic/Polish donor ink and unchanged legacy TGA glyphs");
}
