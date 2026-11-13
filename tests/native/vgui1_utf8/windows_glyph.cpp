// Typed GDI receiver for actual extracted Win32 FontPlat methods. This checks
// ANSI/Unicode API choice on this host; actual Windows GDI runtime is separate.
#include "VGui1TextEncoding.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

using uchar = unsigned char;
constexpr int GLYPH_COUNT = 256;
constexpr int VGUI_RGBA_BYTES = 4;
constexpr int OPAQUE = 2;
constexpr int TRANSPARENT = 1;
struct ABC {
    int abcA = 0;
    unsigned int abcB = 0;
    int abcC = 0;
};
struct Metrics {
    int tmHeight = 4;
};
bool unicodeDraw = false;
int drawn = 0;
int unicodeMetric = 0;
void SelectObject(int, int) {}
void SetBkColor(int, int) {}
void SetTextColor(int, int) {}
void SetBkMode(int, int) {}
void MoveToEx(int, int, int, void*) {}
int RGB(int, int, int) { return 0; }
void ExtTextOutA(int, int, int, int, void*, const char* text, int, void*) {
    unicodeDraw = false;
    drawn = static_cast<unsigned char>(*text);
}
void ExtTextOutW(int, int, int, int, void*, const wchar_t* text, int, void*) {
    unicodeDraw = true;
    drawn = *text;
}
bool GetCharABCWidthsW(int, int first, int, ABC* abc) {
    unicodeMetric = first;
    abc->abcB = 17;
    return true;
}
struct FontPlat {
    int _dc = 0;
    int _font = 0;
    int _bufferWide = 32;
    int _bufferTall = 4;
    Metrics _metrics;
    ABC _abc[GLYPH_COUNT];
    uchar storage[32 * 4 * VGUI_RGBA_BYTES]{};
    uchar* _pixels = storage;
#include "windows-methods.inc"
};

int main() {
    FontPlat font;
    font._abc[0xf3].abcB = 9; // Deliberately differs from U+00F3 (e.g. ACP1251 byte у).
    constexpr char PolishAccent[] = u8"ó";
    const cse_vgui1::text::Run run(PolishAccent, sizeof(PolishAccent) - 1);
    const auto glyph = run.GlyphAt(0);
    auto abc = font.CharacterMetrics(glyph.value);
    uchar output[32 * 4 * VGUI_RGBA_BYTES]{};
    font.getCharRGBA(glyph.value, 0, 0, 32, 4, output);
    if (!run.IsUtf8() || !cse_vgui1::text::IsUnicodeByteGlyph(glyph.value)
        || abc.abcB != 17 || unicodeMetric != 0xf3 || !unicodeDraw || drawn != 0xf3) {
        std::fputs("FAILED actual Win32 glyph methods: UTF8 ó must use W metrics and raster\n", stderr);
        return 1;
    }
    unicodeMetric = 0;
    abc = font.CharacterMetrics(0xf3);
    font.getCharRGBA(0xf3, 0, 0, 32, 4, output);
    if (abc.abcB != 9 || unicodeMetric != 0 || unicodeDraw || drawn != 0xf3) {
        std::fputs("FAILED actual Win32 glyph methods: old byte must preserve A metrics/raster\n", stderr);
        return 1;
    }
    std::puts("PASS actual Win32 glyph method receiver: UTF8 ó uses Unicode; legacy byte keeps ANSI");
}
