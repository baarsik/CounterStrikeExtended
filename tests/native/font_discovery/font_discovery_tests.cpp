#include "native/MacFontDiscovery.h"

#include <CoreText/CoreText.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

void CheckFace(FT_Library library, const cse::fonts::FontFile& file, FT_ULong character = 'A') {
    Require(!file.path.empty(), "discovery returns an owned path");
    Require(access(file.path.c_str(), R_OK) == 0, "font file is readable after discovery returns");
    FT_Face face = nullptr;
    Require(FT_New_Face(library, file.path.c_str(), file.faceIndex, &face) == 0,
        "discovered file and index open with pinned FreeType");
    const char* postScriptName = FT_Get_Postscript_Name(face);
    Require(postScriptName != nullptr && file.postScriptName == postScriptName,
        "FreeType face matches CoreText PostScript identity");
    Require(FT_Get_Char_Index(face, character) != 0, "selected face contains the requested character");
    FT_Done_Face(face);
}

void TestFamily(FT_Library library) {
    cse::fonts::FontFile file;
    Require(cse::fonts::FindMacFontFile(library, "Helvetica", 400, false, file), "find Helvetica family");
    Require(file.postScriptName == "Helvetica", "regular family resolves to regular face");
    CheckFace(library, file);
    std::printf("Helvetica: %s face=%ld ps=%s\n", file.path.c_str(), file.faceIndex, file.postScriptName.c_str());
}

void TestStyle(FT_Library library) {
    cse::fonts::FontFile file;
    Require(cse::fonts::FindMacFontFile(library, "Helvetica", 700, true, file), "find bold italic family face");
    Require(file.postScriptName == "Helvetica-BoldOblique", "requested bold italic selects its actual face");
    Require(file.faceIndex != 0, "collection style retains a nonzero face index");
    CheckFace(library, file);
    std::printf("Helvetica bold italic: %s face=%ld ps=%s\n",
        file.path.c_str(), file.faceIndex, file.postScriptName.c_str());
    Require(cse::fonts::FindMacFontFile(library, "Helvetica-BoldOblique", 700, true, file),
        "PostScript names are accepted for fallback cache identity");
    CheckFace(library, file);
}

void TestUnicode(FT_Library library) {
    constexpr const char* fixture = CSE_UNICODE_FONT_FIXTURE;
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(fixture), std::strlen(fixture), false);
    Require(url != nullptr, "create local font fixture URL");
    Require(CTFontManagerRegisterFontsForURL(url, kCTFontManagerScopeProcess, nullptr),
        "register Unicode fixture for this process only");
    cse::fonts::FontFile file;
    Require(cse::fonts::FindMacFontFile(library, u8"CSE 字体 Fixture with a long Unicode family name", 400, false, file),
        "discover registered non-ASCII family by its UTF-8 name");
    Require(file.postScriptName == "CSEUnicodeFixture-Regular", "Unicode family finds the fixture face");
    CheckFace(library, file);
    Require(CTFontManagerUnregisterFontsForURL(url, kCTFontManagerScopeProcess, nullptr),
        "unregister process-local Unicode fixture");
    CFRelease(url);
    CheckFace(library, file);
    Require(!cse::fonts::FindMacFontFile(library, u8"CSE 字体 Fixture with a long Unicode family name", 400, false, file),
        "registration lifetime is respected on subsequent lookup");
}

void TestInvalid(FT_Library library) {
    cse::fonts::FontFile file;
    Require(cse::fonts::FindMacFontFile(library, "Helvetica", 400, false, file), "seed output for failure reset");
    Require(!cse::fonts::FindMacFontFile(library, "CSE Font That Does Not Exist 87921", 400, false, file),
        "unknown family fails without silent substitution");
    Require(file.path.empty() && file.postScriptName.empty() && file.faceIndex == 0, "failure clears old output");
    Require(!cse::fonts::FindMacFontFile(library, nullptr, 400, false, file), "null family fails");
    Require(!cse::fonts::FindMacFontFile(library, "", 400, false, file), "empty family fails");
    Require(!cse::fonts::FindMacFontFile(library, "\xc0\xaf", 400, false, file), "malformed UTF-8 family fails");
    constexpr std::size_t oversizedNameBytes = 1025;
    const std::string oversizedName(oversizedNameBytes, 'A');
    Require(!cse::fonts::FindMacFontFile(library, oversizedName.c_str(), 400, false, file), "oversized family fails");
    Require(!cse::fonts::FindMacFontFile(nullptr, "Helvetica", 400, false, file), "null FreeType library fails");
}

