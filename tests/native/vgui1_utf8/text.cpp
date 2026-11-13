#include "VGui1NativeFontFace.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

// The fixture compiles the actual three production methods. Only the receiver
// replaces window/App dependencies; metric/raster calls use the real native face.
#include "VGui1TextEncoding.h"

namespace {
void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "FAILED VGUI1 UTF8: %s\n", message);
        std::exit(1);
    }
}
}

namespace vgui {
class Panel {};
class Color {
public:
    void getColor(int& red, int& green, int& blue, int& alpha) {
        red = green = blue = 255;
        alpha = 0;
    }
};
class Font {
public:
    explicit Font(cse_vgui1::legacy::NativeFontFace& face) : face_(face) {}
    void getTextSize(const char* text, int& wide, int& tall);
    void getCharABCwide(int character, int& a, int& b, int& c) {
        if (character < 0) {
            character += 256;
        }
        Require(face_.RawAbc(static_cast<unsigned int>(character), a, b, c), "real font ABC glyph access");
        a = std::max(a, 0);
    }
    int getTall() {
        return face_.Metrics().height;
    }
private:
    cse_vgui1::legacy::NativeFontFace& face_;
};
struct Printed {
    int x;
    int y;
    std::string bytes;
};
class TextImage {
public:
    TextImage(Font& font, const char* text, int available) : font_(font), _text(text), available_(available) {}
    void getTextSizeWrapped(int& wide, int& tall);
    void paint(Panel* panel);
    void getSize(int& wide, int& tall) {
        wide = available_;
        tall = 1000;
    }
    Font* getFont() {
        return &font_;
    }
    void getColor(Color&) {}
    void drawSetTextColor(int, int, int, int) {}
    void drawSetTextFont(Font*) {}
    void drawPrintChar(int x, int y, char ch) {
        printed.push_back({x, y, std::string(1, ch)});
    }
    void drawPrintText(int x, int y, const char* bytes, int length) {
        printed.push_back({x, y, std::string(bytes, static_cast<std::size_t>(length))});
    }
    std::vector<Printed> printed;
private:
    Font& font_;
    const char* _text;
    int available_;
};
#include "methods.inc"
}

