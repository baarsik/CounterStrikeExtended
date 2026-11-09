#include "VGui1LegacyRaster.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H
#include <crtdbg.h>
#include <cfenv>
#include "gdi_owner.inc"

namespace {
constexpr int kCellHeight = 20;
constexpr std::uint32_t kExpectedCodePage = 1252;
constexpr const char* kExpectedProgram = "b3658eadae55e682b5f69eb64c439c1ecc8f196c0bb8d4756d145d13bc86476a";
constexpr std::size_t kSfntHeaderBytes = 12;
constexpr std::size_t kSfntRecordBytes = 16;

std::uint32_t Big32(const unsigned char* data) {
    return (static_cast<std::uint32_t>(data[0]) << 24) | (static_cast<std::uint32_t>(data[1]) << 16)
        | (static_cast<std::uint32_t>(data[2]) << 8) | data[3];
}

void TableFacts(FILE* output, const std::vector<unsigned char>& program) {
    Require(program.size() >= kSfntHeaderBytes, "bounded selected SFNT header");
    const std::size_t count = Big16(program.data() + 4);
    Require(count <= (program.size() - kSfntHeaderBytes) / kSfntRecordBytes, "bounded selected table directory");
    for (std::size_t index = 0; index < count; ++index) {
        const auto* record = program.data() + kSfntHeaderBytes + index * kSfntRecordBytes;
        const std::size_t offset = Big32(record + 8);
        const std::size_t length = Big32(record + 12);
        Require(offset <= program.size() && length <= program.size() - offset, "selected table span");
        std::fprintf(output, "table\t%s\t%zu\t%s\n", Hex(record, 4).c_str(), length,
            Hash(program.data() + offset, length).c_str());
    }
}

void ObserveSmoothing(FILE* output, UINT action, const char* name) {
    UINT value = 0;
    SetLastError(0);
    const BOOL ok = SystemParametersInfoW(action, 0, &value, 0);
    std::fprintf(output, "smoothing\t%s\t%d\t%u\t%lu\n", name, ok != FALSE, value,
        ok ? 0UL : GetLastError());
}

bool Order(const char* name, const std::vector<int>& order, FILE* inputs) {
    using namespace cse_vgui1::legacy;
    const Request request{"regular", "Arial", kCellHeight};
    RealGdiFont gdi(request);
    char selected[256]{};
    Require(GetTextFaceA(gdi.dc(), sizeof(selected), selected) > 0 && std::strcmp(selected, "Arial") == 0,
        "regular Arial selected by the actual constructor");
    const DWORD size = GetFontData(gdi.dc(), 0, 0, nullptr, 0);
    Require(size != GDI_ERROR && size > 0 && size <= kMaximumFontBytes, "bounded transient GetFontData stream");
    std::vector<unsigned char> program(size);
    Require(GetFontData(gdi.dc(), 0, 0, program.data(), size) == size, "GDI stream directly in transient memory");
    const std::string hashBeforeFt = Hash(program.data(), program.size());
    std::fprintf(inputs, "program-before-FT\t%s\t%lu\t%s\n", name, size, hashBeforeFt.c_str());
    Require(hashBeforeFt == kExpectedProgram, "same frozen selected GDI program identity before FT");

    FILE* meta = Open((std::string(name) + "-metrics.tsv").c_str());
    FILE* widths = Open((std::string(name) + "-abc.tsv").c_str());
    FILE* identities = Open((std::string(name) + "-glyphs.tsv").c_str());
    FILE* pixels = Open((std::string(name) + "-pixels.tsv").c_str());
    FILE* gdiPixels = Open((std::string(name) + "-gdi.rgba").c_str());
    FILE* ftPixels = Open((std::string(name) + "-candidate.rgba").c_str());
    TableFacts(meta, program);
    FT_Library library = nullptr;
    FT_Face face = nullptr;
    Require(FT_Init_FreeType(&library) == 0
        && FT_New_Memory_Face(library, program.data(), static_cast<FT_Long>(program.size()), 0, &face) == 0,
        "metadata opens the identical transient selected program at face zero");
    int major = 0;
    int minor = 0;
    int patch = 0;
    FT_Library_Version(library, &major, &minor, &patch);
    Require(major == FREETYPE_MAJOR && minor == FREETYPE_MINOR && patch == FREETYPE_PATCH,
        "actual linked FreeType version matches pinned headers");
    std::fprintf(meta, "FT-version\t%d\t%d\t%d\n", major, minor, patch);
    const char* postscript = FT_Get_Postscript_Name(face);
    Require(face->num_faces == 1 && postscript && std::strcmp(postscript, "ArialMT") == 0,
        "unambiguous selected memory face and PostScript identity");
    const auto* os2 = static_cast<const TT_OS2*>(FT_Get_Sfnt_Table(face, ft_sfnt_os2));
    Require(os2 && face->charmap, "selected program OS2 and default Unicode charmap");
    std::fprintf(meta, "FT-program\t%s\t%ld\t%ld\t%u\t%u\t%u\t%u\t%d\t%u\t%u\t%ld\t%ld\n",
        postscript, face->num_faces, face->num_glyphs, face->units_per_EM, os2->usWinAscent,
        os2->usWinDescent, os2->usWeightClass, os2->xAvgCharWidth, face->charmap->platform_id,
        face->charmap->encoding_id, face->bbox.xMin, face->bbox.xMax);
    const auto& tm = gdi.metrics();
    std::fprintf(meta, "GDI-metrics\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%u\n", tm.tmHeight,
        tm.tmAscent, tm.tmDescent, tm.tmInternalLeading, tm.tmExternalLeading, tm.tmAveCharWidth,
        tm.tmMaxCharWidth, static_cast<unsigned int>(tm.tmCharSet));
    std::fprintf(meta, "device\t%d\t%d\t%d\t%d\t%d\n", GetDeviceCaps(gdi.dc(), LOGPIXELSX),
        GetDeviceCaps(gdi.dc(), LOGPIXELSY), GetDeviceCaps(gdi.dc(), TECHNOLOGY),
        GetDeviceCaps(gdi.dc(), BITSPIXEL), GetDeviceCaps(gdi.dc(), PLANES));
    Result result = Result::InvalidArgument;
    int nativeError = 0;
    auto candidate = HarmonyCandidate::CreateOwned(program.data(), program.size(), kCellHeight, result, nativeError);
    std::fprintf(meta, "candidate-result\t%d\t%d\n", static_cast<int>(result), nativeError);
    Require(Hash(program.data(), program.size()) == hashBeforeFt, "FT retained the identical caller bytes");
    if (!candidate) {
        for (FILE* file : {meta, widths, identities, pixels, gdiPixels, ftPixels}) {
            Require(std::fclose(file) == 0, "preserve candidate error receipts");
        }
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        std::fprintf(stderr, "Candidate unavailable result=%d native=%d\n", static_cast<int>(result), nativeError);
        return false;
    }
    const auto& metrics = candidate->GetMetrics();
    std::fprintf(meta, "candidate-metrics\t%d\t%d\t%d\t%d\t%d\t%d\n", metrics.height, metrics.ascent,
        metrics.descent, metrics.internalLeading, metrics.averageWidth, metrics.maximumWidth);
    const int width = (std::max)(tm.tmMaxCharWidth, static_cast<LONG>(metrics.maximumWidth))
        + kPenX + kGuardCells;
    const int height = (std::max)(tm.tmHeight, static_cast<LONG>(metrics.height)) + kPenY + kGuardCells;
    Require(width > 0 && width < kCanvasLimit && height > 0 && height < kCanvasLimit, "bounded shared canvas");
    const std::size_t canvasBytes = static_cast<std::size_t>(width) * height * VGUI_RGBA_BYTES;
    std::vector<unsigned char> gdiOutput(canvasBytes);
    std::vector<unsigned char> ftOutput(canvasBytes);
    const RgbaCanvas canvas{ftOutput.data(), ftOutput.size(), width, height};
    for (std::size_t index = 0; index < order.size(); ++index) {
        const int byte = order[index];
        const char input = static_cast<char>(byte);
        wchar_t unit = 0;
        Require(MultiByteToWideChar(kExpectedCodePage, 0, &input, 1, &unit, 1) == 1,
            "actual retained CP1252 byte decoding");
        Require(unit == LegacyByteToUnicode(static_cast<unsigned char>(byte)), "unchanged explicit byte mapping");
        WORD ansiGlyph = 0;
        WORD wideGlyph = 0;
        Require(GetGlyphIndicesA(gdi.dc(), &input, 1, &ansiGlyph, GGI_MARK_NONEXISTING_GLYPHS) != GDI_ERROR
            && GetGlyphIndicesW(gdi.dc(), &unit, 1, &wideGlyph, GGI_MARK_NONEXISTING_GLYPHS) != GDI_ERROR,
            "actual ANSI and Unicode glyph identity controls");
        const unsigned int ftGlyph = candidate->GetGlyphIndex(static_cast<unsigned int>(byte));
        Require(ftGlyph == FT_Get_Char_Index(face, unit), "candidate glyph index matches the identical memory face");
        std::fprintf(identities, "%zu\t%d\t%u\t%u\t%u\t%u\n", index, byte,
            static_cast<unsigned int>(unit), ansiGlyph, wideGlyph, ftGlyph);
        int a = 0;
        int b = 0;
        int c = 0;
        gdi.getCharABCwide(byte, a, b, c);
        const ABC& raw = gdi.abc(byte);
        GlyphAbc abc;
        Require(candidate->GetAbc(static_cast<unsigned int>(byte), abc) == Result::Ok, "candidate raw ABC");
        std::fprintf(widths, "%zu\t%d\t%d\t%u\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
            index, byte, raw.abcA, raw.abcB, raw.abcC, a, b, c, abc.a, abc.b, abc.c);
        std::fill(gdiOutput.begin(), gdiOutput.end(), static_cast<unsigned char>(0x5a));
        std::fill(ftOutput.begin(), ftOutput.end(), static_cast<unsigned char>(0x5a));
        gdi.getCharRGBA(byte, kPenX, kPenY, width, height, gdiOutput.data());
        Require(GdiFlush() != FALSE, "synchronize actual production DIB operation");
        Require(candidate->Raster(static_cast<unsigned int>(byte), kPenX, kPenY, canvas) == Result::Ok,
            "unchanged current candidate raster");
        std::size_t different = 0;
        for (std::size_t offset = 0; offset < gdiOutput.size(); ++offset) {
            different += gdiOutput[offset] != ftOutput[offset];
        }
        std::fprintf(pixels, "%zu\t%d\t%d\t%d\t%zu\t%s\t%s\n", index, byte, width, height, different,
            Hash(gdiOutput.data(), gdiOutput.size()).c_str(), Hash(ftOutput.data(), ftOutput.size()).c_str());
        Require(std::fwrite(gdiOutput.data(), 1, gdiOutput.size(), gdiPixels) == gdiOutput.size()
            && std::fwrite(ftOutput.data(), 1, ftOutput.size(), ftPixels) == ftOutput.size(),
            "save only allowed raster measurements, never SFNT bytes");
    }
    Require(FT_Done_Face(face) == 0 && FT_Done_FreeType(library) == 0, "metadata face/library cleanup");
    for (FILE* file : {meta, widths, identities, pixels, gdiPixels, ftPixels}) {
        Require(std::fclose(file) == 0, "measurement output close");
    }
    // The original program vector and candidate's private copy die in this
    // process. Neither stream nor any table is ever written to a file.
    return true;
}
}

