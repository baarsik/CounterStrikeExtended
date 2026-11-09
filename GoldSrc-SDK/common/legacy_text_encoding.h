#ifndef CSE_COMMON_LEGACY_TEXT_ENCODING_H
#define CSE_COMMON_LEGACY_TEXT_ENCODING_H

#include <climits>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace cse {
namespace legacy_text {
namespace detail {

static_assert(CHAR_BIT == 8, "Legacy text requires octets");
constexpr std::uint32_t Replacement = 0xfffd;
constexpr std::uint32_t UnicodeMaximum = 0x10ffff;
constexpr std::uint32_t FirstSupplementary = 0x10000;
constexpr std::uint32_t HighFirst = 0xd800;
constexpr std::uint32_t HighLast = 0xdbff;
constexpr std::uint32_t LowFirst = 0xdc00;
constexpr std::uint32_t LowLast = 0xdfff;
constexpr unsigned int SurrogateShift = 10;
constexpr std::uint32_t SurrogateMask = 0x3ff;
constexpr unsigned char ContinuationFirst = 0x80;
constexpr unsigned char ContinuationLast = 0xbf;
constexpr unsigned char TwoFirst = 0xc2;
constexpr unsigned char TwoLast = 0xdf;
constexpr unsigned char ThreeFirst = 0xe0;
constexpr unsigned char ThreeLast = 0xef;
constexpr unsigned char FourFirst = 0xf0;
constexpr unsigned char FourLast = 0xf4;
constexpr unsigned int ContinuationBits = 6;
constexpr std::uint32_t FirstTwoByteScalar = 0x80;
constexpr std::uint32_t FirstThreeByteScalar = 0x800;
constexpr unsigned char TwoPrefix = 0xc0;
constexpr unsigned char TwoPayloadMask = 0x1f;
constexpr unsigned char ThreePayloadMask = 0x0f;
constexpr unsigned char FourPayloadMask = 0x07;
constexpr unsigned char MinimumThreeSecond = 0xa0;
constexpr unsigned char SurrogateLead = 0xed;
constexpr unsigned char MinimumFourSecond = 0x90;
constexpr std::size_t MaximumUtf8Bytes = 4;
constexpr std::uint32_t ContinuationMask = 0x3f;

inline bool Continuation(unsigned char byte) noexcept {
    return byte >= ContinuationFirst && byte <= ContinuationLast;
}

struct Decoded {
    std::uint32_t scalar;
    std::size_t consumed;
};

inline Decoded DecodeUtf8(const unsigned char* input) noexcept {
    const unsigned char lead = input[0];
    if (lead < ContinuationFirst) {
        return {lead, 1};
    }
    unsigned int total = 0;
    std::uint32_t scalar = 0;
    if (lead >= TwoFirst && lead <= TwoLast) {
        total = 2;
        scalar = lead & TwoPayloadMask;
    } else if (lead >= ThreeFirst && lead <= ThreeLast) {
        total = 3;
        scalar = lead & ThreePayloadMask;
    } else if (lead >= FourFirst && lead <= FourLast) {
        total = 4;
        scalar = lead & FourPayloadMask;
    } else {
        return {Replacement, 1};
    }
    if (!Continuation(input[1])) {
        return {Replacement, 1};
    }
    const unsigned char second = input[1];
    // CP_UTF8/flags0 consumes this continuation before rejecting an overlong,
    // surrogate or out-of-range prefix. This differs from generic UTF8 repair.
    if ((lead == ThreeFirst && second < MinimumThreeSecond) || (lead == SurrogateLead && second >= MinimumThreeSecond) ||
        (lead == FourFirst && second < MinimumFourSecond) || (lead == FourLast && second >= MinimumFourSecond)) {
        return {Replacement, 2};
    }
    scalar = (scalar << ContinuationBits) | (second & ContinuationMask);
    for (unsigned int index = 2; index < total; ++index) {
        if (!Continuation(input[index])) {
            return {Replacement, index};
        }
        scalar = (scalar << ContinuationBits) | (input[index] & ContinuationMask);
    }
    return {scalar, total};
}

struct Encoded {
    unsigned char bytes[MaximumUtf8Bytes];
    std::size_t count;
};

inline Encoded EncodeUtf8(std::uint32_t scalar) noexcept {
    Encoded output{{}, 0};
    if (scalar < FirstTwoByteScalar) {
        output.bytes[0] = static_cast<unsigned char>(scalar);
        output.count = 1;
    } else if (scalar < FirstThreeByteScalar) {
        output.bytes[0] = static_cast<unsigned char>(TwoPrefix | (scalar >> ContinuationBits));
        output.bytes[1] = static_cast<unsigned char>(ContinuationFirst | (scalar & ContinuationMask));
        output.count = 2;
    } else if (scalar < FirstSupplementary) {
        output.bytes[0] = static_cast<unsigned char>(ThreeFirst | (scalar >> (ContinuationBits * 2)));
        output.bytes[1] = static_cast<unsigned char>(ContinuationFirst | ((scalar >> ContinuationBits) & ContinuationMask));
        output.bytes[2] = static_cast<unsigned char>(ContinuationFirst | (scalar & ContinuationMask));
        output.count = 3;
    } else {
        output.bytes[0] = static_cast<unsigned char>(FourFirst | (scalar >> (ContinuationBits * 3)));
        output.bytes[1] = static_cast<unsigned char>(ContinuationFirst | ((scalar >> (ContinuationBits * 2)) & ContinuationMask));
        output.bytes[2] = static_cast<unsigned char>(ContinuationFirst | ((scalar >> ContinuationBits) & ContinuationMask));
        output.bytes[3] = static_cast<unsigned char>(ContinuationFirst | (scalar & ContinuationMask));
        output.count = 4;
    }
    return output;
}

template<typename Cell>
inline Decoded DecodeWide(const Cell* input) noexcept {
    using UnsignedCell = typename std::make_unsigned<Cell>::type;
    const std::uint32_t first = static_cast<std::uint32_t>(static_cast<UnsignedCell>(input[0]));
    if (first >= HighFirst && first <= HighLast) {
        const std::uint32_t second = static_cast<std::uint32_t>(static_cast<UnsignedCell>(input[1]));
        if (second >= LowFirst && second <= LowLast) {
            return {FirstSupplementary + ((first - HighFirst) << SurrogateShift) + (second - LowFirst), 2};
        }
        return {Replacement, 1};
    }
    if ((first >= LowFirst && first <= LowLast) || first > UnicodeMaximum) {
        return {Replacement, 1};
    }
    return {first, 1};
}

} // namespace detail

// Output cells always contain UTF16 code units, including on wchar_t32 hosts.
// Capacities are physical destination BYTES; successful counts include NUL.
// Null destinations and capacities smaller than one cell safely return 0/no write.
// Otherwise the last cell is always NUL, preserving the retained Win wrapper.
template<typename Cell>
inline int Utf8ToWideCells(const char* source, Cell* destination, int destinationBytes) noexcept {
    static_assert(std::is_integral<Cell>::value && (sizeof(Cell) == 2 || sizeof(Cell) == 4),
        "Wide text cells must be 16-bit or 32-bit integers");
    if (!destination || destinationBytes <= 0) {
        return 0;
    }
    const std::size_t capacity = static_cast<std::size_t>(destinationBytes) / sizeof(Cell);
    if (capacity == 0) {
        return 0;
    }
    std::size_t written = 0;
    int result = 0;
    if (source) {
        const unsigned char* input = reinterpret_cast<const unsigned char*>(source);
        while (true) {
            const detail::Decoded decoded = detail::DecodeUtf8(input);
            const std::size_t cells = decoded.scalar >= detail::FirstSupplementary ? 2 : 1;
            if (cells > capacity - written) {
                break;
            }
            if (cells == 2) {
                const std::uint32_t value = decoded.scalar - detail::FirstSupplementary;
                destination[written++] = static_cast<Cell>(detail::HighFirst + (value >> detail::SurrogateShift));
                destination[written++] = static_cast<Cell>(detail::LowFirst + (value & detail::SurrogateMask));
            } else {
                destination[written++] = static_cast<Cell>(decoded.scalar);
            }
            if (decoded.scalar == 0) {
                result = static_cast<int>(written);
                break;
            }
            input += decoded.consumed;
        }
    }
    destination[capacity - 1] = 0;
    return result;
}

// Surrogate-unit pairs combine. Valid native32 scalars are also accepted; negative
// or >10FFFF cells and unpaired surrogates become U+FFFD, matching flags0 repair.
// clearFirst is the extra preclear performed ONLY by Q_UCS2ToUTF8.
template<typename Cell>
inline int WideCellsToUtf8(const Cell* source, char* destination, int destinationBytes,
    bool clearFirst = false) noexcept {
    static_assert(std::is_integral<Cell>::value && (sizeof(Cell) == 2 || sizeof(Cell) == 4),
        "Wide text cells must be 16-bit or 32-bit integers");
    if (!destination || destinationBytes <= 0) {
        return 0;
    }
    const std::size_t capacity = static_cast<std::size_t>(destinationBytes);
    if (clearFirst) {
        destination[0] = 0;
    }
    std::size_t written = 0;
    int result = 0;
    if (source) {
        unsigned char* output = reinterpret_cast<unsigned char*>(destination);
        while (true) {
            const detail::Decoded decoded = detail::DecodeWide(source);
            const detail::Encoded encoded = detail::EncodeUtf8(decoded.scalar);
            if (encoded.count > capacity - written) {
                break;
            }
            for (std::size_t byte = 0; byte < encoded.count; ++byte) {
                output[written++] = encoded.bytes[byte];
            }
            if (decoded.scalar == 0) {
                result = static_cast<int>(written);
                break;
            }
            source += decoded.consumed;
        }
    }
    destination[capacity - 1] = 0;
    return result;
}

} // namespace legacy_text
} // namespace cse

#endif
