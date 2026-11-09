#include "gdi_owner.inc"
#include "candidates.h"

namespace {
constexpr int kOutputWidth = 24;
constexpr int kOutputHeight = 24;
using Canvas = std::array<unsigned char, kOutputWidth * kOutputHeight * VGUI_RGBA_BYTES>;

void UnicodeRaster(RealGdiFont& font, wchar_t unit, Canvas& canvas) {
    uchar* rgba = canvas.data();
    const int rgbaWide = kOutputWidth;
    Require(GdiFlush() != FALSE, "flush before owned DIB reset");
    const auto bytes = static_cast<std::size_t>(font.bufferWide()) * font.bufferTall() * VGUI_RGBA_BYTES;
    std::memset(const_cast<unsigned char*>(font.pixels()), 0, bytes);
    SetBkColor(font.dc(), RGB(0, 0, 0));
    SetTextColor(font.dc(), RGB(255, 255, 255));
    SetBkMode(font.dc(), OPAQUE);
    MoveToEx(font.dc(), 0, 0, nullptr);
    Require(ExtTextOutW(font.dc(), 0, 0, 0, nullptr, &unit, 1, nullptr) != FALSE, "one Unicode candidate draw");
    SetBkMode(font.dc(), TRANSPARENT);
    Require(GdiFlush() != FALSE, "candidate DIB synchronization");
    const float byteToUnit = 1.0f / 255.0f;
    const int ch = -1; // Candidate glyph ink is not masked by the original tab-byte policy.
    for (int y = 0; y < font.metrics().tmHeight; ++y) {
        for (int x = 0; x < font.bufferWide(); ++x) {
            const uchar* pixel = font.pixels() + VGUI_RGBA_BYTES * (x + y * font.bufferWide());
            const int outX = kPenX + x;
            const int outY = kPenY + y;
#include "actual_pixel.inc"
        }
    }
}
unsigned int AlphaPixels(const Canvas& rgba, const RealGdiFont& font) {
    unsigned int count = 0;
    for (int y = 0; y < font.metrics().tmHeight; ++y) {
        for (int x = 0; x < font.bufferWide(); ++x) {
            const auto offset = VGUI_RGBA_BYTES * ((kPenX + x) + (kPenY + y) * kOutputWidth);
            count += rgba[static_cast<std::size_t>(offset) + 3] != 0;
        }
    }
    return count;
}
}
int main() {
    static_cast<void>(&Big16);
    FILE* output = Open("aliases.tsv");
    FILE* provider = Open("providers.tsv");
    Provider(provider, "CreateFontA", &CreateFontA);
    Provider(provider, "ExtTextOutA", &ExtTextOutA);
    Provider(provider, "ExtTextOutW", &ExtTextOutW);
    Provider(provider, "GetGlyphIndicesW", &GetGlyphIndicesW);
    const Request request{"fixedsys", "FixedSys", 18, 0, 0.0f, 400, false, false, false, false};
    const DWORD before = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    {
        RealGdiFont original(request);
        std::array<std::string, GLYPH_COUNT> hashes;
        std::array<unsigned int, GLYPH_COUNT> alpha{};
        Canvas rgba{};
        for (unsigned int byte = 0; byte < GLYPH_COUNT; ++byte) {
            rgba.fill(0x5a);
            original.getCharRGBA(static_cast<int>(byte), kPenX, kPenY, kOutputWidth, kOutputHeight, rgba.data());
            hashes[byte] = Hash(rgba.data(), rgba.size());
            alpha[byte] = AlphaPixels(rgba, original);
        }
        RealGdiFont unicode(request);
        for (const auto& candidate : kCandidates) {
            const auto unit = static_cast<wchar_t>(candidate.unicode);
            WORD glyph = 0xcccc;
            SetLastError(0);
            const DWORD query = GetGlyphIndicesW(unicode.dc(), &unit, 1, &glyph, GGI_MARK_NONEXISTING_GLYPHS);
            const DWORD error = GetLastError();
            rgba.fill(0x5a);
            UnicodeRaster(unicode, unit, rgba);
            const auto digest = Hash(rgba.data(), rgba.size());
            std::fprintf(output, "%u\t%u\t%s\t%lu\t%u\t%lu\t%u\t%u\t%s\t%s\t%d\n", candidate.byte,
                candidate.unicode, candidate.label, query, glyph, error, alpha[candidate.byte],
                AlphaPixels(rgba, unicode), hashes[candidate.byte].c_str(), digest.c_str(),
                hashes[candidate.byte] == digest);
        }
    }
    Require(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) == before, "owned GDI objects return to baseline");
    Require(std::fclose(output) == 0 && std::fclose(provider) == 0, "close metadata/digest receipts");
    std::puts("PASS finite Unicode candidate comparison only; aliases not inferred from names/default glyphs");
}