int main() {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    Require(GetACP() == kExpectedCodePage, "same retained reference ACP1252");
    FILE* inputs = Open("inputs.tsv");
    std::fprintf(inputs, "compiler\t%u\tptr%zu\twchar%zu\tACP%u\trounding%d\trng-none\n",
        _MSC_FULL_VER, sizeof(void*), sizeof(wchar_t), GetACP(), std::fegetround());
    Provider(inputs, "CreateFontA", &CreateFontA);
    Provider(inputs, "GetFontData", &GetFontData);
    Provider(inputs, "ExtTextOutA", &ExtTextOutA);
    std::fprintf(inputs, "gdi-batch-limit\t%lu\n", GdiGetBatchLimit());
    ObserveSmoothing(inputs, SPI_GETFONTSMOOTHING, "enabled");
    ObserveSmoothing(inputs, SPI_GETFONTSMOOTHINGTYPE, "type");
    ObserveSmoothing(inputs, SPI_GETFONTSMOOTHINGORIENTATION, "orientation");
    ObserveSmoothing(inputs, SPI_GETFONTSMOOTHINGCONTRAST, "contrast");
    std::vector<int> ascending;
    std::vector<int> descending;
    for (int byte = 0; byte < GLYPH_COUNT; ++byte) {
        ascending.push_back(byte);
        descending.push_back(GLYPH_COUNT - 1 - byte);
    }
    bool ok = Order("ascending", ascending, inputs);
    if (ok) {
        ok = Order("descending", descending, inputs);
    }
    if (ok) {
        ok = Order("sequence", {'A', ' ', 'H', ' ', 'M', 'H', 'A', '\t', 'W', 'A', 'A', ' '}, inputs);
    }
    Require(std::fclose(inputs) == 0, "input measurement close");
    if (ok) {
        std::puts("PASS measurement completion: matched transient GDI program/current candidate, no font bytes saved");
    }
    return ok ? 0 : 3;
}
