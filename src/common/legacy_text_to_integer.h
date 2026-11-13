#ifndef CSE_LEGACY_TEXT_TO_INTEGER_H
#define CSE_LEGACY_TEXT_TO_INTEGER_H

#include <cstdint>
#include <limits>
#include <type_traits>

// Explicit byte profiles measured against the retained Win32 UCRT. The native
// Steam-ID callers use C/UTF8 decimal text; this does not consult the host locale.
enum class CseLegacyIntegerByteWhitespace {
    COrUtf8,
    SingleByteNbsp
};

namespace cse_legacy_text_detail {
inline std::uint32_t Unit(char value) noexcept {
    return static_cast<unsigned char>(value);
}

inline std::uint32_t Unit(wchar_t value) noexcept {
    using UnsignedWide = typename std::make_unsigned<wchar_t>::type;
    static_assert(std::numeric_limits<UnsignedWide>::digits <= 32, "Native wchar_t width");
    return static_cast<std::uint32_t>(static_cast<UnsignedWide>(value));
}

struct ByteClassifier {
    CseLegacyIntegerByteWhitespace whitespace;

    bool IsSpace(std::uint32_t unit) const noexcept {
        constexpr std::uint32_t kFirstAsciiSpace = 0x09;
        constexpr std::uint32_t kLastAsciiSpace = 0x0d;
        constexpr std::uint32_t kNoBreakSpaceByte = 0xa0;
        return (unit >= kFirstAsciiSpace && unit <= kLastAsciiSpace) || unit == ' '
            || (whitespace == CseLegacyIntegerByteWhitespace::SingleByteNbsp && unit == kNoBreakSpaceByte);
    }

    int Digit(std::uint32_t unit) const noexcept {
        constexpr int kInvalidDigit = -1;
        return unit >= '0' && unit <= '9' ? static_cast<int>(unit - '0') : kInvalidDigit;
    }
};

struct WideClassifier {
    bool IsSpace(std::uint32_t unit) const noexcept {
        // These are UTF16 code units, including the retained U+180E classification.
        constexpr std::uint32_t kFirstAsciiSpace = 0x09;
        constexpr std::uint32_t kLastAsciiSpace = 0x0d;
        constexpr std::uint32_t kFirstGeneralPunctuationSpace = 0x2000;
        constexpr std::uint32_t kLastGeneralPunctuationSpace = 0x200a;
        constexpr std::uint32_t kSingleSpaces[]{
            0x20, 0x85, 0xa0, 0x1680, 0x180e, 0x2028, 0x2029, 0x202f, 0x205f, 0x3000
        };
        if ((unit >= kFirstAsciiSpace && unit <= kLastAsciiSpace)
            || (unit >= kFirstGeneralPunctuationSpace && unit <= kLastGeneralPunctuationSpace)) {
            return true;
        }
        for (const std::uint32_t space : kSingleSpaces) {
            if (unit == space) {
                return true;
            }
        }
        return false;
    }

    int Digit(std::uint32_t unit) const noexcept {
        // The measured UCRT table has these 18 BMP decimal blocks. Newer Unicode
        // blocks and non-BMP native wchar_t values must not alias a UTF16 digit.
        constexpr std::uint32_t kDecimalRadix = 10;
        constexpr int kInvalidDigit = -1;
        constexpr std::uint32_t kDigitZeroUnits[]{
            0x0030, 0x0660, 0x06f0, 0x0966, 0x09e6, 0x0a66, 0x0ae6, 0x0b66, 0x0c66,
            0x0ce6, 0x0d66, 0x0e50, 0x0ed0, 0x0f20, 0x1040, 0x17e0, 0x1810, 0xff10
        };
        for (const std::uint32_t zero : kDigitZeroUnits) {
            if (unit >= zero && unit - zero < kDecimalRadix) {
                return static_cast<int>(unit - zero);
            }
        }
        return kInvalidDigit;
    }
};

template<typename Signed, typename Character, typename Classifier>
inline Signed Parse(const Character* text, Classifier classifier) noexcept {
    static_assert(std::numeric_limits<Signed>::is_signed, "Signed conversion result");
    using Unsigned = typename std::make_unsigned<Signed>::type;
    constexpr Unsigned kDecimalRadix = 10;
    constexpr Signed kMinimum = (std::numeric_limits<Signed>::min)();
    constexpr Signed kMaximum = (std::numeric_limits<Signed>::max)();
    const Character* position = text;
    while (classifier.IsSpace(Unit(*position))) {
        ++position;
    }
    const bool negative = Unit(*position) == '-';
    if (negative || Unit(*position) == '+') {
        ++position;
    }
    const Unsigned limit = static_cast<Unsigned>(kMaximum) + static_cast<Unsigned>(negative);
    const Unsigned cutoff = limit / kDecimalRadix;
    const Unsigned lastDigit = limit % kDecimalRadix;
    Unsigned magnitude = 0;
    for (;;) {
        const int digit = classifier.Digit(Unit(*position));
        if (digit < 0) {
            break;
        }
        const Unsigned nextDigit = static_cast<Unsigned>(digit);
        if (magnitude > cutoff || (magnitude == cutoff && nextDigit > lastDigit)) {
            return negative ? kMinimum : kMaximum;
        }
        magnitude = magnitude * kDecimalRadix + nextDigit;
        ++position;
    }
    if (negative && magnitude == limit) {
        return kMinimum;
    }
    const Signed value = static_cast<Signed>(magnitude);
    return negative ? -value : value;
}
}

// Value-only conversions for non-null, NUL-terminated input. They deliberately
// preserve errno and do not reproduce CRT invalid-parameter or locale side effects.
inline std::int32_t CseLegacyAtoi(const char* text,
    CseLegacyIntegerByteWhitespace whitespace = CseLegacyIntegerByteWhitespace::COrUtf8) noexcept {
    return cse_legacy_text_detail::Parse<std::int32_t>(text, cse_legacy_text_detail::ByteClassifier{whitespace});
}

inline std::int64_t CseLegacyAtoi64(const char* text,
    CseLegacyIntegerByteWhitespace whitespace = CseLegacyIntegerByteWhitespace::COrUtf8) noexcept {
    return cse_legacy_text_detail::Parse<std::int64_t>(text, cse_legacy_text_detail::ByteClassifier{whitespace});
}

// Native wchar_t may be wider than Windows UTF16. Values above the code-unit
// domain (including negative wchar_t values) stop parsing; no narrowing is used.
inline std::int32_t CseLegacyWtoi(const wchar_t* text) noexcept {
    return cse_legacy_text_detail::Parse<std::int32_t>(text, cse_legacy_text_detail::WideClassifier{});
}

#endif
