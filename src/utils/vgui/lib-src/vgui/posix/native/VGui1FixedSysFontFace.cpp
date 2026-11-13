#include "VGui1FixedSysFontFace.h"

#include "../../common/VGui1TextEncoding.h"
#include <algorithm>
#include <array>
#include <climits>
#include <cstring>
#include <limits>
#include <utility>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include FT_TRUETYPE_TABLES_H

#if FREETYPE_MAJOR != 2 || FREETYPE_MINOR != 14 || FREETYPE_PATCH != 3
#error Requalify the fixed role before changing the pinned FreeType version
#endif
#if defined(__FAST_MATH__) || defined(_M_FP_FAST)
#error The fixed role uses the sealed strict-FP legacy pixel transfer
#endif

namespace cse_vgui1 {
namespace legacy {
namespace {
constexpr std::size_t kByteCount = 256;
constexpr int kCellWidth = FixedSysCellWidthV1;
constexpr int kCellHeight = FixedSysCellHeightV1;
constexpr int kBaseline = FixedSysAscentV1;
constexpr int kDescent = FixedSysDescentV1;
constexpr int kReferenceEm = kCellHeight - FixedSysInternalLeadingV1;
constexpr int kRequestedHeight = 18;
constexpr int kRoleWeight = FixedSysRoleWeightV1;
constexpr unsigned int kNeutralStyleMask = 3;
constexpr unsigned int kGrayLevels = 256;
constexpr std::size_t kPixelBytes = 4;
constexpr std::size_t kCellBytes = kCellWidth * kCellHeight * kPixelBytes;
constexpr std::size_t kMaximumOffset = static_cast<std::size_t>((std::numeric_limits<std::ptrdiff_t>::max)());
constexpr std::size_t kLongHmtxRecordBytes = 4;
constexpr std::size_t kTrailingHmtxBearingBytes = 2;
constexpr unsigned char kFullCoverage = 255;
constexpr FT_ULong kHmtxTag = FT_MAKE_TAG('h', 'm', 't', 'x');

bool EqualAscii(const std::string& name, const char* expected) {
    std::size_t index = 0;
    for (; index < name.size() && expected[index]; ++index) {
        const auto left = static_cast<unsigned char>(name[index]);
        const auto right = static_cast<unsigned char>(expected[index]);
        const auto a = left >= 'A' && left <= 'Z' ? left - 'A' + 'a' : left;
        const auto b = right >= 'A' && right <= 'Z' ? right - 'A' + 'a' : right;
        if (a != b) {
            return false;
        }
    }
    return index == name.size() && expected[index] == 0;
}

FixedSysByteRoute ByteRoute(unsigned int byte) {
    constexpr std::array<unsigned int, 16> kBlankBytes{{
        0x00, 0x02, 0x09, 0x0a, 0x0d, 0x1c, 0x1d, 0x1e,
        0x1f, 0x20, 0x81, 0x8d, 0x8f, 0x90, 0x9d, 0xa0}};
    if (std::find(kBlankBytes.begin(), kBlankBytes.end(), byte) != kBlankBytes.end()) {
        return FixedSysByteRoute::ForcedBlank;
    }
    if (byte == 0x0c || byte == 0x7f) {
        return FixedSysByteRoute::AuthoredDefaultBlock;
    }
    if (byte < 0x20) {
        return FixedSysByteRoute::NativeNotdef;
    }
    // All defined graphics, including the valid C1/Euro bytes whose old GDI
    // fallback image was a block, keep their own native cmap glyphs.
    return FixedSysByteRoute::MappedGraphic;
}

bool GraphicByte(unsigned int byte) {
    return ByteRoute(byte) == FixedSysByteRoute::MappedGraphic || byte == 0x20 || byte == 0xa0;
}

bool Product(std::size_t left, std::size_t right, std::size_t& output) {
    if (right != 0 && left > kMaximumOffset / right) {
        return false;
    }
    output = left * right;
    return true;
}

unsigned int Big16(const unsigned char* bytes) {
    constexpr unsigned int kHighByteShift = 8;
    return (static_cast<unsigned int>(bytes[0]) << kHighByteShift) | bytes[1];
}
}

struct FixedSysFontFace::Impl {
    std::vector<unsigned char> bytes;
    FT_Library library = nullptr;
    FT_Face face = nullptr;
    std::array<FT_UInt, kByteCount> glyphs{};
    std::array<std::array<unsigned char, kCellBytes>, kByteCount> cells{};
    FixedSysRoleInfo info;
    unsigned int commonAdvance = 0;
    unsigned int units = 0;

