#include "VGui1LegacyRaster.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_LCD_FILTER_H
#include FT_MODULE_H
#include FT_TRUETYPE_TABLES_H

#if FREETYPE_MAJOR != 2 || FREETYPE_MINOR != 14 || FREETYPE_PATCH != 3
#error Requalify this isolated candidate before changing the pinned FreeType version
#endif

#if defined(__FAST_MATH__) || defined(_M_FP_FAST)
#error The legacy pixel conversion requires its explicit strict-FP source policy
#endif

namespace cse_vgui1 {
namespace legacy {
namespace {
constexpr std::size_t kPixelBytes = 4;
constexpr double kByteMaximum = 255.0;
constexpr std::size_t kMaximumOffset = static_cast<std::size_t>((std::numeric_limits<std::ptrdiff_t>::max)());

bool Product(std::size_t left, std::size_t right, std::size_t& result) {
    if (right != 0 && left > kMaximumOffset / right) {
        return false;
    }
    result = left * right;
    return true;
}

bool SpansOverlap(const void* left, std::size_t leftBytes, const void* right, std::size_t rightBytes) {
    const auto a = reinterpret_cast<std::uintptr_t>(left);
    const auto b = reinterpret_cast<std::uintptr_t>(right);
    return a <= b ? b - a < leftBytes : a - b < rightBytes;
}
}

Result ConvertLegacyBgr(const unsigned char* bgr, bool tab, unsigned char* output) noexcept {
    if (!bgr || !output) {
        return Result::InvalidArgument;
    }
    // Preserve the original float reciprocal and intermediate precision. The
    // weights are float literals promoted to double in the legacy expression.
    const float byteToUnit = 1.0f / 255.0f;
    const float blue = tab ? 0.0f : static_cast<float>(bgr[0] * static_cast<double>(byteToUnit));
    const float green = tab ? 0.0f : static_cast<float>(bgr[1] * static_cast<double>(byteToUnit));
    const double red = tab ? 0.0 : bgr[2] * static_cast<double>(byteToUnit);
    output[0] = static_cast<unsigned char>(static_cast<int>(blue * kByteMaximum));
    output[1] = static_cast<unsigned char>(static_cast<int>(green * kByteMaximum));
    output[2] = static_cast<unsigned char>(static_cast<int>(red * kByteMaximum));
    const double coverage = red * 0.11f + static_cast<double>(green) * 0.55f + static_cast<double>(blue) * 0.34f;
    output[3] = static_cast<unsigned char>(static_cast<int>(coverage * kByteMaximum));
    return Result::Ok;
}

Result TransferLegacyBgr(const BgrView& source, int copiedWidth, int copiedHeight,
    int penX, int penY, const RgbaCanvas& destination, bool tab) noexcept {
    if (source.width < 0 || source.height < 0 || destination.width < 0 || destination.height < 0
        || copiedWidth < 0 || copiedHeight < 0 || copiedWidth > source.width || copiedHeight > source.height
        || penX < 0 || penY < 0 || source.pitch == (std::numeric_limits<std::ptrdiff_t>::min)()) {
        return Result::InvalidArgument;
    }
    const auto stride = static_cast<std::size_t>(source.pitch < 0 ? -source.pitch : source.pitch);
    std::size_t rowBytes = 0;
    std::size_t sourceRows = 0;
    std::size_t canvasPixels = 0;
    std::size_t canvasBytes = 0;
    if (!Product(static_cast<std::size_t>(source.width), kPixelBytes, rowBytes) || stride < rowBytes
        || !Product(source.height > 0 ? static_cast<std::size_t>(source.height - 1) : 0, stride, sourceRows)
        || sourceRows > kMaximumOffset - rowBytes
        || !Product(static_cast<std::size_t>(destination.width), static_cast<std::size_t>(destination.height),
            canvasPixels) || !Product(canvasPixels, kPixelBytes, canvasBytes)) {
        return Result::InvalidArgument;
    }
    const std::size_t sourceBytes = source.height == 0 ? 0 : sourceRows + rowBytes;
    if (sourceBytes > source.capacity || canvasBytes > destination.capacity
        || (sourceBytes != 0 && !source.data) || (canvasBytes != 0 && !destination.data)
        || (sourceBytes != 0 && canvasBytes != 0
            && SpansOverlap(source.data, sourceBytes, destination.data, canvasBytes))) {
        return Result::InvalidArgument;
    }
    if (penX >= destination.width || penY >= destination.height) {
        return Result::Ok;
    }
    const int rows = (std::min)(copiedHeight, destination.height - penY);
    const int columns = (std::min)(copiedWidth, destination.width - penX);
    for (int row = 0; row < rows; ++row) {
        const auto sourceRow = static_cast<std::size_t>(source.pitch < 0 ? source.height - 1 - row : row);
        for (int column = 0; column < columns; ++column) {
            const auto* pixel = source.data + sourceRow * stride + static_cast<std::size_t>(column) * kPixelBytes;
            auto* output = destination.data + (static_cast<std::size_t>(penY + row) * destination.width
                + static_cast<std::size_t>(penX + column)) * kPixelBytes;
            ConvertLegacyBgr(pixel, tab, output);
        }
    }
    return Result::Ok;
}

std::uint16_t LegacyByteToUnicode(unsigned char byte) noexcept {
    constexpr unsigned int kFirstExtendedByte = 0x80;
    constexpr std::array<std::uint16_t, 32> kExtendedUnits{{
        0x20ac, 0x0081, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021,
        0x02c6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008d, 0x017d, 0x008f,
        0x0090, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
        0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, 0x009d, 0x017e, 0x0178}};
    return byte >= kFirstExtendedByte && byte - kFirstExtendedByte < kExtendedUnits.size()
        ? kExtendedUnits[byte - kFirstExtendedByte] : byte;
}

namespace {
constexpr std::size_t kLegacyByteCount = 256;
constexpr std::int64_t kFixedScale = 64;

bool ToInt(std::int64_t value, int& output) {
    if (value < INT_MIN || value > INT_MAX) {
        return false;
    }
    output = static_cast<int>(value);
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
    if (!ToInt((static_cast<std::int64_t>(metrics.horiAdvance) + kFixedScale / 2) / kFixedScale, advance)
        || !ToInt(left, output.a) || !ToInt(black, output.b)
        || !ToInt(static_cast<std::int64_t>(advance) - left - black, output.c)) {
        return false;
    }
    return true;
}
}

struct HarmonyCandidate::Impl {
    std::vector<unsigned char> bytes;
    FT_Library library = nullptr;
    FT_Face face = nullptr;
    CandidateMetrics metrics;
    std::array<GlyphAbc, kLegacyByteCount> abc{};
    std::array<int, kLegacyByteCount> advances{};
    std::array<FT_UInt, kLegacyByteCount> glyphs{};
    std::vector<unsigned char> canvas;
    int canvasHeight = 0;

