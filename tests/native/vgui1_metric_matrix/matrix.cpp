#include "VGui1LegacyRaster.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H
#include FT_MODULE_H
#include FT_LCD_FILTER_H
#include FT_OUTLINE_H
#include FT_BBOX_H
#include <cfenv>
#include <climits>
#include <crtdbg.h>
#include "gdi_owner.inc"

namespace {
using cse_vgui1::legacy::GlyphAbc;
#include "actual_metric_conversion.inc"

constexpr int kCellHeight = 20;
constexpr FT_UInt kCurrentInterpreter = 40;
constexpr FT_UInt kClassicInterpreter = 35;
constexpr FT_Pos kOneThirdPixel = 21;
constexpr WORD kMissingGlyph = 0xffff;
constexpr std::size_t kSfntHeaderBytes = 12;
constexpr std::size_t kSfntRecordBytes = 16;
constexpr const char* kProgram = "b3658eadae55e682b5f69eb64c439c1ecc8f196c0bb8d4756d145d13bc86476a";

struct Configuration {
    const char* name;
    FT_UInt interpreter;
    FT_Int32 flags;
};

constexpr Configuration kConfigurations[]{
    {"i40-lcd", kCurrentInterpreter, FT_LOAD_TARGET_LCD},
    {"i40-normal", kCurrentInterpreter, FT_LOAD_TARGET_NORMAL},
    {"i40-mono", kCurrentInterpreter, FT_LOAD_TARGET_MONO},
    {"i40-mono-compute", kCurrentInterpreter, FT_LOAD_TARGET_MONO | FT_LOAD_COMPUTE_METRICS},
    {"i35-lcd", kClassicInterpreter, FT_LOAD_TARGET_LCD},
    {"i35-normal", kClassicInterpreter, FT_LOAD_TARGET_NORMAL},
    {"i35-mono", kClassicInterpreter, FT_LOAD_TARGET_MONO},
    {"i35-normal-compute", kClassicInterpreter, FT_LOAD_TARGET_NORMAL | FT_LOAD_COMPUTE_METRICS},
    {"i40-scaled-unhinted", kCurrentInterpreter, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP}};

struct DesignMetric {
    FT_Pos bearing;
    FT_Pos width;
    FT_Pos advance;
};

class MemoryFace {
public:
    MemoryFace() = default;
    MemoryFace(const MemoryFace&) = delete;
    MemoryFace& operator=(const MemoryFace&) = delete;

    ~MemoryFace() {
        if (face) {
            FT_Done_Face(face);
        }
        if (library) {
            FT_Done_FreeType(library);
        }
    }

    FT_Error Open(const std::vector<unsigned char>& program, FT_UInt interpreter) {
        FT_Error error = FT_Init_FreeType(&library);
        if (!error) {
            error = FT_Property_Set(library, "truetype", "interpreter-version", &interpreter);
        }
        if (!error) {
            FT_Vector geometry[3]{{-kOneThirdPixel, 0}, {0, 0}, {kOneThirdPixel, 0}};
            error = FT_Library_SetLcdGeometry(library, geometry);
        }
        if (!error) {
            error = FT_New_Memory_Face(library, program.data(), static_cast<FT_Long>(program.size()), 0, &face);
        }
        return error;
    }

