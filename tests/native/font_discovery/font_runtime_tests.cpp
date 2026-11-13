#include "FontManager.h"
#include "LinuxFont.h"
#include "native/MacFontDiscovery.h"

#include <CoreText/CoreText.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BITMAP_H

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

constexpr int fontHeight = 24;
constexpr int fontWeight = 400;
constexpr int antialiasFlag = 0x010;
constexpr int dropShadowFlag = 0x080;

void TestRasterFlags() {
    constexpr int underlineFlag = 0x002;
    constexpr int strikeoutFlag = 0x004;
    constexpr int channels = 4;
    constexpr int canvasWidth = 64;
    constexpr int canvasHeight = 64;
    constexpr unsigned char guard = 0xa5;
    CLinuxFont plain;
    CLinuxFont antialiased;
    Require(plain.Create("Helvetica", fontHeight, fontWeight, 0, 0, 0), "create monochrome font");
    Require(antialiased.Create("Helvetica", fontHeight, fontWeight, 0, 0, antialiasFlag),
        "create antialiased font");
    std::vector<unsigned char> mono(canvasWidth * canvasHeight * channels);
    std::vector<unsigned char> gray(mono.size());
    plain.GetCharRGBA('S', 0, 0, canvasWidth, canvasHeight, mono.data());
    antialiased.GetCharRGBA('S', 0, 0, canvasWidth, canvasHeight, gray.data());
    bool monoInk = false;
    bool grayEdge = false;
    for (std::size_t offset = channels - 1; offset < mono.size(); offset += channels) {
        Require(mono[offset] == 0 || mono[offset] == 255, "disabled antialiasing yields binary coverage");
        monoInk = monoInk || mono[offset] != 0;
        grayEdge = grayEdge || (gray[offset] > 0 && gray[offset] < 255);
    }
    Require(monoInk && grayEdge, "antialias selection changes actual edge coverage");

    // FreeType's own converter is the independent oracle for packed mono rows.
    FT_Library library = nullptr;
    FT_Face face = nullptr;
    Require(FT_Init_FreeType(&library) == 0, "initialize independent FreeType library");
    cse::fonts::FontFile selected;
    Require(cse::fonts::FindMacFontFile(library, "Helvetica", fontWeight, false, selected),
        "independently select the native collection face");
    Require(FT_New_Face(library, selected.path.c_str(), selected.faceIndex, &face) == 0,
        "open independent FreeType face");
    Require(FT_Set_Pixel_Sizes(face, 0, fontHeight - 1) == 0 &&
        FT_Load_Char(face, 'S', FT_LOAD_RENDER | FT_LOAD_TARGET_MONO | FT_LOAD_MONOCHROME) == 0,
        "render independent monochrome bitmap");
    FT_Bitmap converted;
    FT_Bitmap_Init(&converted);
    Require(FT_Bitmap_Convert(library, &face->glyph->bitmap, &converted, 1) == 0,
        "expand packed rows using FreeType bitmap conversion");
    for (unsigned int y = 0; y < converted.rows; ++y) {
        for (unsigned int x = 0; x < converted.width; ++x) {
            const int targetX = face->glyph->bitmap_left + static_cast<int>(x);
            const int targetY = plain.GetAscent() - face->glyph->bitmap_top + static_cast<int>(y);
            if (targetX >= 0 && targetY >= 0 && targetX < canvasWidth && targetY < canvasHeight) {
                const unsigned char expected = converted.buffer[y * converted.pitch + x] ? 255 : 0;
                Require(mono[(targetY * canvasWidth + targetX) * channels + channels - 1] == expected,
                    "native raster preserves each converted mono bitmap sample and row pitch");
            }
        }
    }
    FT_Bitmap_Done(library, &converted);
    FT_Done_Face(face);
    FT_Done_FreeType(library);

    for (const int flags : {underlineFlag, strikeoutFlag, underlineFlag | strikeoutFlag}) {
        CLinuxFont decorated;
        Require(decorated.Create("Helvetica", fontHeight, fontWeight, 0, 0, flags),
            "create requested text decoration");
        std::vector<unsigned char> pixels(mono.size());
        decorated.GetCharRGBA(' ', 0, 0, canvasWidth, canvasHeight, pixels.data());
        int a = 0;
        int b = 0;
        int c = 0;
        decorated.GetCharABCWidths(' ', a, b, c);
        bool aboveBaseline = false;
        bool belowBaseline = false;
        for (int y = 0; y < decorated.GetHeight(); ++y) {
            int ink = 0;
            for (int x = 0; x < a + b + c; ++x) {
                ink += pixels[(y * canvasWidth + x) * channels + channels - 1] != 0;
            }
            if (ink) {
                Require(ink == a + b + c, "decoration spans the complete space advance");
                aboveBaseline = aboveBaseline || y < decorated.GetAscent();
                belowBaseline = belowBaseline || y >= decorated.GetAscent();
            }
        }
        Require(aboveBaseline == ((flags & strikeoutFlag) != 0), "strikeout appears above the baseline");
        Require(belowBaseline == ((flags & underlineFlag) != 0), "underline appears below the baseline");
    }

    constexpr int offsetX = 3;
    constexpr int offsetY = 5;
    std::vector<unsigned char> translated(mono.size() + 2, guard);
    std::fill(translated.begin() + 1, translated.end() - 1, 0);
    plain.GetCharRGBA('S', offsetX, offsetY, canvasWidth, canvasHeight, translated.data() + 1);
    for (int y = 0; y < canvasHeight - offsetY; ++y) {
        for (int x = 0; x < canvasWidth - offsetX; ++x) {
            Require(std::memcmp(mono.data() + (y * canvasWidth + x) * channels,
                translated.data() + 1 + ((y + offsetY) * canvasWidth + x + offsetX) * channels, channels) == 0,
                "raster canvas offsets preserve glyph coverage");
        }
    }
    Require(translated.front() == guard && translated.back() == guard, "raster respects caller byte span");
}

