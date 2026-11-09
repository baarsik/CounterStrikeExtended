#include <Windows.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>
#include "VGUI_Font.h"
#include "fileimage.h"
#include "vfontdata.h"
#include "vgui_win32.h"

namespace {
constexpr int kGlyphCount = 256;
constexpr int kMetricHeight = 5;
constexpr int kMaximumWidth = 6;
constexpr int kAverageWidth = 3;
constexpr int kPixelBytes = 4;
struct Request {
    int height;
    int width;
    int escapement;
    int orientation;
    int weight;
    DWORD italic;
    DWORD underline;
    DWORD strikeout;
    DWORD charset;
};
std::vector<Request> requests;
std::vector<std::unique_ptr<unsigned char[]>> bitmaps;
bool abcFailure = false;
int lastMoveX = 0;
int outputCalls = 0;
int dcTag = 0;
int fontTag = 0;
int bitmapTag = 0;
HDC selectedTextDC = nullptr;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s\n", message);
        std::abort();
    }
}

HFONT WINAPI ControlledCreateFontA(int height, int width, int escapement, int orientation, int weight,
    DWORD italic, DWORD underline, DWORD strikeout, DWORD charset, DWORD, DWORD, DWORD, DWORD, LPCSTR) {
    requests.push_back({height, width, escapement, orientation, weight, italic, underline, strikeout, charset});
    return reinterpret_cast<HFONT>(&fontTag);
}

HDC WINAPI ControlledCreateCompatibleDC(HDC) {
    return reinterpret_cast<HDC>(&dcTag);
}

HGDIOBJ WINAPI ControlledSelectObject(HDC dc, HGDIOBJ object) {
    selectedTextDC = dc;
    return object;
}

UINT WINAPI ControlledSetTextAlign(HDC, UINT flags) {
    Require(flags == TA_UPDATECP, "actual VGUI1 alignment call");
    return flags;
}

BOOL WINAPI ControlledGetTextMetricsA(HDC, LPTEXTMETRICA metrics) {
    *metrics = {};
    metrics->tmHeight = kMetricHeight;
    metrics->tmAscent = 3;
    metrics->tmDescent = 2;
    metrics->tmMaxCharWidth = kMaximumWidth;
    metrics->tmAveCharWidth = kAverageWidth;
    return TRUE;
}

HBITMAP WINAPI ControlledCreateDIBSection(HDC, const BITMAPINFO* information, UINT, VOID** bits, HANDLE, DWORD) {
    Require(information->bmiHeader.biWidth == kMaximumWidth
        && information->bmiHeader.biHeight == -(3 + 2 + kMetricHeight)
        && information->bmiHeader.biBitCount == kPixelBytes * 8, "actual top-down DIB geometry");
    constexpr std::size_t kBytes = kMaximumWidth * (3 + 2 + kMetricHeight) * kPixelBytes;
    std::unique_ptr<unsigned char[]> pixels(new unsigned char[kBytes]);
    for (std::size_t offset = 0; offset < kBytes; offset += kPixelBytes) {
        pixels[offset] = 17;
        pixels[offset + 1] = 91;
        pixels[offset + 2] = 203;
        pixels[offset + 3] = 0;
    }
    *bits = pixels.get();
    bitmaps.push_back(std::move(pixels));
    return reinterpret_cast<HBITMAP>(&bitmapTag);
}

BOOL WINAPI ControlledGetCharABCWidthsA(HDC, UINT first, UINT last, LPABC widths) {
    Require(first == 0 && last == kGlyphCount - 1, "actual full byte ABC query");
    if (abcFailure) {
        return FALSE;
    }
    for (int index = 0; index < kGlyphCount; ++index) {
        widths[index] = {-2, 4, 1};
    }
    return TRUE;
}

COLORREF WINAPI ControlledColor(HDC, COLORREF color) {
    return color;
}

int WINAPI ControlledSetBkMode(HDC, int mode) {
    return mode;
}

BOOL WINAPI ControlledMoveToEx(HDC, int x, int, LPPOINT) {
    lastMoveX = x;
    return TRUE;
}

BOOL WINAPI ControlledExtTextOutA(HDC, int, int, UINT, const RECT*, LPCSTR, UINT length, const INT*) {
    Require(length == 1, "raster issues one ANSI byte without pair kerning");
    ++outputCalls;
    return TRUE;
}
}