    FT_Library library = nullptr;
    FT_Face face = nullptr;
};

std::array<DesignMetric, GLYPH_COUNT> Design(const std::vector<unsigned char>& program, FILE* output) {
    MemoryFace memory;
    Require(memory.Open(program, kCurrentInterpreter) == 0, "same-program design metadata face");
    std::array<DesignMetric, GLYPH_COUNT> result{};
    std::fputs("byte\tglyph\tbearingUnits\twidthUnits\tadvanceUnits\tcboxMinX\tcboxMaxX\tunitsPerEm\n", output);
    for (unsigned int byte = 0; byte < GLYPH_COUNT; ++byte) {
        const FT_UInt glyph = FT_Get_Char_Index(memory.face,
            cse_vgui1::legacy::LegacyByteToUnicode(static_cast<unsigned char>(byte)));
        Require(FT_Load_Glyph(memory.face, glyph, FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING
            | FT_LOAD_NO_BITMAP | FT_LOAD_LINEAR_DESIGN) == 0, "unscaled design metric control");
        const auto& metrics = memory.face->glyph->metrics;
        result[byte] = {metrics.horiBearingX, metrics.width, metrics.horiAdvance};
        FT_BBox bounds{};
        FT_Outline_Get_CBox(&memory.face->glyph->outline, &bounds);
        std::fprintf(output, "%u\t%u\t%ld\t%ld\t%ld\t%ld\t%ld\t%u\n", byte, glyph,
            metrics.horiBearingX, metrics.width, metrics.horiAdvance, bounds.xMin, bounds.xMax,
            memory.face->units_per_EM);
    }
    return result;
}

void Measure(const Configuration& config, const std::vector<unsigned char>& program, int pixels,
    const std::array<DesignMetric, GLYPH_COUNT>& design, FILE* meta) {
    MemoryFace memory;
    const FT_Error open = memory.Open(program, config.interpreter);
    std::fprintf(meta, "configuration\t%s\t%u\t%ld\t%d\n", config.name, config.interpreter,
        static_cast<long>(config.flags), open);
    if (open) {
        return; // Unsupported interpreter/configuration is retained, not faked.
    }
    FT_UInt observed = 0;
    Require(FT_Property_Get(memory.library, "truetype", "interpreter-version", &observed) == 0,
        "actual supported interpreter identity");
    Require(observed == config.interpreter, "requested interpreter is not silently substituted");
    Require(FT_Set_Pixel_Sizes(memory.face, 0, static_cast<FT_UInt>(pixels)) == 0,
        "same candidate nominal pixel size");
    std::fprintf(meta, "size\t%s\t%u\t%u\t%u\t%ld\t%ld\n", config.name, observed,
        memory.face->size->metrics.x_ppem, memory.face->size->metrics.y_ppem,
        memory.face->size->metrics.x_scale, memory.face->size->metrics.y_scale);
    FILE* output = Open((std::string(config.name) + ".tsv").c_str());
    std::fputs("byte\tglyph\tbearing26\twidth26\tadvance26\tlinearAdvance16\tlsbDelta26\trsbDelta26"
        "\tcboxMinX26\tcboxMaxX26\tbboxMinX26\tbboxMaxX26\ta\tb\tc\tadvance"
        "\texactBearingNumerator\texactWidthNumerator\texactAdvanceNumerator\texactDenominator\n", output);
    for (unsigned int byte = 0; byte < GLYPH_COUNT; ++byte) {
        const FT_UInt glyph = FT_Get_Char_Index(memory.face,
            cse_vgui1::legacy::LegacyByteToUnicode(static_cast<unsigned char>(byte)));
        Require(FT_Load_Glyph(memory.face, glyph, config.flags) == 0, "actual diagnostic FT load target");
        const auto slot = memory.face->glyph;
        const auto& metrics = slot->metrics;
        FT_BBox cbox{};
        FT_BBox bbox{};
        FT_Outline_Get_CBox(&slot->outline, &cbox);
        Require(FT_Outline_Get_BBox(&slot->outline, &bbox) == 0, "actual exact outline extrema");
        GlyphAbc abc;
        int advance = 0;
        Require(GlyphMetrics(slot, abc, advance), "unchanged candidate conversion under each configuration");
        const std::int64_t scale = static_cast<std::int64_t>(pixels) * kFixedScale;
        std::fprintf(output, "%u\t%u\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld"
            "\t%d\t%d\t%d\t%d\t%lld\t%lld\t%lld\t%u\n", byte, glyph, metrics.horiBearingX,
            metrics.width, metrics.horiAdvance, slot->linearHoriAdvance, slot->lsb_delta, slot->rsb_delta,
            cbox.xMin, cbox.xMax, bbox.xMin, bbox.xMax, abc.a, abc.b, abc.c, advance,
            static_cast<long long>(design[byte].bearing * scale),
            static_cast<long long>(design[byte].width * scale),
            static_cast<long long>(design[byte].advance * scale), memory.face->units_per_EM);
    }
    Require(std::fclose(output) == 0, "metric configuration output close");
}
}