void CheckRaster(CLinuxFont* font, int character) {
    Require(font != nullptr, "font exists for the requested character");
    constexpr int textureWidth = 64;
    constexpr int textureHeight = 64;
    constexpr int channels = 4;
    std::vector<unsigned char> pixels(textureWidth * textureHeight * channels);
    font->GetCharRGBA(character, 0, 0, textureWidth, textureHeight, pixels.data());
    bool visible = false;
    for (std::size_t offset = channels - 1; offset < pixels.size(); offset += channels) {
        visible = visible || pixels[offset] != 0;
    }
    Require(visible, "existing FreeType rasterizer produces nonempty glyph alpha");
}

void TestInvalid() {
    CLinuxFont missing;
    Require(!missing.Create("CSE Font That Does Not Exist 87921", fontHeight, fontWeight, 0, 0, antialiasFlag),
        "macOS font creation rejects unknown names without Fontconfig substitution");
    Require(!missing.IsValid(), "failed font remains invalid");
}

void TestRendering() {
    CLinuxFont font;
    Require(font.Create("Helvetica", fontHeight, fontWeight, 0, 0, antialiasFlag), "create native-discovered font");
    Require(font.HasChar(L'A'), "primary face covers Latin text");
    CheckRaster(&font, 'A');
    CLinuxFont effected;
    Require(effected.Create("Helvetica", fontHeight, fontWeight, 2, 2, antialiasFlag | dropShadowFlag),
        "create font with existing blur, scanline and shadow effects");
    CheckRaster(&effected, 'A');
    Require(font.Create("Helvetica", fontHeight, 700, 0, 0, antialiasFlag), "recreate existing font with another face");
    CheckRaster(&font, 'A');
    Require(font.Create(font.GetName(), fontHeight, fontWeight, 0, 0, antialiasFlag),
        "recreation safely accepts this font's owned name");
    CheckRaster(&font, 'A');
    Require(!font.Create(nullptr, fontHeight, fontWeight, 0, 0, antialiasFlag), "failed recreation rejects null name");
    Require(!font.IsValid(), "failed recreation discards the old face");
}