    ~Impl() {
        if (face) {
            FT_Done_Face(face);
        }
        if (library) {
            FT_Done_FreeType(library);
        }
    }

    bool Load(FT_UInt glyph, Result& result, int& error) {
        result = Result::NativeFailure;
        error = FT_Load_Glyph(face, glyph, FT_LOAD_TARGET_NORMAL);
        if (error != 0) {
            return false;
        }
        if (face->glyph->format != FT_GLYPH_FORMAT_OUTLINE) {
            result = Result::UnsupportedRequest;
            return false;
        }
        error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
        if (error != 0) {
            return false;
        }
        const auto& bitmap = face->glyph->bitmap;
        result = Result::UnsupportedRequest;
        if (bitmap.width == 0 && bitmap.rows == 0) {
            return true;
        }
        if (bitmap.width == 0 || bitmap.rows == 0 || bitmap.pixel_mode != FT_PIXEL_MODE_GRAY
            || bitmap.num_grays != kGrayLevels || bitmap.pitch <= 0 || !bitmap.buffer
            || static_cast<unsigned int>(bitmap.pitch) < bitmap.width) {
            return false;
        }
        std::size_t rows = 0;
        if (!Product(static_cast<std::size_t>(bitmap.rows - 1), static_cast<std::size_t>(bitmap.pitch), rows)
            || bitmap.width > kMaximumOffset - rows) {
            result = Result::Unrepresentable;
            return false;
        }
        return true;
    }

    bool Nonempty() const {
        const auto& bitmap = face->glyph->bitmap;
        for (unsigned int row = 0; row < bitmap.rows; ++row) {
            const auto* source = bitmap.buffer + static_cast<std::size_t>(row) * bitmap.pitch;
            for (unsigned int column = 0; column < bitmap.width; ++column) {
                const unsigned char value = source[column];
                const unsigned char bgr[]{value, value, value, 0};
                unsigned char rgba[kPixelBytes]{};
                ConvertLegacyBgr(bgr, false, rgba);
                if (rgba[kPixelBytes - 1] != 0) {
                    return true;
                }
            }
        }
        return false;
    }

    bool TrySize(int ppem, Result& result, int& error, bool& fits) {
        result = Result::NativeFailure;
        error = FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(ppem));
        if (error != 0) {
            return false;
        }
        std::int64_t left = 0;
        std::int64_t right = (static_cast<std::int64_t>(commonAdvance) * ppem + units - 1) / units;
        std::int64_t top = 0;
        std::int64_t bottom = 0;
        fits = true;
        for (unsigned int byte = 0; byte < kByteCount; ++byte) {
            const auto route = ByteRoute(byte);
            if (route != FixedSysByteRoute::MappedGraphic && route != FixedSysByteRoute::NativeNotdef) {
                continue;
            }
            if (!Load(glyphs[byte], result, error)) {
                return false;
            }
            if (!Nonempty()) {
                fits = false;
            }
            const auto* slot = face->glyph;
            left = (std::min)(left, static_cast<std::int64_t>(slot->bitmap_left));
            right = (std::max)(right, static_cast<std::int64_t>(slot->bitmap_left) + slot->bitmap.width);
            top = (std::max)(top, static_cast<std::int64_t>(slot->bitmap_top));
            bottom = (std::max)(bottom, static_cast<std::int64_t>(slot->bitmap.rows) - slot->bitmap_top);
        }
        fits = fits && right - left <= kCellWidth && top <= kBaseline && bottom <= kDescent;
        if (fits) {
            // left starts at 0 and right is positive: an 8px span, top<=12 and
            // bottom<=3 prove these retained-int diagnostic conversions fit.
            info.fitPpem = ppem;
            info.originX = static_cast<int>(-left);
            info.minimumBitmapLeft = static_cast<int>(left);
            info.maximumBitmapRight = static_cast<int>(right);
            info.maximumBitmapTop = static_cast<int>(top);
            info.maximumBitmapBottom = static_cast<int>(bottom);
        }
        return true;
    }

