#include "VGui1NativeFontFace.h"

#include "../../common/VGui1TextEncoding.h"
#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_LCD_FILTER_H
#include FT_MODULE_H
#include FT_OUTLINE_H
#include FT_SYNTHESIS_H
#include FT_TRUETYPE_TABLES_H

#if FREETYPE_MAJOR != 2 || FREETYPE_MINOR != 14 || FREETYPE_PATCH != 3
#error Requalify the native face before changing the pinned FreeType version
#endif
#if defined(__FAST_MATH__) || defined(_M_FP_FAST)
#error Native legacy transfer needs the explicit strict-FP source policy
#endif

namespace cse_vgui1 {
namespace legacy {
namespace {
constexpr std::size_t kByteCount = 256;
constexpr std::size_t kPixelBytes = 4;
constexpr std::int64_t kFixedScale = 64;
// GDI semibold and heavier requests can use an emboldened regular donor.
constexpr int kSemiboldWeight = 600;
constexpr FT_Fixed kMatrixScale = 65536;
constexpr int kTenthsPerTurn = 3600;
constexpr double kRadiansPerTenth = 3.14159265358979323846 / 1800.0;
constexpr unsigned int kWindowsPlatform = 3;
constexpr unsigned int kWindowsSymbolEncoding = 0;
constexpr FT_ULong kSymbolHighByte = 0xf000;
constexpr long kMaximumFaceIndex = 0x7fffffffL;
constexpr std::size_t kMaximumOffset = static_cast<std::size_t>((std::numeric_limits<std::ptrdiff_t>::max)());

bool ToInt(std::int64_t value, int& output) {
    if (value < INT_MIN || value > INT_MAX) {
        return false;
    }
    output = static_cast<int>(value);
    return true;
}

bool Product(std::size_t left, std::size_t right, std::size_t& output) {
    if (right != 0 && left > kMaximumOffset / right) {
        return false;
    }
    output = left * right;
    return true;
}

bool RoundPositive(std::int64_t value, std::int64_t scale, std::int64_t denominator, int& output) {
    if (value < 0 || scale <= 0 || denominator <= 0
        || value > ((std::numeric_limits<std::int64_t>::max)() - denominator / 2) / scale) {
        return false;
    }
    return ToInt((value * scale + denominator / 2) / denominator, output);
}

std::int64_t Floor26(std::int64_t value) {
    return value / kFixedScale - static_cast<int>(value % kFixedScale < 0);
}

std::int64_t Ceil26(std::int64_t value) {
    return value / kFixedScale + static_cast<int>(value % kFixedScale > 0);
}

// Kept identical to the frozen ordinary candidate's measured conversion. The
// slot realization differs from GDI; this does not advertise metric parity.
bool GlyphMetrics(FT_GlyphSlot slot, GlyphAbc& output, int& advance) {
    const auto& metrics = slot->metrics;
    if (metrics.horiBearingX < INT_MIN || metrics.horiBearingX > INT_MAX
        || metrics.width < 0 || metrics.width > INT_MAX || metrics.horiAdvance < 0
        || metrics.horiAdvance > INT_MAX) {
        return false;
    }
    const std::int64_t left = Floor26(metrics.horiBearingX);
    const std::int64_t right = Ceil26(static_cast<std::int64_t>(metrics.horiBearingX) + metrics.width);
    const std::int64_t black = (std::max)(std::int64_t{1}, right - left);
    return ToInt((static_cast<std::int64_t>(metrics.horiAdvance) + kFixedScale / 2) / kFixedScale, advance)
        && ToInt(left, output.a) && ToInt(black, output.b)
        && ToInt(static_cast<std::int64_t>(advance) - left - black, output.c);
}

bool SupportedRequest(const FontRequest& request) {
    constexpr std::uint32_t kSupportedFlags = FontSymbol | FontItalic | FontUnderline | FontStrikeout;
    return request.tall > 0 && std::isfinite(request.rotation)
        && (request.flags & ~kSupportedFlags) == 0 && request.ansiCodePage == LegacyAnsiCodePage;
}

bool SymbolMap(FT_Face face, std::array<FT_UInt, kByteCount>& output) {
    bool found = false;
    bool any = false;
    for (int index = 0; index < face->num_charmaps; ++index) {
        FT_CharMap map = face->charmaps[index];
        if (map->platform_id != kWindowsPlatform || map->encoding_id != kWindowsSymbolEncoding
            || map->encoding != FT_ENCODING_MS_SYMBOL || FT_Set_Charmap(face, map) != 0) {
            continue;
        }
        std::array<FT_UInt, kByteCount> actual{};
        FT_UInt glyph = 0;
        FT_ULong code = FT_Get_First_Char(face, &glyph);
        while (glyph != 0) {
            // Validate actual nonzero records in both byte domains instead
            // of assuming that every symbol font uses a fixed F000 alias.
            if (code < kByteCount || (code >= kSymbolHighByte && code - kSymbolHighByte < kByteCount)) {
                const auto byte = static_cast<std::size_t>(code < kByteCount ? code : code - kSymbolHighByte);
                if (actual[byte] != 0 && actual[byte] != glyph) {
                    return false;
                }
                actual[byte] = glyph;
                any = true;
            }
            const FT_ULong previous = code;
            code = FT_Get_Next_Char(face, code, &glyph);
            if (glyph != 0 && code <= previous) {
                return false;
            }
        }
        if (found && output != actual) {
            return false;
        }
        output = actual;
        found = true;
    }
    return found && any;
}
}

struct NativeFontFace::Impl {
    std::vector<unsigned char> bytes;
    FT_Library library = nullptr;
    FT_Face face = nullptr;
    long faceIndex = 0;
    std::string postScriptName;
    unsigned int intrinsicWeight = 0;
    bool symbol = false;
    CandidateMetrics metrics;
    std::array<GlyphAbc, kByteCount> abc{};
    std::array<int, kByteCount> advances{};
    std::array<FT_UInt, kByteCount> glyphs{};
    std::vector<unsigned char> canvas;
    int canvasHeight = 0;
    bool syntheticBold = false;
    bool syntheticItalic = false;
    bool underline = false;
    bool strikeout = false;
    bool rotated = false;
    int underlinePosition = 0;
    int strikeoutPosition = 0;
    int decorationThickness = 1;
    double cosine = 1.0;
    double sine = 0.0;
    FT_Matrix rotation{kMatrixScale, 0, 0, kMatrixScale};
    std::uint32_t capabilities = CapabilityAbc | CapabilityLegacyRgba | CapabilityPenOffsets | CapabilityWeight;

