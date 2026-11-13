#if !defined(__APPLE__)
#error This private VGUI1 adapter must be selected only for Apple targets
#endif

#include "VGUI_Font.h"
#include "VGui1FontPlatform.h"
#include "fileimage.h"
#include "vfontdata.h"
#include <climits>
#include <limits>
#include <memory>

namespace vgui {
namespace {
class BitmapFontPlat final : public BaseFontPlat {
public:
    BitmapFontPlat(const char* name, int weight, bool italic, bool underline, bool strikeout)
        : name_(name ? name : ""), weight_(weight), italic_(italic), underline_(underline), strikeout_(strikeout) {}

    bool Load(void* data, int length) {
        constexpr std::size_t kTgaHeaderBytes = 18;
        constexpr std::size_t kTgaWidthOffset = 12;
        constexpr std::size_t kTgaHeightOffset = 14;
        constexpr unsigned int kHighByteShift = CHAR_BIT;
        constexpr std::uint64_t kMaximumRunPixels = 128;
        constexpr std::uint64_t kMinimumRunBytes = 1 + VGUI_RGBA_BYTES;
        if (!data || length < static_cast<int>(kTgaHeaderBytes)) {
            return false;
        }
        const auto* bytes = static_cast<const unsigned char*>(data);
        const auto width = static_cast<unsigned int>(bytes[kTgaWidthOffset])
            | (static_cast<unsigned int>(bytes[kTgaWidthOffset + 1]) << kHighByteShift);
        const auto height = static_cast<unsigned int>(bytes[kTgaHeightOffset])
            | (static_cast<unsigned int>(bytes[kTgaHeightOffset + 1]) << kHighByteShift);
        const std::uint64_t pixels = static_cast<std::uint64_t>(width) * height;
        const std::uint64_t minimumBytes = kTgaHeaderBytes + bytes[0]
            + ((width + kMaximumRunPixels - 1) / kMaximumRunPixels) * height * kMinimumRunBytes;
        // The retained decoder uses signed-int products. Reject malformed or
        // unrepresentable inputs before its allocations; valid TGA rules stay
        // in that decoder, including its legacy row/run and mask behavior.
        if (width < VFONT_GLYPH_COUNT || height == 0 || pixels > INT_MAX / VGUI_RGBA_BYTES
            || minimumBytes > static_cast<std::uint64_t>(length)) {
            return false;
        }
        FileImageStream_Memory stream(data, length);
        const bool loaded = LoadVFontDataFrom32BitTGA(&stream, &data_);
        return loaded && !stream.ErrorStatus();
    }

    bool equals(const char*, int, int, float, int, bool, bool, bool, bool) override {
        return false;
    }

    void getCharRGBA(int ch, int x, int y, int wide, int tall, uchar* rgba) override {
        cse_vgui1::RequireReady(this);
        if (ch >= VFONT_GLYPH_COUNT) {
            UnicodeFallback()->getCharRGBA(ch, x, y, wide, tall, rgba);
            return;
        }
        if (ch < 0) {
            ch = 0;
        }
        if (ch >= VFONT_GLYPH_COUNT || x < 0 || y < 0 || wide < 0 || tall < 0 || !rgba
            || static_cast<std::uint64_t>(wide) * static_cast<std::uint64_t>(tall)
                > static_cast<std::uint64_t>((std::numeric_limits<std::ptrdiff_t>::max)()) / VGUI_RGBA_BYTES) {
            cse_vgui1::InvalidLegacyUse("invalid bitmap glyph/canvas");
        }
        for (int row = 0; row < data_.m_BitmapCharHeight; ++row) {
            for (int col = 0; col < data_.m_BitmapCharWidth; ++col) {
                const std::int64_t outX = static_cast<std::int64_t>(x) + col;
                const std::int64_t outY = static_cast<std::int64_t>(y) + row;
                if (outX >= wide || outY >= tall) {
                    continue;
                }
                const std::size_t source = (static_cast<std::size_t>(row) * VFONT_GLYPH_COUNT
                    + static_cast<std::size_t>(ch)) * static_cast<std::size_t>(data_.m_BitmapCharWidth)
                    + static_cast<std::size_t>(col);
                const uchar color = data_.m_pBitmap[source] ? 255 : 0;
                uchar* pixel = rgba + (static_cast<std::size_t>(outY) * static_cast<std::size_t>(wide)
                    + static_cast<std::size_t>(outX)) * VGUI_RGBA_BYTES;
                pixel[0] = color;
                pixel[1] = color;
                pixel[2] = color;
                pixel[3] = color;
            }
        }
    }

