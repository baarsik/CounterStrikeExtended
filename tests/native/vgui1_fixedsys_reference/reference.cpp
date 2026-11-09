#include <Windows.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {
unsigned int abcCalls = 0;
BOOL abcReturned = FALSE;
DWORD abcError = 0;
BOOL WINAPI TrackedGetCharABCWidthsA(HDC dc, UINT first, UINT last, LPABC abc) {
    SetLastError(0);
    const BOOL result = ::GetCharABCWidthsA(dc, first, last, abc);
    abcError = GetLastError();
    abcReturned = result;
    ++abcCalls;
    return result;
}
}
// Forward unchanged constructor calls to the actual API and retain its result.
#define GetCharABCWidthsA TrackedGetCharABCWidthsA
#include "gdi_owner.inc"
#undef GetCharABCWidthsA

int main() {
    static_cast<void>(&Big16); // Reused checked owner includes this unrelated metadata helper.
    FILE* meta = Open("metrics.tsv");
    FILE* cells = Open("cells.tsv");
    FILE* providers = Open("providers.tsv");
    Provider(providers, "CreateFontA", &CreateFontA);
    Provider(providers, "GetTextMetricsA", &GetTextMetricsA);
    Provider(providers, "GetCharABCWidthsA", &GetCharABCWidthsA);
    Provider(providers, "ExtTextOutA", &ExtTextOutA);
    Provider(providers, "GetGlyphIndicesA", &GetGlyphIndicesA);
    Provider(providers, "GetGlyphIndicesW", &GetGlyphIndicesW);
    BOOL smoothing = FALSE;
    UINT smoothingType = 0;
    UINT orientation = 0;
    UINT contrast = 0;
    const BOOL smoothingOk = SystemParametersInfoW(SPI_GETFONTSMOOTHING, 0, &smoothing, 0);
    const BOOL typeOk = SystemParametersInfoW(SPI_GETFONTSMOOTHINGTYPE, 0, &smoothingType, 0);
    const BOOL orientationOk = SystemParametersInfoW(SPI_GETFONTSMOOTHINGORIENTATION, 0, &orientation, 0);
    const BOOL contrastOk = SystemParametersInfoW(SPI_GETFONTSMOOTHINGCONTRAST, 0, &contrast, 0);
    std::fprintf(meta, "environment\tcompiler=%d\tACP=%u\tsmoothing=%d/%d\ttype=%u/%d\torientation=%u/%d"
        "\tcontrast=%u/%d\trng=none\n", _MSC_FULL_VER, GetACP(), smoothing, smoothingOk,
        smoothingType, typeOk, orientation, orientationOk, contrast, contrastOk);
    const DWORD objectsBefore = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    {
        const Request request{"fixedsys", "FixedSys", 18, 0, 0.0f, 400, false, false, false, false};
        RealGdiFont font(request);
        Require(abcCalls == 1, "exact retained constructor ABC query");
        char selected[256]{};
        Require(GetTextFaceA(font.dc(), static_cast<int>(sizeof(selected)), selected) > 0, "selected GDI face");
        const auto& m = font.metrics();
        std::fprintf(meta, "request\tFixedSys\t18\t0\t0\t400\tANSI\tDEFAULT_QUALITY\tno-styles\n");
        std::fprintf(meta, "selected\t%s\theight=%ld\tascent=%ld\tdescent=%ld\tinternal=%ld\texternal=%ld"
            "\taverage=%ld\tmaximum=%ld\tweight=%ld\titalic=%u\tpitch-family=%u\tcharset=%u"
            "\tfirst=%u\tlast=%u\tdefault=%u\tbreak=%u\tbuffer=%d/%d\tdpi=%d/%d\n", selected,
            m.tmHeight, m.tmAscent, m.tmDescent, m.tmInternalLeading, m.tmExternalLeading,
            m.tmAveCharWidth, m.tmMaxCharWidth, m.tmWeight, m.tmItalic, m.tmPitchAndFamily, m.tmCharSet,
            static_cast<unsigned int>(static_cast<unsigned char>(m.tmFirstChar)),
            static_cast<unsigned int>(static_cast<unsigned char>(m.tmLastChar)),
            static_cast<unsigned int>(static_cast<unsigned char>(m.tmDefaultChar)),
            static_cast<unsigned int>(static_cast<unsigned char>(m.tmBreakChar)), font.bufferWide(), font.bufferTall(),
            GetDeviceCaps(font.dc(), LOGPIXELSX), GetDeviceCaps(font.dc(), LOGPIXELSY));
        std::fprintf(meta, "constructor-ABC\tcalls=%u\treturned=%d\terror=%lu\tfallback=%d\n",
            abcCalls, abcReturned, abcError, !abcReturned);
        SetLastError(0);
        const DWORD rangesBytes = GetFontUnicodeRanges(font.dc(), nullptr);
        const DWORD rangesError = GetLastError();
        std::fprintf(meta, "unicode-ranges\tbytes=%lu\terror=%lu\n", rangesBytes, rangesError);
        constexpr int kWidth = 24;
        constexpr int kHeight = 24;
        constexpr std::size_t kBytes = kWidth * kHeight * VGUI_RGBA_BYTES;
        std::array<unsigned char, kBytes> rgba{};
        for (unsigned int byte = 0; byte < GLYPH_COUNT; ++byte) {
            const char input = static_cast<char>(byte);
            wchar_t wide = 0;
            Require(MultiByteToWideChar(1252, 0, &input, 1, &wide, 1) == 1, "exact CP1252 unit");
            WORD ansiGlyph = 0xcccc;
            WORD wideGlyph = 0xcccc;
            SetLastError(0);
            const DWORD ansiResult = GetGlyphIndicesA(font.dc(), &input, 1, &ansiGlyph, GGI_MARK_NONEXISTING_GLYPHS);
            const DWORD ansiError = GetLastError();
            SetLastError(0);
            const DWORD wideResult = GetGlyphIndicesW(font.dc(), &wide, 1, &wideGlyph, GGI_MARK_NONEXISTING_GLYPHS);
            const DWORD wideError = GetLastError();
            SIZE extent{};
            const BOOL extentOk = GetTextExtentPoint32A(font.dc(), &input, 1, &extent);
            int a = 0;
            int b = 0;
            int c = 0;
            font.getCharABCwide(static_cast<int>(byte), a, b, c);
            const auto raw = font.abc(static_cast<int>(byte));
            rgba.fill(0x5a);
            font.getCharRGBA(static_cast<int>(byte), kPenX, kPenY, kWidth, kHeight, rgba.data());
            Require(GdiFlush() != FALSE, "owned memory-DIB flush");
            unsigned int alphaPixels = 0;
            unsigned int alphaSum = 0;
            for (int y = 0; y < m.tmHeight; ++y) {
                for (int x = 0; x < font.bufferWide(); ++x) {
                    const std::size_t position = (static_cast<std::size_t>(y + kPenY) * kWidth
                        + static_cast<std::size_t>(x + kPenX)) * VGUI_RGBA_BYTES;
                    alphaPixels += rgba[position + 3] != 0;
                    alphaSum += rgba[position + 3];
                }
            }
            std::fprintf(cells, "%u\t%u\t%d\t%u\t%d\t%d\t%d\t%d\t%lu\t%u\t%lu\t%lu\t%u"
                "\t%lu\t%d\t%ld\t%ld\t%u\t%u\t%s\n", byte, static_cast<unsigned int>(wide),
                raw.abcA, raw.abcB, raw.abcC, a, b, c, ansiResult, ansiGlyph, ansiError,
                wideResult, wideGlyph, wideError, extentOk, extent.cx, extent.cy, alphaPixels, alphaSum,
                Hash(rgba.data(), rgba.size()).c_str());
        }
    }
    const DWORD objectsAfter = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    std::fprintf(meta, "objects\tbefore=%lu\tafter=%lu\n", objectsBefore, objectsAfter);
    Require(objectsBefore == objectsAfter, "fixture-owned GDI resources return to baseline");
    Require(std::fclose(meta) == 0 && std::fclose(cells) == 0 && std::fclose(providers) == 0,
        "close owned measurement receipts");
    std::puts("PASS one exact FixedSys18/400 memory-GDI tuple; no registration/window/font program export");
}