    bool Cache(Result& result, int& error) {
        constexpr int kDefaultLeft = 1;
        constexpr int kDefaultTop = 3;
        constexpr int kDefaultWidth = 6;
        constexpr int kDefaultHeight = 9;
        for (unsigned int byte = 0; byte < kByteCount; ++byte) {
            auto& cell = cells[byte];
            cell.fill(0);
            const auto route = ByteRoute(byte);
            if (route == FixedSysByteRoute::ForcedBlank) {
                continue;
            }
            if (route == FixedSysByteRoute::AuthoredDefaultBlock) {
                for (int y = kDefaultTop; y < kDefaultTop + kDefaultHeight; ++y) {
                    for (int x = kDefaultLeft; x < kDefaultLeft + kDefaultWidth; ++x) {
                        const auto offset = static_cast<std::size_t>(y * kCellWidth + x) * kPixelBytes;
                        cell[offset] = kFullCoverage;
                        cell[offset + 1] = kFullCoverage;
                        cell[offset + 2] = kFullCoverage;
                    }
                }
                continue;
            }
            if (!Load(glyphs[byte], result, error) || !Nonempty()) {
                return false;
            }
            const auto* slot = face->glyph;
            const auto& bitmap = slot->bitmap;
            const std::int64_t x0 = static_cast<std::int64_t>(info.originX) + slot->bitmap_left;
            const std::int64_t y0 = static_cast<std::int64_t>(kBaseline) - slot->bitmap_top;
            if (x0 < 0 || y0 < 0 || x0 + bitmap.width > kCellWidth || y0 + bitmap.rows > kCellHeight) {
                result = Result::Unrepresentable;
                return false;
            }
            for (unsigned int y = 0; y < bitmap.rows; ++y) {
                const auto* row = bitmap.buffer + static_cast<std::size_t>(y) * bitmap.pitch;
                for (unsigned int x = 0; x < bitmap.width; ++x) {
                    const auto offset = (static_cast<std::size_t>(y0 + y) * kCellWidth
                        + static_cast<std::size_t>(x0 + x)) * kPixelBytes;
                    cell[offset] = row[x];
                    cell[offset + 1] = row[x];
                    cell[offset + 2] = row[x];
                }
            }
        }
        return true;
    }
};

FixedSysFontFace::FixedSysFontFace(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
FixedSysFontFace::~FixedSysFontFace() = default;

bool FixedSysFontFace::Matches(const FontRequest& request) noexcept {
    return request.resolutionProfile == FixedSysRoleProfileV1 && EqualAscii(request.requestedName, "FixedSys")
        && request.lookupIdentity == "Courier" && request.tall == kRequestedHeight && request.wide == 0
        && request.rotation == 0.0f && request.rotationTenths == 0 && request.weight == kRoleWeight
        && request.flags == 0 && request.ansiCodePage == LegacyAnsiCodePage;
}

std::unique_ptr<FixedSysFontFace> FixedSysFontFace::CreateOwned(const void* bytes, std::size_t length,
    long faceIndex, const char* expectedPostScriptName, const FontRequest& request,
    Result& result, int& nativeError, std::string& diagnostic) noexcept {
    result = Result::InvalidArgument;
    nativeError = 0;
    diagnostic = "invalid FixedSys role input/index/identity";
    if (!bytes || length == 0 || faceIndex != 0 || !expectedPostScriptName
        || std::strcmp(expectedPostScriptName, "Courier") != 0) {
        return {};
    }
    result = Result::UnsupportedRequest;
    diagnostic = "request does not match the explicit FixedSys18 role";
    if (!Matches(request)) {
        return {};
    }
    result = Result::Unrepresentable;
    diagnostic = "FixedSys font byte span is not addressable by FreeType";
    if (length > kMaximumOffset || length > static_cast<std::size_t>((std::numeric_limits<FT_Long>::max)())) {
        return {};
    }
    std::unique_ptr<Impl> impl(new Impl);
    const auto* first = static_cast<const unsigned char*>(bytes);
    impl->bytes.assign(first, first + length);
    result = Result::NativeFailure;
    diagnostic = "FixedSys FreeType initialization failed";
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
    result = Result::InvalidFont;
    diagnostic = "FixedSys indexed memory face/PS identity is invalid";
    nativeError = FT_New_Memory_Face(impl->library, impl->bytes.data(), static_cast<FT_Long>(length), 0, &impl->face);
    if (nativeError != 0) {
        return {};
    }
    FT_Face face = impl->face;
    const char* ps = FT_Get_Postscript_Name(face);
    if (face->face_index != 0 || !ps || std::strcmp(ps, expectedPostScriptName) != 0) {
        return {};
    }
    const auto* head = static_cast<const TT_Header*>(FT_Get_Sfnt_Table(face, ft_sfnt_head));
    const auto* hhea = static_cast<const TT_HoriHeader*>(FT_Get_Sfnt_Table(face, ft_sfnt_hhea));
    const auto* maxp = static_cast<const TT_MaxProfile*>(FT_Get_Sfnt_Table(face, ft_sfnt_maxp));
    const auto* post = static_cast<const TT_Postscript*>(FT_Get_Sfnt_Table(face, ft_sfnt_post));
    result = Result::UnsupportedRequest;
    diagnostic = "FixedSys role needs neutral OS2-less scalable SFNT head/hhea/hmtx/cmap/post metrics";
    if (!FT_IS_SFNT(face) || !FT_IS_SCALABLE(face) || FT_HAS_MULTIPLE_MASTERS(face)
        || FT_HAS_COLOR(face) || !head || !hhea || !maxp || !post || FT_Get_Sfnt_Table(face, ft_sfnt_os2)
        || head->Units_Per_EM == 0 || head->Units_Per_EM != face->units_per_EM
        || head->xMin >= head->xMax || head->yMin >= head->yMax
        || hhea->Ascender <= 0 || hhea->Descender > 0 || hhea->Line_Gap != 0
        || hhea->metric_Data_Format != 0 || (head->Mac_Style & kNeutralStyleMask) != 0 || post->italicAngle != 0
        || (face->style_flags & (FT_STYLE_FLAG_BOLD | FT_STYLE_FLAG_ITALIC)) != 0
        || maxp->numGlyphs == 0 || face->num_glyphs != maxp->numGlyphs || hhea->number_Of_HMetrics == 0
        || hhea->number_Of_HMetrics > maxp->numGlyphs) {
        return {};
    }
    FT_ULong tableLength = 0;
    nativeError = FT_Load_Sfnt_Table(face, kHmtxTag, 0, nullptr, &tableLength);
    const std::size_t minimumHmtx = static_cast<std::size_t>(hhea->number_Of_HMetrics) * kLongHmtxRecordBytes
        + static_cast<std::size_t>(maxp->numGlyphs - hhea->number_Of_HMetrics) * kTrailingHmtxBearingBytes;
    if (nativeError != 0 || tableLength < minimumHmtx || tableLength > length) {
        return {};
    }
    std::vector<unsigned char> hmtx(static_cast<std::size_t>(tableLength));
    nativeError = FT_Load_Sfnt_Table(face, kHmtxTag, 0, hmtx.data(), &tableLength);
    if (nativeError != 0 || tableLength < minimumHmtx || FT_Select_Charmap(face, FT_ENCODING_UNICODE) != 0) {
        return {};
    }
    for (unsigned int byte = 0; byte < kByteCount; ++byte) {
        const auto mapped = FT_Get_Char_Index(face, LegacyByteToUnicode(static_cast<unsigned char>(byte)));
        if (GraphicByte(byte) && mapped == 0) {
            diagnostic = "FixedSys donor is missing a required CP1252 graphic mapping";
            return {};
        }
        impl->glyphs[byte] = ByteRoute(byte) == FixedSysByteRoute::NativeNotdef ? 0 : mapped;
        constexpr unsigned int kAsciiFirst = 0x20;
        constexpr unsigned int kAsciiLast = 0x7e;
        if (byte >= kAsciiFirst && byte <= kAsciiLast) {
            const auto metric = (std::min)(mapped, static_cast<FT_UInt>(hhea->number_Of_HMetrics - 1));
            const auto advance = Big16(hmtx.data() + static_cast<std::size_t>(metric) * kLongHmtxRecordBytes);
            if (advance == 0 || (impl->commonAdvance != 0 && advance != impl->commonAdvance)) {
                diagnostic = "FixedSys donor has no common positive ASCII design advance";
                return {};
            }
            impl->commonAdvance = advance;
        }
    }
    impl->units = head->Units_Per_EM;
    const std::int64_t cellUnits = static_cast<std::int64_t>(hhea->Ascender) - hhea->Descender;
    const int maximumPpem = static_cast<int>((std::min)(static_cast<std::int64_t>(kReferenceEm),
        static_cast<std::int64_t>(kCellHeight) * impl->units / cellUnits));
    diagnostic = "no one gray global fit preserves all FixedSys graphic/notdef ink inside the fixed cell";
    for (int ppem = maximumPpem; ppem > 0; --ppem) {
        bool fits = false;
        if (!impl->TrySize(ppem, result, nativeError, fits)) {
            return {};
        }
        if (fits) {
            if (!impl->Cache(result, nativeError)) {
                return {};
            }
            result = Result::Ok;
            nativeError = 0;
            diagnostic.clear();
            return std::unique_ptr<FixedSysFontFace>(new FixedSysFontFace(std::move(impl)));
        }
    }
    result = Result::UnsupportedRequest;
    nativeError = 0;
    return {};
}

FontMetrics FixedSysFontFace::Metrics() const noexcept {
    return {kCellHeight, kCellWidth, CapabilityAbc | CapabilityLegacyRgba | CapabilityPenOffsets | CapabilityWeight};
}

bool FixedSysFontFace::RawAbc(unsigned int byte, int& a, int& b, int& c) noexcept {
    if (byte >= kByteCount && GlyphIndex(byte) == 0) {
        return false;
    }
    a = 0;
    b = kCellWidth;
    c = 0;
    return true;
}

bool FixedSysFontFace::Raster(unsigned int byte, int x, int y, int wide, int tall, unsigned char* rgba) noexcept {
    if (x < 0 || y < 0 || wide < 0 || tall < 0 || (byte >= kByteCount && GlyphIndex(byte) == 0)) {
        return false;
    }
    std::size_t pixels = 0;
    std::size_t bytes = 0;
    if (!Product(static_cast<std::size_t>(wide), static_cast<std::size_t>(tall), pixels)
        || !Product(pixels, kPixelBytes, bytes)) {
        return false;
    }
    std::array<unsigned char, kCellBytes> unicodeCell{};
    if (byte >= kByteCount) {
        bool fits = false;
        Result result{};
        int error = 0;
        // Keep the measured legacy 8x15 cell/advance. A taller accented donor
        // glyph may need a smaller realization to retain all of its ink.
        for (int ppem = impl_->info.fitPpem; ppem > 0; --ppem) {
            if (FT_Set_Pixel_Sizes(impl_->face, 0, static_cast<FT_UInt>(ppem)) != 0
                || !impl_->Load(GlyphIndex(byte), result, error)) {
                break;
            }
            const auto* slot = impl_->face->glyph;
            const auto& bitmap = slot->bitmap;
            const int top = kBaseline - slot->bitmap_top;
            if (bitmap.width > static_cast<unsigned int>(kCellWidth) || top < 0
                || static_cast<std::int64_t>(top) + bitmap.rows > kCellHeight) {
                continue;
            }
            const int left = (kCellWidth - static_cast<int>(bitmap.width)) / 2;
            for (unsigned int row = 0; row < bitmap.rows; ++row) {
                for (unsigned int column = 0; column < bitmap.width; ++column) {
                    const auto target = (static_cast<std::size_t>(top + row) * kCellWidth
                        + left + column) * kPixelBytes;
                    const auto value = bitmap.buffer[static_cast<std::size_t>(row) * bitmap.pitch + column];
                    unicodeCell[target] = value;
                    unicodeCell[target + 1] = value;
                    unicodeCell[target + 2] = value;
                }
            }
            fits = impl_->Nonempty();
            break;
        }
        const int restored = FT_Set_Pixel_Sizes(impl_->face, 0, static_cast<FT_UInt>(impl_->info.fitPpem));
        if (!fits || restored != 0) {
            return false;
        }
    }
    const auto& cell = byte < kByteCount ? impl_->cells[byte] : unicodeCell;
    const BgrView source{cell.data(), cell.size(), kCellWidth, kCellHeight, kCellWidth * kPixelBytes};
    return TransferLegacyBgr(source, kCellWidth, kCellHeight, x, y, {rgba, bytes, wide, tall}, byte == '\t')
        == Result::Ok;
}

bool FixedSysFontFace::Route(unsigned int byte, FixedSysByteRoute& route) const noexcept {
    if (byte >= kByteCount) {
        return false;
    }
    route = ByteRoute(byte);
    return true;
}

unsigned int FixedSysFontFace::GlyphIndex(unsigned int byte) const noexcept {
    if (byte < kByteCount) {
        return impl_->glyphs[byte];
    }
    byte = static_cast<unsigned int>(cse_vgui1::text::UnicodeValue(static_cast<int>(byte)));
    constexpr unsigned int BmpMaximum = 0xffff;
    constexpr unsigned int FirstSurrogate = 0xd800;
    constexpr unsigned int LastSurrogate = 0xdfff;
    if (byte > BmpMaximum || (byte >= FirstSurrogate && byte <= LastSurrogate)) {
        return 0;
    }
    return FT_Get_Char_Index(impl_->face, byte);
}

const FixedSysRoleInfo& FixedSysFontFace::Info() const noexcept {
    return impl_->info;
}

}
}