// Replace only font.cpp's GDI calls, after the real SDK has declared every type.
// No GDI font, DC, bitmap, enumeration or drawing function executes in this fixture.
#define CreateFontA ControlledCreateFontA
#define CreateCompatibleDC ControlledCreateCompatibleDC
#define SelectObject ControlledSelectObject
#define SetTextAlign ControlledSetTextAlign
#define GetTextMetricsA ControlledGetTextMetricsA
#define CreateDIBSection ControlledCreateDIBSection
#define GetCharABCWidthsA ControlledGetCharABCWidthsA
#define SetBkColor ControlledColor
#define SetTextColor ControlledColor
#define SetBkMode ControlledSetBkMode
#define MoveToEx ControlledMoveToEx
#define ExtTextOutA ControlledExtTextOutA
#include "Font.cpp"
#undef CreateFontA
#undef CreateCompatibleDC
#undef SelectObject
#undef SetTextAlign
#undef GetTextMetricsA
#undef CreateDIBSection
#undef GetCharABCWidthsA
#undef SetBkColor
#undef SetTextColor
#undef SetBkMode
#undef MoveToEx
#undef ExtTextOutA

char* vgui::vgui_strdup(const char* source) {
    const std::size_t size = std::strlen(source) + 1;
    char* result = new char[size];
    std::memcpy(result, source, size);
    return result;
}

int main() {
    vgui::Font primary("Controlled", 18, 7, -12.375f, 700, true, true, true, true);
    Require(requests.size() == 1, "single private font creation");
    const Request& request = requests.front();
    Require(request.height == 18 && request.width == 7 && request.escapement == -123
        && request.orientation == -123 && request.weight == 700 && request.italic == 1
        && request.underline == 1 && request.strikeout == 1 && request.charset == SYMBOL_CHARSET,
        "actual width, angle, styles and symbol request forwarded unchanged");
    int a = 0;
    int b = 0;
    int c = 0;
    primary.getCharABCwide('A', a, b, c);
    Require(a == 0 && b == 4 && c == 1 && primary.getTall() == kMetricHeight,
        "negative A clamp and device metric height");
    std::array<unsigned char, 8 * 8 * kPixelBytes> rgba{};
    primary.getCharRGBA('A', 1, 1, 8, 8, rgba.data());
    const unsigned char expected[]{17, 91, 203, 78};
    Require(std::memcmp(rgba.data() + (8 + 1) * kPixelBytes, expected, sizeof(expected)) == 0
        && lastMoveX == 2 && outputCalls == 1, "actual byte raster coverage and raw negative-bearing origin");
    rgba.fill(0xff);
    primary.getCharRGBA('\t', 0, 0, 8, 8, rgba.data());
    Require(rgba[0] == 0 && rgba[1] == 0 && rgba[2] == 0 && rgba[3] == 0,
        "tab's glyph region becomes transparent");
    vgui::SurfacePlat surface{};
    surface.drawDC = reinterpret_cast<HDC>(&bitmapTag);
    primary.getPlat()->drawSetTextFont(&surface);
    Require(selectedTextDC == surface.drawDC, "actual Surface font-selection lane");
    vgui::Font duplicate("CONTROLLED", 18, 7, -12.375f, 700, true, true, true, true);
    Require(requests.size() == 1 && duplicate.getPlat() == primary.getPlat() && duplicate.getId() == -1,
        "inherited cache hit shares platform and retains -1 public ID quirk");
    abcFailure = true;
    vgui::Font average("FallbackABC", -18, 0, 0.0f, 400, false, false, false, false);
    average.getCharABCwide('A', a, b, c);
    Require(a == 0 && b == kAverageWidth && c == 0 && requests.back().height == -18
        && requests.back().charset == ANSI_CHARSET, "ABC failure fallback and signed requested height");
    int width = 0;
    int height = 0;
    average.getTextSize("AA\nA", width, height);
    Require(width == 6 && height == kMetricHeight * 2, "actual byte text line measurement");
    const int beforeReset = primary.getId();
    vgui::Font_Reset();
    Require(primary.getTall() == kMetricHeight, "reset preserves an existing platform borrower");
    vgui::Font restarted("Controlled", 18, 7, -12.375f, 700, true, true, true, true);
    Require(requests.size() == 3 && restarted.getId() > beforeReset,
        "reset clears lookup without resetting font ID generation");
    std::printf("PASS controlled actual Font.cpp: ptr=%zu sizeofFont=%zu requests=%zu rasterCalls=%d\n",
        sizeof(void*), sizeof(vgui::Font), requests.size(), outputCalls);
    return 0;
}