    void getCharABCwide(int ch, int& a, int& b, int& c) override {
        cse_vgui1::RequireReady(this);
        if (ch >= VFONT_GLYPH_COUNT) {
            UnicodeFallback()->getCharABCwide(ch, a, b, c);
            return;
        }
        if (ch < 0) {
            ch = 0;
        } else if (ch >= VFONT_GLYPH_COUNT) {
            ch = VFONT_GLYPH_COUNT - 1;
        }
        a = 0;
        b = data_.m_CharWidths[ch] + 1;
        c = 0;
    }

    int getTall() override {
        cse_vgui1::RequireReady(this);
        return data_.m_BitmapCharHeight;
    }

    int getWide() override {
        cse_vgui1::RequireReady(this);
        return data_.m_BitmapCharWidth;
    }

    void drawSetTextFont(SurfacePlat* surface) override {
        cse_vgui1::RequireReady(this);
        CseVgui1SelectFontForSurfaceV1(surface, this);
    }

private:
    BaseFontPlat* UnicodeFallback() {
        if (!unicodeFallback_) {
            // Original TGA byte glyphs stay intact. Missing Unicode cells use
            // the same requested family and bitmap cell height, realized only
            // when a validated UTF8 run actually needs a non-byte glyph.
            unicodeFallback_ = cse_vgui1::CreatePlatform(name_.c_str(), data_.m_BitmapCharHeight, 0, 0.0f,
                weight_, italic_, underline_, strikeout_, false);
        }
        cse_vgui1::RequireReady(unicodeFallback_);
        return unicodeFallback_;
    }

    VFontData data_;
    std::string name_;
    int weight_;
    bool italic_;
    bool underline_;
    bool strikeout_;
    BaseFontPlat* unicodeFallback_ = nullptr;
};
}

Font::Font(const char* name, int tall, int wide, float rotation, int weight,
    bool italic, bool underline, bool strikeout, bool symbol) {
    init(name, nullptr, 0, tall, wide, rotation, weight, italic, underline, strikeout, symbol);
}

Font::Font(const char* name, void* data, int length, int tall, int wide, float rotation, int weight,
    bool italic, bool underline, bool strikeout, bool symbol) {
    init(name, data, length, tall, wide, rotation, weight, italic, underline, strikeout, symbol);
}

void Font::init(const char* name, void* data, int length, int tall, int wide, float rotation, int weight,
    bool italic, bool underline, bool strikeout, bool symbol) {
    _name = cse_vgui1::OwnFontName(name);
    _plat = nullptr;
    _id = -1;
    if (data) {
        std::unique_ptr<BitmapFontPlat> bitmap(new BitmapFontPlat(name, weight, italic, underline, strikeout));
        if (!bitmap->Load(data, length)) {
            cse_vgui1::RecordBitmapFailure(name);
            return;
        }
        _plat = bitmap.release();
        cse_vgui1::AdoptBitmapPlatform(_plat, name);
    } else {
        _plat = cse_vgui1::FindCachedPlatform(name, tall, wide, rotation, weight,
            italic, underline, strikeout, symbol);
        if (_plat) {
            return; // Retained Windows cache-hit ID behavior.
        }
        _plat = cse_vgui1::CreatePlatform(name, tall, wide, rotation, weight,
            italic, underline, strikeout, symbol);
    }
    _id = cse_vgui1::NextFontId();
}

void Font::getCharRGBA(int ch, int x, int y, int wide, int tall, uchar* rgba) {
    cse_vgui1::RequireReady(_plat);
    _plat->getCharRGBA(ch, x, y, wide, tall, rgba);
}

void Font::getCharABCwide(int ch, int& a, int& b, int& c) {
    cse_vgui1::RequireReady(_plat);
    _plat->getCharABCwide(ch, a, b, c);
}

int Font::getTall() {
    cse_vgui1::RequireReady(_plat);
    return _plat->getTall();
}

int Font::getWide() {
    cse_vgui1::RequireReady(_plat);
    return _plat->getWide();
}

void Font_Reset() {
    cse_vgui1::ResetLookup();
}
}