void TestFallback() {
    CFontManager& manager = FontManager();
    const vgui2::HFont handle = manager.CreateFont();
    Require(manager.AddGlyphSetToFont(handle, "Helvetica", fontHeight, fontWeight, 0, 0, antialiasFlag),
        "font manager creates primary glyph set without Linux family replacements");
    CLinuxFont* latin = manager.GetFontForChar(handle, L'A');
    Require(latin && std::strcmp(latin->GetName(), "Helvetica") == 0, "Latin text retains requested primary font");
    constexpr wchar_t chineseCharacter = 0x4e2d;
    CLinuxFont* chinese = manager.GetFontForChar(handle, chineseCharacter);
    Require(chinese != nullptr && chinese != latin, "font manager lazily resolves a native covering CJK face");
    Require(chinese->HasChar(chineseCharacter), "native fallback contains the requested glyph");
    Require(manager.GetFontForChar(handle, chineseCharacter) == chinese, "repeated fallback reuses an owned font");
    CheckRaster(chinese, chineseCharacter);
    int a = 0;
    int b = 0;
    int c = 0;
    manager.GetCharABCwide(handle, chineseCharacter, a, b, c);
    Require(a + b + c > 0, "metrics use the same native fallback font as rendering");
    const vgui2::HFont styledHandle = manager.CreateFont();
    constexpr int italicFlag = 0x001;
    Require(manager.AddGlyphSetToFont(styledHandle, "Helvetica", fontHeight, 700, 0, 0, antialiasFlag | italicFlag),
        "font manager creates bold italic primary");
    CheckRaster(manager.GetFontForChar(styledHandle, chineseCharacter), chineseCharacter);
    manager.ClearAllFonts();
}

void TestUnicode() {
    constexpr const char* fixture = CSE_UNICODE_FONT_FIXTURE;
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(fixture), std::strlen(fixture), false);
    Require(url && CTFontManagerRegisterFontsForURL(url, kCTFontManagerScopeProcess, nullptr),
        "register local Unicode font for runtime fixture");
    {
        CLinuxFont font;
        constexpr const char* family = u8"CSE 字体 Fixture with a long Unicode family name";
        Require(font.Create(family, fontHeight, fontWeight, 0, 0, antialiasFlag),
            "runtime font creation accepts a Unicode family");
        Require(font.IsEqualTo(family, fontHeight, fontWeight, 0, 0, antialiasFlag),
            "runtime font cache preserves UTF-8 family identity");
        char discoveredPath[4096] = {};
        Require(font.FindFontDataFile(family, fontHeight, fontWeight, 0, 0, antialiasFlag,
            discoveredPath, sizeof(discoveredPath)), "runtime lookup discovers registered fixture file");
        Require(std::strcmp(discoveredPath, fixture) == 0, "Unicode lookup uses fixture rather than substituted data");
        char shortPath[4] = {'x', 'x', 'x', 'x'};
        Require(!font.FindFontDataFile(family, fontHeight, fontWeight, 0, 0, antialiasFlag,
            shortPath, sizeof(shortPath)), "runtime lookup rejects truncated output paths");
        Require(shortPath[0] == '\0', "path truncation failure clears caller output");
        Require(CTFontManagerUnregisterFontsForURL(url, kCTFontManagerScopeProcess, nullptr),
            "remove process font registration while FreeType owns a face");
        CheckRaster(&font, 'A');
    }
    CFRelease(url);
}

void TestLifetime() {
    constexpr int repetitions = 32;
    for (int iteration = 0; iteration < repetitions; ++iteration) {
        TestFallback();
        TestInvalid();
    }
}

}

int main(int argc, char** argv) {
    Require(argc == 2, "one runtime fixture scenario is required");
    const std::string scenario = argv[1];
    if (scenario == "invalid") {
        TestInvalid();
    } else if (scenario == "rendering") {
        TestRendering();
    } else if (scenario == "raster-flags") {
        TestRasterFlags();
    } else if (scenario == "fallback") {
        TestFallback();
    } else if (scenario == "unicode") {
        TestUnicode();
    } else if (scenario == "lifetime") {
        TestLifetime();
    } else {
        Require(false, "unknown runtime fixture scenario");
    }
    return EXIT_SUCCESS;
}