void TestFallback(FT_Library library) {
    constexpr std::uint16_t chineseCharacter = 0x4e2d;
    cse::fonts::FontFile file;
    Require(cse::fonts::FindMacFontFallback(library, "Helvetica", 400, false, &chineseCharacter, 1, file),
        "discover CJK fallback for Helvetica");
    Require(file.postScriptName != "Helvetica", "CJK character selects a covering fallback face");
    CheckFace(library, file, chineseCharacter);
    std::printf("CJK fallback: %s face=%ld ps=%s\n", file.path.c_str(), file.faceIndex, file.postScriptName.c_str());
    constexpr std::uint16_t latinCharacter = 'A';
    Require(cse::fonts::FindMacFontFallback(library, "Helvetica", 400, false, &latinCharacter, 1, file),
        "fallback retains primary when primary covers the scalar");
    Require(file.postScriptName == "Helvetica", "covered scalar keeps primary face");
    CheckFace(library, file);
    Require(cse::fonts::FindMacFontFallback(library, "Helvetica", 700, true, &chineseCharacter, 1, file),
        "discover CJK fallback from a bold italic primary");
    CheckFace(library, file, chineseCharacter);
}

void TestUtf16(FT_Library library) {
    constexpr std::uint16_t smile[] = {0xd83d, 0xde42};
    constexpr FT_ULong smileScalar = 0x1f642;
    cse::fonts::FontFile file;
    Require(cse::fonts::FindMacFontFallback(library, "Helvetica", 400, false, smile, 2, file),
        "surrogate pair discovers supplementary scalar fallback");
    CheckFace(library, file, smileScalar);
    Require(!cse::fonts::FindMacFontFallback(library, "Helvetica", 400, false, smile, 1, file),
        "isolated high surrogate fails");
    Require(!cse::fonts::FindMacFontFallback(library, "Helvetica", 400, false, smile + 1, 1, file),
        "isolated low surrogate fails");
    constexpr std::uint16_t wrongPair[] = {0xd83d, 'A'};
    Require(!cse::fonts::FindMacFontFallback(library, "Helvetica", 400, false, wrongPair, 2, file),
        "malformed surrogate pair fails");
    Require(!cse::fonts::FindMacFontFallback(library, "Helvetica", 400, false, nullptr, 1, file),
        "null UTF-16 input fails");
    Require(!cse::fonts::FindMacFontFallback(library, "Helvetica", 400, false, smile, 0, file),
        "empty UTF-16 input fails");
    constexpr std::uint16_t twoScalars[] = {'A', 'B'};
    Require(!cse::fonts::FindMacFontFallback(library, "Helvetica", 400, false, twoScalars, 2, file),
        "multiple BMP scalars fail the single-scalar contract");
    constexpr std::uint16_t noncharacter[] = {0xdbff, 0xdfff};
    Require(!cse::fonts::FindMacFontFallback(library, "Helvetica", 400, false, noncharacter, 2, file),
        "unsupported noncharacter fails without LastResort substitution");
    Require(file.path.empty(), "UTF-16 rejection clears previous result");
}

void TestLifetime(FT_Library library) {
    cse::fonts::FontFile original;
    Require(cse::fonts::FindMacFontFile(library, "Helvetica", 700, true, original), "retain independent owned result");
    constexpr int repetitions = 128;
    for (int iteration = 0; iteration < repetitions; ++iteration) {
        TestFallback(library);
        TestInvalid(library);
    }
    CheckFace(library, original);
}

}

int main(int argc, char** argv) {
    Require(argc == 2, "one fixture scenario is required");
    FT_Library library = nullptr;
    Require(FT_Init_FreeType(&library) == 0, "initialize pinned FreeType");
    FT_Int major = 0;
    FT_Int minor = 0;
    FT_Int patch = 0;
    FT_Library_Version(library, &major, &minor, &patch);
    Require(major == 2 && minor == 14 && patch == 3, "fixture uses pinned FreeType 2.14.3");
    const std::string scenario = argv[1];
    if (scenario == "family") {
        TestFamily(library);
    } else if (scenario == "style") {
        TestStyle(library);
    } else if (scenario == "unicode") {
        TestUnicode(library);
    } else if (scenario == "invalid") {
        TestInvalid(library);
    } else if (scenario == "fallback") {
        TestFallback(library);
    } else if (scenario == "utf16") {
        TestUtf16(library);
    } else if (scenario == "lifetime") {
        TestLifetime(library);
    } else {
        Require(false, "unknown fixture scenario");
    }
    FT_Done_FreeType(library);
    return EXIT_SUCCESS;
}
