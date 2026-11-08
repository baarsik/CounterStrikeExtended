#include "FontManager.h"
#include "LinuxFont.h"

#include <CoreText/CoreText.h>

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