    ~Impl() {
        if (face) {
            FT_Done_Face(face);
        }
        if (library) {
            FT_Done_FreeType(library);
        }
    }
};

HarmonyCandidate::HarmonyCandidate(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}

HarmonyCandidate::~HarmonyCandidate() = default;

std::unique_ptr<HarmonyCandidate> HarmonyCandidate::CreateOwned(const void* bytes, std::size_t length,
    int cellHeight, Result& result, int& nativeError) noexcept {
    result = Result::InvalidArgument;
    nativeError = 0;
    if (!bytes || length == 0 || cellHeight <= 0) {
        return {};
    }
    if (length > static_cast<std::size_t>((std::numeric_limits<FT_Long>::max)()) || length > kMaximumOffset) {
        result = Result::Unrepresentable;
        return {};
    }
    std::unique_ptr<Impl> impl(new Impl);
    const auto* first = static_cast<const unsigned char*>(bytes);
    impl->bytes.assign(first, first + length);
    nativeError = FT_Init_FreeType(&impl->library);
    if (nativeError != 0) {
        result = Result::NativeFailure;
        return {};
    }
    constexpr FT_UInt kInterpreterVersion = 40;
    FT_UInt interpreter = kInterpreterVersion;
    nativeError = FT_Property_Set(impl->library, "truetype", "interpreter-version", &interpreter);
    if (nativeError != 0) {
        result = Result::UnsupportedRequest;
        return {};
    }
    constexpr FT_Pos kOneThirdPixel = 21;
    FT_Vector geometry[3]{{-kOneThirdPixel, 0}, {0, 0}, {kOneThirdPixel, 0}};
    nativeError = FT_Library_SetLcdGeometry(impl->library, geometry);
    if (nativeError != 0) {
        result = Result::UnsupportedRequest;
        return {};
    }
    nativeError = FT_New_Memory_Face(impl->library, impl->bytes.data(), static_cast<FT_Long>(length), 0, &impl->face);
    if (nativeError != 0) {
        result = Result::InvalidFont;
        return {};
    }
    FT_Face face = impl->face;
    const auto* os2 = static_cast<const TT_OS2*>(FT_Get_Sfnt_Table(face, ft_sfnt_os2));
    if (!FT_IS_SCALABLE(face) || !os2 || face->units_per_EM == 0 || os2->xAvgCharWidth <= 0
        || face->bbox.xMin < INT_MIN || face->bbox.xMax > INT_MAX || face->bbox.xMax <= face->bbox.xMin) {
        result = Result::UnsupportedRequest;
        return {};
    }
    const std::int64_t cellUnits = static_cast<std::int64_t>(os2->usWinAscent) + os2->usWinDescent;
    const std::int64_t units = face->units_per_EM;
    const std::int64_t emPixels = cellUnits > 0 ? static_cast<std::int64_t>(cellHeight) * units / cellUnits : 0;
    auto& metrics = impl->metrics;
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
        result = Result::Unrepresentable;
        return {};
    }
    std::size_t pixels = 0;
    std::size_t canvasBytes = 0;
    if (!Product(static_cast<std::size_t>(metrics.maximumWidth), static_cast<std::size_t>(impl->canvasHeight), pixels)
        || !Product(pixels, kPixelBytes, canvasBytes)) {
        result = Result::Unrepresentable;
        return {};
    }
    nativeError = FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(emPixels));
    if (nativeError != 0) {
        result = Result::NativeFailure;
        return {};
    }
    for (std::size_t byte = 0; byte < kLegacyByteCount; ++byte) {
        impl->glyphs[byte] = FT_Get_Char_Index(face, LegacyByteToUnicode(static_cast<unsigned char>(byte)));
        nativeError = FT_Load_Glyph(face, impl->glyphs[byte], FT_LOAD_TARGET_LCD);
        if (nativeError != 0) {
            result = Result::NativeFailure;
            return {};
        }
        if (!GlyphMetrics(face->glyph, impl->abc[byte], impl->advances[byte])) {
            result = Result::Unrepresentable;
            return {};
        }
    }
    impl->canvas.resize(canvasBytes, 0);
    result = Result::Ok;
    return std::unique_ptr<HarmonyCandidate>(new HarmonyCandidate(std::move(impl)));
}