    int LoadGlyph(FT_UInt glyph) {
        const FT_Int32 flags = FT_LOAD_TARGET_LCD
            | ((syntheticItalic || rotated) ? FT_LOAD_NO_BITMAP : 0);
        const int error = FT_Load_Glyph(face, glyph, flags);
        if (error != 0) {
            return error;
        }
        if (syntheticBold) {
            FT_GlyphSlot_Embolden(face->glyph);
        }
        if (syntheticItalic) {
            FT_GlyphSlot_Oblique(face->glyph);
            // FreeType's synthetic oblique changes the outline, not slot metrics.
            // ABC values describe the unrotated ink even when escapement is set.
            FT_BBox bounds{};
            FT_Outline_Get_CBox(&face->glyph->outline, &bounds);
            face->glyph->metrics.horiBearingX = bounds.xMin;
            face->glyph->metrics.width = bounds.xMax - bounds.xMin;
        }
        return 0;
    }

    ~Impl() {
        if (face) {
            FT_Done_Face(face);
        }
        if (library) {
            FT_Done_FreeType(library);
        }
    }
};

NativeFontFace::NativeFontFace(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
NativeFontFace::~NativeFontFace() = default;

std::unique_ptr<NativeFontFace> NativeFontFace::CreateOwned(const void* bytes, std::size_t length,
    long faceIndex, const char* expectedPostScriptName, const FontRequest& request,
    Result& result, int& nativeError, std::string& diagnostic) noexcept {
    result = Result::InvalidArgument;
    nativeError = 0;
    diagnostic = "invalid owned font input/index/identity";
    if (!bytes || length == 0 || faceIndex < 0 || faceIndex > kMaximumFaceIndex
        || !expectedPostScriptName || !expectedPostScriptName[0]) {
        return {};
    }
    result = Result::UnsupportedRequest;
    diagnostic = "native face does not support this geometry/style/byte encoding";
    if (!SupportedRequest(request)) {
        return {};
    }
    result = Result::Unrepresentable;
    diagnostic = "font byte span exceeds the native FT/addressable range";
    if (length > static_cast<std::size_t>((std::numeric_limits<FT_Long>::max)()) || length > kMaximumOffset) {
        return {};
    }
    std::unique_ptr<Impl> impl(new Impl);
    const auto* first = static_cast<const unsigned char*>(bytes);
    impl->bytes.assign(first, first + length);
    result = Result::NativeFailure;
    diagnostic = "FreeType library/profile initialization failed";
    nativeError = FT_Init_FreeType(&impl->library);
    if (nativeError != 0) {
        return {};
    }
    constexpr FT_UInt kInterpreterVersion = 40;
    FT_UInt interpreter = kInterpreterVersion;
    nativeError = FT_Property_Set(impl->library, "truetype", "interpreter-version", &interpreter);
    if (nativeError != 0) {
        return {};
    }
    constexpr FT_Pos kOneThirdPixel = 21;
    FT_Vector geometry[3]{{-kOneThirdPixel, 0}, {0, 0}, {kOneThirdPixel, 0}};
    nativeError = FT_Library_SetLcdGeometry(impl->library, geometry);
    if (nativeError != 0) {
        return {};
    }
    result = Result::InvalidFont;
    diagnostic = "indexed memory face is invalid or does not match discovered PostScript identity";
    nativeError = FT_New_Memory_Face(impl->library, impl->bytes.data(), static_cast<FT_Long>(length),
        static_cast<FT_Long>(faceIndex), &impl->face);
    if (nativeError != 0) {
        return {};
    }
    FT_Face face = impl->face;
    const char* postScriptName = FT_Get_Postscript_Name(face);
    if (face->face_index != faceIndex || !postScriptName || std::strcmp(postScriptName, expectedPostScriptName) != 0) {
        return {};
    }
    impl->faceIndex = faceIndex;
    impl->postScriptName = postScriptName;
    const auto* os2 = static_cast<const TT_OS2*>(FT_Get_Sfnt_Table(face, ft_sfnt_os2));
    result = Result::UnsupportedRequest;
    diagnostic = "native face needs scalable SFNT metrics";
    if (!FT_IS_SCALABLE(face) || !os2 || face->units_per_EM == 0 || os2->xAvgCharWidth <= 0
        || face->bbox.xMin < INT_MIN || face->bbox.xMax > INT_MAX || face->bbox.xMax <= face->bbox.xMin) {
        return {};
    }
    impl->intrinsicWeight = os2->usWeightClass;
    impl->symbol = (request.flags & FontSymbol) != 0;
    impl->syntheticBold = request.weight >= kSemiboldWeight && os2->usWeightClass < kSemiboldWeight
        && (face->style_flags & FT_STYLE_FLAG_BOLD) == 0;
    impl->syntheticItalic = (request.flags & FontItalic) != 0 && (face->style_flags & FT_STYLE_FLAG_ITALIC) == 0;
    impl->underline = (request.flags & FontUnderline) != 0;
    impl->strikeout = (request.flags & FontStrikeout) != 0;
    impl->rotated = request.rotationTenths % kTenthsPerTurn != 0;
    if (request.wide != 0) {
        impl->capabilities |= CapabilityWidth;
    }
    if (request.rotationTenths != 0) {
        impl->capabilities |= CapabilityRotation;
    }
    if ((request.flags & FontItalic) != 0) {
        impl->capabilities |= CapabilityItalic;
    }
    if (impl->underline) {
        impl->capabilities |= CapabilityUnderline;
    }
    if (impl->strikeout) {
        impl->capabilities |= CapabilityStrikeout;
    }
    if (impl->symbol) {
        impl->capabilities |= CapabilitySymbol;
    }
    const double angle = (request.rotationTenths % kTenthsPerTurn) * kRadiansPerTenth;
    impl->cosine = std::cos(angle);
    impl->sine = std::sin(angle);
    // FT outlines use y-up coordinates: positive GDI escapement is counterclockwise.
    impl->rotation = {static_cast<FT_Fixed>(std::lround(impl->cosine * kMatrixScale)),
        static_cast<FT_Fixed>(std::lround(-impl->sine * kMatrixScale)),
        static_cast<FT_Fixed>(std::lround(impl->sine * kMatrixScale)),
        static_cast<FT_Fixed>(std::lround(impl->cosine * kMatrixScale))};
    diagnostic = "no unambiguous native byte cmap for the requested charset";
    if (impl->symbol) {
        if (!SymbolMap(face, impl->glyphs)) {
            return {};
        }
    } else {
        if (FT_Select_Charmap(face, FT_ENCODING_UNICODE) != 0) {
            return {};
        }
        for (std::size_t byte = 0; byte < kByteCount; ++byte) {
            impl->glyphs[byte] = FT_Get_Char_Index(face, LegacyByteToUnicode(static_cast<unsigned char>(byte)));
        }
    }
    const std::int64_t cellUnits = static_cast<std::int64_t>(os2->usWinAscent) + os2->usWinDescent;
    const std::int64_t units = face->units_per_EM;
    const std::int64_t emPixels = cellUnits > 0 ? static_cast<std::int64_t>(request.tall) * units / cellUnits : 0;
    auto& metrics = impl->metrics;
    result = Result::Unrepresentable;
    diagnostic = "native metric/canvas values exceed the retained integer API";
    if (emPixels <= 0 || emPixels > INT_MAX
        || !RoundPositive(os2->usWinAscent, emPixels, units, metrics.ascent)
        || !RoundPositive(os2->usWinDescent, emPixels, units, metrics.descent)
        || !RoundPositive(os2->xAvgCharWidth, emPixels, units, metrics.averageWidth)
        || !RoundPositive(static_cast<std::int64_t>(face->bbox.xMax) - face->bbox.xMin,
            emPixels, units, metrics.maximumWidth)
        || !ToInt(static_cast<std::int64_t>(metrics.ascent) + metrics.descent, metrics.height)
        || !ToInt(static_cast<std::int64_t>(metrics.height) - emPixels, metrics.internalLeading)
        || !ToInt(static_cast<std::int64_t>(metrics.height) * 2, impl->canvasHeight)
        || metrics.height <= 0 || metrics.maximumWidth <= 0) {
        return {};
    }
    // lfWidth is the average character width; preserve proportional advances.
    const std::int64_t requestedWidth = request.wide < 0 ? -static_cast<std::int64_t>(request.wide) : request.wide;
    std::int64_t horizontalSize = 0;
    if (requestedWidth != 0) {
        horizontalSize = requestedWidth * units * kFixedScale / os2->xAvgCharWidth;
        if (horizontalSize <= 0 || horizontalSize > INT_MAX || emPixels > INT_MAX / kFixedScale
            || !ToInt(requestedWidth, metrics.averageWidth)
            || !RoundPositive(static_cast<std::int64_t>(face->bbox.xMax) - face->bbox.xMin,
                requestedWidth, os2->xAvgCharWidth, metrics.maximumWidth)) {
            return {};
        }
    }
    result = Result::NativeFailure;
    diagnostic = "FreeType size/glyph metric initialization failed";
    nativeError = horizontalSize == 0 ? FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(emPixels))
        : FT_Set_Char_Size(face, static_cast<FT_F26Dot6>(horizontalSize),
            static_cast<FT_F26Dot6>(emPixels * kFixedScale), 0, 0);
    if (nativeError != 0) {
        return {};
    }
    impl->decorationThickness = static_cast<int>((std::max)(std::int64_t{1},
        static_cast<std::int64_t>(face->underline_thickness) * emPixels / units));
    impl->underlinePosition = static_cast<int>((std::min)(std::int64_t{0},
        static_cast<std::int64_t>(face->underline_position) * emPixels / units));
    impl->strikeoutPosition = static_cast<int>(static_cast<std::int64_t>(os2->yStrikeoutPosition) * emPixels / units);
    for (std::size_t byte = 0; byte < kByteCount; ++byte) {
        nativeError = impl->LoadGlyph(impl->glyphs[byte]);
        auto& abc = impl->abc[byte];
        if (nativeError != 0 || !GlyphMetrics(face->glyph, abc, impl->advances[byte])) {
            return {};
        }
        if (impl->syntheticBold || impl->syntheticItalic) {
            metrics.maximumWidth = (std::max)(metrics.maximumWidth, abc.b);
        }
    }
    std::size_t pixels = 0;
    std::size_t canvasBytes = 0;
    result = Result::Unrepresentable;
    if (!Product(static_cast<std::size_t>(metrics.maximumWidth), static_cast<std::size_t>(impl->canvasHeight), pixels)
        || !Product(pixels, kPixelBytes, canvasBytes)) {
        return {};
    }
    impl->canvas.resize(canvasBytes, 0);
    result = Result::Ok;
    nativeError = 0;
    diagnostic.clear();
    return std::unique_ptr<NativeFontFace>(new NativeFontFace(std::move(impl)));
}

FontMetrics NativeFontFace::Metrics() const noexcept {
    return {impl_->metrics.height, impl_->metrics.maximumWidth, impl_->capabilities};
}

const CandidateMetrics& NativeFontFace::DetailedMetrics() const noexcept {
    return impl_->metrics;
}

unsigned int NativeFontFace::GlyphIndex(unsigned int byte) const noexcept {
    if (byte < impl_->glyphs.size()) {
        return impl_->glyphs[byte];
    }
    byte = static_cast<unsigned int>(cse_vgui1::text::UnicodeValue(static_cast<int>(byte)));
    constexpr unsigned int BmpMaximum = 0xffff;
    constexpr unsigned int FirstSurrogate = 0xd800;
    constexpr unsigned int LastSurrogate = 0xdfff;
    if (impl_->symbol || byte > BmpMaximum || (byte >= FirstSurrogate && byte <= LastSurrogate)) {
        return 0;
    }
    return FT_Get_Char_Index(impl_->face, byte);
}

long NativeFontFace::FaceIndex() const noexcept {
    return impl_->faceIndex;
}

const std::string& NativeFontFace::PostScriptName() const noexcept {
    return impl_->postScriptName;
}

unsigned int NativeFontFace::IntrinsicWeight() const noexcept {
    return impl_->intrinsicWeight;
}

bool NativeFontFace::RawAbc(unsigned int character, int& a, int& b, int& c) noexcept {
    GlyphAbc abc;
    if (character < impl_->abc.size()) {
        abc = impl_->abc[character];
    } else {
        const auto glyph = GlyphIndex(character);
        int advance = 0;
        if (glyph == 0 || impl_->LoadGlyph(glyph) != 0 || !GlyphMetrics(impl_->face->glyph, abc, advance)) {
            return false;
        }
    }
    a = abc.a;
    b = abc.b;
    c = abc.c;
    return true;
}

bool NativeFontFace::Raster(unsigned int byte, int x, int y, int wide, int tall, unsigned char* rgba) noexcept {
    if (wide < 0 || tall < 0) {
        return false;
    }
    GlyphAbc abc;
    int advance = 0;
    const auto glyph = GlyphIndex(byte);
    if (byte < impl_->abc.size()) {
        abc = impl_->abc[byte];
        advance = impl_->advances[byte];
    } else {
        if (glyph == 0 || impl_->LoadGlyph(glyph) != 0 || !GlyphMetrics(impl_->face->glyph, abc, advance)) {
            return false;
        }
    }
    // The legacy GDI-compatible canvas retains state between byte glyphs.
    // Unicode uses a separate zeroed canvas, preserving that byte behavior.
    std::vector<unsigned char> unicodeCanvas;
    if (byte >= impl_->abc.size()) {
        unicodeCanvas.resize(impl_->canvas.size(), 0);
    }
    auto& canvas = byte < impl_->abc.size() ? impl_->canvas : unicodeCanvas;
    std::size_t pixels = 0;
    std::size_t capacity = 0;
    if (!Product(static_cast<std::size_t>(wide), static_cast<std::size_t>(tall), pixels)
        || !Product(pixels, kPixelBytes, capacity)) {
        return false;
    }
    const RgbaCanvas output{rgba, capacity, wide, tall};
    const auto& metrics = impl_->metrics;
    const BgrView view{canvas.data(), canvas.size(), metrics.maximumWidth, impl_->canvasHeight,
        static_cast<std::ptrdiff_t>(metrics.maximumWidth) * static_cast<std::ptrdiff_t>(kPixelBytes)};
    if (TransferLegacyBgr(view, 0, 0, x, y, output, false) != Result::Ok) {
        return false;
    }
    const std::int64_t origin = -static_cast<std::int64_t>(abc.a);
    const std::int64_t backgroundEnd = origin + advance;
    for (int row = 0; row < metrics.height; ++row) {
        for (std::int64_t column = (std::max)(std::int64_t{0}, origin);
            column < (std::min)(static_cast<std::int64_t>(metrics.maximumWidth), backgroundEnd); ++column) {
            auto* pixel = canvas.data()
                + (static_cast<std::size_t>(row) * metrics.maximumWidth
                    + static_cast<std::size_t>(column)) * kPixelBytes;
            std::memset(pixel, 0, kPixelBytes);
        }
    }
    if (impl_->LoadGlyph(glyph) != 0) {
        return false;
    }
    if (impl_->rotated) {
        FT_Outline_Transform(&impl_->face->glyph->outline, &impl_->rotation);
        // ExtTextOut's retained TA_TOP origin rotates the baseline about the
        // top-aligned pen, not about the glyph's baseline.
        FT_Outline_Translate(&impl_->face->glyph->outline,
            static_cast<FT_Pos>(std::llround(metrics.ascent * impl_->sine * kFixedScale)),
            static_cast<FT_Pos>(std::llround(metrics.ascent * (1.0 - impl_->cosine) * kFixedScale)));
    }
    if (FT_Render_Glyph(impl_->face->glyph, FT_RENDER_MODE_LCD) != 0) {
        return false;
    }
    const auto* slot = impl_->face->glyph;
    const auto& bitmap = slot->bitmap;
    constexpr unsigned int kLcdChannels = 3;
    if (bitmap.width != 0 && bitmap.rows != 0) {
        if (bitmap.pixel_mode != FT_PIXEL_MODE_LCD || bitmap.width % kLcdChannels != 0 || bitmap.pitch <= 0
            || static_cast<unsigned int>(bitmap.pitch) < bitmap.width || !bitmap.buffer) {
            return false;
        }
        for (unsigned int row = 0; row < bitmap.rows; ++row) {
            const std::int64_t rowY = static_cast<std::int64_t>(metrics.ascent) - slot->bitmap_top + row;
            if (rowY < 0 || rowY >= impl_->canvasHeight) {
                continue;
            }
            const auto* source = bitmap.buffer + static_cast<std::size_t>(row) * bitmap.pitch;
            for (unsigned int column = 0; column < bitmap.width / kLcdChannels; ++column) {
                const std::int64_t columnX = origin + slot->bitmap_left + column;
                if (columnX < 0 || columnX >= metrics.maximumWidth) {
                    continue;
                }
                auto* pixel = canvas.data()
                    + (static_cast<std::size_t>(rowY) * metrics.maximumWidth
                        + static_cast<std::size_t>(columnX)) * kPixelBytes;
                pixel[0] = source[column * kLcdChannels + 2];
                pixel[1] = source[column * kLcdChannels + 1];
                pixel[2] = source[column * kLcdChannels];
            }
        }
    }
    if (impl_->underline || impl_->strikeout) {
        constexpr double kPixelCenter = 0.5;
        constexpr unsigned char kFullCoverage = 255;
        for (int row = 0; row < metrics.height; ++row) {
            for (int column = 0; column < (std::min)(abc.b, metrics.maximumWidth); ++column) {
                const double rotatedX = column - origin + kPixelCenter;
                const double rotatedY = -row - kPixelCenter;
                const double glyphX = impl_->cosine * rotatedX + impl_->sine * rotatedY;
                const double glyphY = -impl_->sine * rotatedX + impl_->cosine * rotatedY + metrics.ascent;
                const auto onLine = [&](int position) {
                    return glyphY < position && glyphY >= position - impl_->decorationThickness;
                };
                if (glyphX >= 0 && glyphX < advance
                    && ((impl_->underline && onLine(impl_->underlinePosition))
                        || (impl_->strikeout && onLine(impl_->strikeoutPosition)))) {
                    auto* pixel = canvas.data()
                        + (static_cast<std::size_t>(row) * metrics.maximumWidth + column) * kPixelBytes;
                    std::fill(pixel, pixel + kLcdChannels, kFullCoverage);
                }
            }
        }
    }
    return TransferLegacyBgr(view, (std::min)(abc.b, metrics.maximumWidth), metrics.height,
        x, y, output, byte == '\t') == Result::Ok;
}

}
}