int main() {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    Require(GetACP() == 1252, "retained ACP1252 context");
    const Request request{"regular", "Arial", kCellHeight};
    RealGdiFont gdi(request);
    char selected[256]{};
    Require(GetTextFaceA(gdi.dc(), sizeof(selected), selected) > 0 && std::strcmp(selected, "Arial") == 0,
        "same GDI selected family");
    const DWORD length = GetFontData(gdi.dc(), 0, 0, nullptr, 0);
    Require(length != GDI_ERROR && length > 0 && length <= kMaximumFontBytes, "bounded transient GDI stream");
    std::vector<unsigned char> program(length);
    Require(GetFontData(gdi.dc(), 0, 0, program.data(), length) == length, "transient selected memory input");
    Require(Hash(program.data(), program.size()) == kProgram, "exact matched-program identity before any FT call");
    Require(program.size() >= kSfntHeaderBytes
        && Big16(program.data() + 4) <= (program.size() - kSfntHeaderBytes) / kSfntRecordBytes,
        "bounded SFNT metadata directory");
    FILE* meta = Open("inputs.tsv");
    std::fprintf(meta, "compiler\t%u\tptr%zu\tACP%u\trounding%d\tprogram\t%lu\t%s\n", _MSC_FULL_VER,
        sizeof(void*), GetACP(), std::fegetround(), length, kProgram);
    Provider(meta, "GetFontData", &GetFontData);
    Provider(meta, "GetGlyphOutlineW", &GetGlyphOutlineW);
    std::fprintf(meta, "order\tascending0-255\trng-none\tdevice-dpi\t%d\t%d\tcharset\t%u\n",
        GetDeviceCaps(gdi.dc(), LOGPIXELSX), GetDeviceCaps(gdi.dc(), LOGPIXELSY),
        static_cast<unsigned int>(gdi.metrics().tmCharSet));
    for (const UINT action : {SPI_GETFONTSMOOTHING, SPI_GETFONTSMOOTHINGTYPE,
        SPI_GETFONTSMOOTHINGORIENTATION, SPI_GETFONTSMOOTHINGCONTRAST}) {
        UINT value = 0;
        const BOOL ok = SystemParametersInfoW(action, 0, &value, 0);
        std::fprintf(meta, "smoothing\t%u\t%d\t%u\n", action, ok != FALSE, value);
    }
    MemoryFace identity;
    Require(identity.Open(program, kCurrentInterpreter) == 0, "current supported identity face");
    Require(identity.face->num_faces == 1 && FT_Get_Postscript_Name(identity.face)
        && std::strcmp(FT_Get_Postscript_Name(identity.face), "ArialMT") == 0, "same selected face index zero");
    const auto* os2 = static_cast<const TT_OS2*>(FT_Get_Sfnt_Table(identity.face, ft_sfnt_os2));
    Require(os2 && identity.face->units_per_EM != 0, "bounded cell-height source tables");
    const std::int64_t cellUnits = static_cast<std::int64_t>(os2->usWinAscent) + os2->usWinDescent;
    Require(cellUnits > 0, "positive candidate cell-height denominator");
    const int pixels = static_cast<int>(static_cast<std::int64_t>(kCellHeight) * identity.face->units_per_EM / cellUnits);
    std::fprintf(meta, "candidate-size-rule\t%d\t%u\t%lld\t%d\n", kCellHeight, identity.face->units_per_EM,
        static_cast<long long>(cellUnits), pixels);
    FILE* gdiMetrics = Open("gdi.tsv");
    std::fputs("byte\tunit\tansiGlyph\twideGlyph\ta\tb\tc\tgoResult\tblackX\tblackY"
        "\toriginX\toriginY\tadvanceX\tadvanceY\n", gdiMetrics);
    MAT2 transform{};
    transform.eM11.value = 1;
    transform.eM22.value = 1;
    for (unsigned int byte = 0; byte < GLYPH_COUNT; ++byte) {
        const char input = static_cast<char>(byte);
        wchar_t unit = 0;
        Require(MultiByteToWideChar(1252, 0, &input, 1, &unit, 1) == 1, "actual ANSI conversion");
        Require(unit == cse_vgui1::legacy::LegacyByteToUnicode(static_cast<unsigned char>(byte)),
            "same explicit byte mapping in every configuration");
        WORD ansi = 0;
        WORD wide = 0;
        Require(GetGlyphIndicesA(gdi.dc(), &input, 1, &ansi, GGI_MARK_NONEXISTING_GLYPHS) != GDI_ERROR
            && GetGlyphIndicesW(gdi.dc(), &unit, 1, &wide, GGI_MARK_NONEXISTING_GLYPHS) != GDI_ERROR,
            "reference glyph identities");
        GLYPHMETRICS gm{};
        DWORD observed = GDI_ERROR;
        if (ansi != kMissingGlyph) {
            observed = GetGlyphOutlineW(gdi.dc(), ansi, GGO_METRICS | GGO_GLYPH_INDEX,
                &gm, 0, nullptr, &transform);
        }
        const ABC& abc = gdi.abc(static_cast<int>(byte));
        std::fprintf(gdiMetrics, "%u\t%u\t%u\t%u\t%d\t%u\t%d\t%lu\t%u\t%u\t%ld\t%ld\t%d\t%d\n",
            byte, static_cast<unsigned int>(unit), ansi, wide, abc.abcA, abc.abcB, abc.abcC, observed,
            gm.gmBlackBoxX, gm.gmBlackBoxY, gm.gmptGlyphOrigin.x, gm.gmptGlyphOrigin.y, gm.gmCellIncX, gm.gmCellIncY);
    }
    Require(std::fclose(gdiMetrics) == 0, "GDI metric facts close");
    FILE* designOutput = Open("design.tsv");
    const auto design = Design(program, designOutput);
    Require(std::fclose(designOutput) == 0, "unscaled design facts close");
    for (const auto& config : kConfigurations) {
        Measure(config, program, pixels, design, meta);
    }
    Require(Hash(program.data(), program.size()) == kProgram, "same transient program after matrix");
    Require(std::fclose(meta) == 0, "matrix input close");
    std::puts("PASS diagnostic measurement completion; no policy/backend selected and no font/table bytes saved");
}
