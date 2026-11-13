#ifndef CSE_VGUI1_TEXT_ENCODING_H
#define CSE_VGUI1_TEXT_ENCODING_H

#include <cstddef>
#include <cstdint>

namespace cse_vgui1 {
namespace text {
struct Character {
    int value;
    std::size_t bytes;
};

// A decoded Unicode Latin-1 character and an ANSI byte share the same numeric
// value but can name different glyphs on a Windows CP1250/1251 desktop. This
// private out-of-Unicode-range tag preserves the distinction through existing
// int metric/raster methods without changing any public VGUI1 ABI slot.
constexpr int UnicodeByteGlyphBase = 0x110000;
constexpr int FirstExtendedByte = 0x80;
constexpr int MaximumByte = 0xff;

inline bool IsUnicodeByteGlyph(int glyph) {
    return glyph >= UnicodeByteGlyphBase + FirstExtendedByte && glyph <= UnicodeByteGlyphBase + MaximumByte;
}

inline int UnicodeValue(int glyph) {
    return IsUnicodeByteGlyph(glyph) ? glyph - UnicodeByteGlyphBase : glyph;
}

inline Character DecodeUtf8(const char* text, std::size_t length) {
    const auto lead = static_cast<unsigned char>(text[0]);
    constexpr unsigned int FirstContinuation = 0x80;
    constexpr unsigned int LastContinuation = 0xbf;
    constexpr unsigned int ContinuationMask = 0x3f;
    constexpr unsigned int ContinuationBits = 6;
    constexpr unsigned int FirstTwoByteLead = 0xc2;
    constexpr unsigned int LastTwoByteLead = 0xdf;
    constexpr unsigned int FirstThreeByteLead = 0xe0;
    constexpr unsigned int LastThreeByteLead = 0xef;
    constexpr unsigned int FirstFourByteLead = 0xf0;
    constexpr unsigned int LastFourByteLead = 0xf4;
    constexpr unsigned int TwoBytePayloadMask = 0x1f;
    constexpr unsigned int ThreeBytePayloadMask = 0x0f;
    constexpr unsigned int FourBytePayloadMask = 0x07;
    constexpr std::size_t TwoByteLength = 2;
    constexpr std::size_t ThreeByteLength = 3;
    constexpr std::size_t FourByteLength = 4;
    constexpr std::uint32_t FirstTwoByteScalar = 0x80;
    constexpr std::uint32_t FirstThreeByteScalar = 0x800;
    constexpr std::uint32_t FirstFourByteScalar = 0x10000;
    constexpr std::uint32_t InvalidScalar = 0x110000;
    if (lead < FirstContinuation) {
        return {lead, 1};
    }
    std::size_t bytes = 0;
    std::uint32_t value = 0;
    std::uint32_t minimum = 0;
    if (lead >= FirstTwoByteLead && lead <= LastTwoByteLead) {
        bytes = TwoByteLength;
        value = lead & TwoBytePayloadMask;
        minimum = FirstTwoByteScalar;
    } else if (lead >= FirstThreeByteLead && lead <= LastThreeByteLead) {
        bytes = ThreeByteLength;
        value = lead & ThreeBytePayloadMask;
        minimum = FirstThreeByteScalar;
    } else if (lead >= FirstFourByteLead && lead <= LastFourByteLead) {
        bytes = FourByteLength;
        value = lead & FourBytePayloadMask;
        minimum = FirstFourByteScalar;
    }
    if (bytes == 0 || length < bytes) {
        return {InvalidScalar, 1};
    }
    for (std::size_t index = 1; index < bytes; ++index) {
        const auto next = static_cast<unsigned char>(text[index]);
        if (next < FirstContinuation || next > LastContinuation) {
            return {InvalidScalar, 1};
        }
        value = (value << ContinuationBits) | (next & ContinuationMask);
    }
    constexpr std::uint32_t FirstSurrogate = 0xd800;
    constexpr std::uint32_t LastSurrogate = 0xdfff;
    if (value < minimum || value >= InvalidScalar || (value >= FirstSurrogate && value <= LastSurrogate)) {
        return {InvalidScalar, 1};
    }
    return {static_cast<int>(value), bytes};
}

// Encoding is selected for the entire run, before iteration. A malformed UTF8
// string remains legacy bytes, including otherwise valid subsequences in it.
class Run {
public:
    Run(const char* text, std::size_t length) : text_(text), length_(length) {
        for (std::size_t offset = 0; offset < length_;) {
            const auto character = DecodeUtf8(text_ + offset, length_ - offset);
            constexpr int UnicodeMaximum = 0x10ffff;
            if (character.value > UnicodeMaximum) {
                utf8_ = false;
                break;
            }
            offset += character.bytes;
        }
    }

    Character At(std::size_t offset) const {
        if (utf8_) {
            auto character = DecodeUtf8(text_ + offset, length_ - offset);
            constexpr int BmpMaximum = 0xffff;
            constexpr int ReplacementCharacter = 0xfffd;
            // Existing glyph interfaces on Windows carry a single UTF16 cell.
            // Keep complete UTF8 byte consumption for supplementary characters
            // while displaying one explicit replacement glyph.
            if (character.value > BmpMaximum) {
                character.value = ReplacementCharacter;
            }
            return character;
        }
        return {static_cast<signed char>(text_[offset]), 1};
    }

    Character GlyphAt(std::size_t offset) const {
        auto character = At(offset);
        if (utf8_ && character.value >= FirstExtendedByte && character.value <= MaximumByte) {
            character.value += UnicodeByteGlyphBase;
        }
        return character;
    }

    bool IsUtf8() const {
        return utf8_;
    }

private:
    const char* text_;
    std::size_t length_;
    bool utf8_ = true;
};
}
}

#endif