const CandidateMetrics& HarmonyCandidate::GetMetrics() const noexcept {
    return impl_->metrics;
}

unsigned int HarmonyCandidate::GetGlyphIndex(unsigned int byte) const noexcept {
    return byte < impl_->glyphs.size() ? impl_->glyphs[byte] : 0;
}

Result HarmonyCandidate::GetAbc(unsigned int byte, GlyphAbc& result) const noexcept {
    if (byte >= impl_->abc.size()) {
        return Result::InvalidArgument;
    }
    result = impl_->abc[byte];
    return Result::Ok;
}

Result HarmonyCandidate::Raster(unsigned int byte, int penX, int penY, const RgbaCanvas& output) noexcept {
    if (byte >= impl_->abc.size()) {
        return Result::InvalidArgument;
    }
    const auto& metrics = impl_->metrics;
    const BgrView view{impl_->canvas.data(), impl_->canvas.size(), metrics.maximumWidth, impl_->canvasHeight,
        static_cast<std::ptrdiff_t>(metrics.maximumWidth) * static_cast<std::ptrdiff_t>(kPixelBytes)};
    const Result validated = TransferLegacyBgr(view, 0, 0, penX, penY, output, false);
    if (validated != Result::Ok) {
        return validated;
    }
    const auto& abc = impl_->abc[byte];
    const std::int64_t origin = -static_cast<std::int64_t>(abc.a);
    const std::int64_t backgroundEnd = origin + impl_->advances[byte];
    for (int row = 0; row < metrics.height; ++row) {
        for (std::int64_t col = (std::max)(std::int64_t{0}, origin);
            col < (std::min)(static_cast<std::int64_t>(metrics.maximumWidth), backgroundEnd); ++col) {
            auto* pixel = impl_->canvas.data()
                + (static_cast<std::size_t>(row) * metrics.maximumWidth + static_cast<std::size_t>(col)) * kPixelBytes;
            std::memset(pixel, 0, kPixelBytes);
        }
    }
    FT_Error error = FT_Load_Glyph(impl_->face, impl_->glyphs[byte], FT_LOAD_TARGET_LCD);
    if (error != 0 || FT_Render_Glyph(impl_->face->glyph, FT_RENDER_MODE_LCD) != 0) {
        return Result::NativeFailure;
    }
    const auto* slot = impl_->face->glyph;
    const auto& bitmap = slot->bitmap;
    constexpr unsigned int kLcdChannels = 3;
    if (bitmap.width != 0 && bitmap.rows != 0) {
        if (bitmap.pixel_mode != FT_PIXEL_MODE_LCD || bitmap.width % kLcdChannels != 0 || bitmap.pitch <= 0
            || static_cast<unsigned int>(bitmap.pitch) < bitmap.width || !bitmap.buffer) {
            return Result::UnsupportedRequest;
        }
        for (unsigned int row = 0; row < bitmap.rows; ++row) {
            const std::int64_t y = static_cast<std::int64_t>(metrics.ascent) - slot->bitmap_top + row;
            if (y < 0 || y >= impl_->canvasHeight) {
                continue;
            }
            const auto* source = bitmap.buffer + static_cast<std::size_t>(row) * bitmap.pitch;
            for (unsigned int col = 0; col < bitmap.width / kLcdChannels; ++col) {
                const std::int64_t x = origin + slot->bitmap_left + col;
                if (x < 0 || x >= metrics.maximumWidth) {
                    continue;
                }
                auto* pixel = impl_->canvas.data()
                    + (static_cast<std::size_t>(y) * metrics.maximumWidth + static_cast<std::size_t>(x)) * kPixelBytes;
                pixel[0] = source[col * kLcdChannels + 2];
                pixel[1] = source[col * kLcdChannels + 1];
                pixel[2] = source[col * kLcdChannels];
            }
        }
    }
    return TransferLegacyBgr(view, (std::min)(abc.b, metrics.maximumWidth), metrics.height,
        penX, penY, output, byte == '\t');
}

}
}
