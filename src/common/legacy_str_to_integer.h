#ifndef CSE_LEGACY_STR_TO_INTEGER_H
#define CSE_LEGACY_STR_TO_INTEGER_H

#include "legacy_text_to_integer.h"
#include <cerrno>
#include <cstdint>
#include <limits>

namespace cse_legacy_str_detail {
struct DecimalResult {
    std::uint32_t magnitude;
    const char* end;
    bool negative;
    bool overflow;
};

inline DecimalResult Parse(const char* text, std::uint32_t positiveLimit, std::uint32_t negativeLimit,
    CseLegacyIntegerByteWhitespace whitespace) noexcept {
    const cse_legacy_text_detail::ByteClassifier classifier{whitespace};
    const char* position = text;
    while (classifier.IsSpace(cse_legacy_text_detail::Unit(*position))) {
        ++position;
    }
    const bool negative = *position == '-';
    if (negative || *position == '+') {
        ++position;
    }
    const char* firstDigit = position;
    const std::uint32_t limit = negative ? negativeLimit : positiveLimit;
    constexpr std::uint32_t kDecimalRadix = 10;
    const std::uint32_t cutoff = limit / kDecimalRadix;
    const std::uint32_t finalDigit = limit % kDecimalRadix;
    std::uint32_t magnitude = 0;
    bool overflow = false;
    for (;;) {
        const int digit = classifier.Digit(cse_legacy_text_detail::Unit(*position));
        if (digit < 0) {
            break;
        }
        const auto nextDigit = static_cast<std::uint32_t>(digit);
        if (!overflow) {
            overflow = magnitude > cutoff || (magnitude == cutoff && nextDigit > finalDigit);
            if (!overflow) {
                magnitude = magnitude * kDecimalRadix + nextDigit;
            }
        }
        // UCRT still consumes the complete decimal run after magnitude overflow.
        ++position;
    }
    return {magnitude, position == firstDigit ? text : position, negative, overflow};
}

inline void SetEnd(char** end, const char* position) noexcept {
    if (end) {
        // Match the CRT's borrowed endpoint type; this function never writes input.
        *end = const_cast<char*>(position);
    }
}
}

// Decimal-only retained UCRT contract for non-null, NUL-terminated byte input.
// Success and no conversion preserve errno; range failure consumes all digits.
// No locale, base inference, wide conversion or invalid-parameter API is implied.
inline std::int32_t CseLegacyStrtol10(const char* text, char** end,
    CseLegacyIntegerByteWhitespace whitespace = CseLegacyIntegerByteWhitespace::COrUtf8) noexcept {
    constexpr std::int32_t kMinimum = (std::numeric_limits<std::int32_t>::min)();
    constexpr std::int32_t kMaximum = (std::numeric_limits<std::int32_t>::max)();
    constexpr auto kPositiveLimit = static_cast<std::uint32_t>(kMaximum);
    constexpr std::uint32_t kNegativeLimit = kPositiveLimit + 1;
    const auto result = cse_legacy_str_detail::Parse(text, kPositiveLimit, kNegativeLimit, whitespace);
    cse_legacy_str_detail::SetEnd(end, result.end);
    if (result.overflow) {
        errno = ERANGE;
        return result.negative ? kMinimum : kMaximum;
    }
    if (result.negative && result.magnitude == kNegativeLimit) {
        return kMinimum;
    }
    const auto value = static_cast<std::int32_t>(result.magnitude);
    return result.negative ? -value : value;
}

inline std::uint32_t CseLegacyStrtoul10(const char* text, char** end,
    CseLegacyIntegerByteWhitespace whitespace = CseLegacyIntegerByteWhitespace::COrUtf8) noexcept {
    constexpr std::uint32_t kMaximum = (std::numeric_limits<std::uint32_t>::max)();
    const auto result = cse_legacy_str_detail::Parse(text, kMaximum, kMaximum, whitespace);
    cse_legacy_str_detail::SetEnd(end, result.end);
    if (result.overflow) {
        errno = ERANGE;
        return kMaximum;
    }
    return result.negative ? std::uint32_t{0} - result.magnitude : result.magnitude;
}

#endif