int main(int argc, char** argv) {
    Require(argc == 2, "ordinary donor font input argument");
    std::ifstream stream(argv[1], std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(stream)), {});
    Require(!bytes.empty(), "real installed font bytes");
    FT_Library library = nullptr;
    FT_Face inspected = nullptr;
    Require(FT_Init_FreeType(&library) == 0 && FT_New_Memory_Face(library, bytes.data(),
        static_cast<FT_Long>(bytes.size()), 0, &inspected) == 0, "inspect exact real font identity");
    const std::string identity = FT_Get_Postscript_Name(inspected);
    FT_Done_Face(inspected);
    FT_Done_FreeType(library);
    cse_vgui1::FontRequest request;
    request.requestedName = identity;
    request.lookupIdentity = identity;
    request.tall = 20;
    request.weight = 400;
    cse_vgui1::legacy::Result result{};
    int error = 0;
    std::string diagnostic;
    auto face = cse_vgui1::legacy::NativeFontFace::CreateOwned(bytes.data(), bytes.size(), 0,
        identity.c_str(), request, result, error, diagnostic);
    Require(face != nullptr, diagnostic.c_str());
    const char truncated[]{static_cast<char>(0xe2), static_cast<char>(0x82)};
    Require(!cse_vgui1::text::Run(truncated, sizeof(truncated)).IsUtf8(), "truncated UTF8 is a legacy run");
    const char surrogate[]{static_cast<char>(0xed), static_cast<char>(0xa0), static_cast<char>(0x80)};
    Require(!cse_vgui1::text::Run(surrogate, sizeof(surrogate)).IsUtf8(), "surrogate UTF8 is a legacy run");
    const char overlong[]{static_cast<char>(0xc0), static_cast<char>(0xaf)};
    Require(!cse_vgui1::text::Run(overlong, sizeof(overlong)).IsUtf8(), "overlong UTF8 is a legacy run");
    const char supplementary[]{static_cast<char>(0xf0), static_cast<char>(0x9f),
        static_cast<char>(0x98), static_cast<char>(0x80)};
    const cse_vgui1::text::Run supplementaryRun(supplementary, sizeof(supplementary));
    Require(supplementaryRun.IsUtf8() && supplementaryRun.At(0).value == 0xfffd
        && supplementaryRun.At(0).bytes == sizeof(supplementary), "supplementary text consumes one complete replacement");
    vgui::Font font(*face);
    int width = 0;
    int height = 0;
    // Removing UTF8 decoding from the production Font/TextImage methods causes
    // this glyph-count/width/paint assertion to fail on actual translated text.
    font.getTextSize(u8"Привет", width, height);
    int expected = 0;
    for (const unsigned int character : {0x41fU, 0x440U, 0x438U, 0x432U, 0x435U, 0x442U}) {
        int a = 0;
        int b = 0;
        int c = 0;
        Require(face->GlyphIndex(character) != 0 && face->RawAbc(character, a, b, c),
            "Cyrillic selects a real Unicode donor glyph");
        expected += std::max(a, 0) + b + c;
    }
    Require(width == expected && height == font.getTall(), "Cyrillic width uses six Unicode glyph advances");
    vgui::TextImage image(font, u8"Zażółć gęślą jaźń", 1000);
    image.paint(nullptr);
    Require(image.printed.size() == 15, "Polish paints fifteen nonspace Unicode glyphs");
    Require(image.printed[2].bytes == u8"ż" && image.printed[3].bytes == u8"ó",
        "paint preserves each original UTF8 scalar run instead of a truncated char");
    int firstWord = 0;
    int secondWord = 0;
    font.getTextSize(u8"Привет", firstWord, height);
    font.getTextSize(u8"мир", secondWord, height);
    const int available = std::max(firstWord, secondWord);
    vgui::TextImage wrapped(font, u8"Привет мир", available);
    wrapped.getTextSizeWrapped(width, height);
    wrapped.paint(nullptr);
    Require(width == available && height == 2 * font.getTall() && wrapped.printed.size() == 9,
        "actual wrapped extent contains two complete Cyrillic words");
    Require(wrapped.printed[0].y == 0 && wrapped.printed[6].y == font.getTall(),
        "paint and measurement agree on actual Unicode word wrap");
    const char legacy[]{static_cast<char>(0xe9), static_cast<char>(0x80), 0};
    font.getTextSize(legacy, width, height);
    int legacyWidth = 0;
    for (unsigned int character : {0xe9U, 0x80U}) {
        int a = 0;
        int b = 0;
        int c = 0;
        font.getCharABCwide(static_cast<int>(character), a, b, c);
        legacyWidth += a + b + c;
    }
    Require(width == legacyWidth, "invalid UTF8 CP1252 bytes retain legacy font advances");
    vgui::TextImage legacyImage(font, legacy, 1000);
    legacyImage.paint(nullptr);
    Require(legacyImage.printed.size() == 2 && legacyImage.printed[0].bytes == std::string(1, legacy[0]),
        "legacy CP1252 paint retains original bytes");
    const char mixed[]{static_cast<char>(0xff), static_cast<char>(0xc5), static_cast<char>(0xbc), 0};
    vgui::TextImage mixedImage(font, mixed, 1000);
    mixedImage.paint(nullptr);
    Require(mixedImage.printed.size() == 3, "invalid UTF8 selects legacy for the entire string");
    vgui::TextImage ascii(font, "one\r\ntwo", 1000);
    ascii.getTextSizeWrapped(width, height);
    ascii.paint(nullptr);
    Require(ascii.printed.size() == 6 && ascii.printed[3].y == font.getTall()
        && height == 2 * font.getTall(), "ASCII CRLF wrapped behavior remains unchanged");
    vgui::TextImage empty(font, "", 1000);
    empty.getTextSizeWrapped(width, height);
    empty.paint(nullptr);
    Require(width == 0 && height == 0 && empty.printed.empty(), "empty wrapped image remains empty");
    const int side = 64;
    const int channels = 4;
    std::vector<unsigned char> previous;
    for (unsigned int character : {0x416U, 0x17cU}) {
        std::vector<unsigned char> pixels(static_cast<std::size_t>(side) * side * channels, 0);
        Require(face->GlyphIndex(character) != 0 && face->Raster(character, 0, 0, side, side, pixels.data()),
            "real font renders a Cyrillic/Polish glyph rather than a missing byte cell");
        bool nonempty = false;
        for (std::size_t index = channels - 1; index < pixels.size(); index += channels) {
            nonempty = nonempty || pixels[index] != 0;
        }
        Require(nonempty && pixels != previous, "actual Unicode glyph raster has distinct visible ink");
        previous = std::move(pixels);
    }
    std::puts("PASS VGUI1 UTF8 real Arial: glyphs, widths, raster, scalar paint, wrapping and CP1252 regression");
}
