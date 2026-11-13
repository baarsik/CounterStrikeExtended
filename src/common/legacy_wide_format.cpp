#include "legacy_wide_format.h"

#include <cerrno>
#include <cfenv>
#include <climits>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

namespace {
static_assert(CHAR_BIT == 8 && sizeof(int) == 4 && sizeof(short) == 2
    && (sizeof(wchar_t) == 2 || sizeof(wchar_t) == 4), "Selected legacy text ABI widths");
constexpr std::uint32_t kAsciiLimit = 0x80;
constexpr unsigned char kContinuationLast = 0xbf;
constexpr unsigned char kTwoByteFirst = 0xc2;
constexpr unsigned char kTwoByteLast = 0xdf;
constexpr unsigned char kThreeByteFirst = 0xe0;
constexpr unsigned char kThreeByteLast = 0xef;
constexpr unsigned char kFourByteFirst = 0xf0;
constexpr unsigned char kFourByteLast = 0xf4;
constexpr unsigned int kContinuationBits = 6;
constexpr unsigned char kContinuationMask = 0x3f;
constexpr unsigned char kTwoBytePayload = 0x1f;
constexpr unsigned char kThreeBytePayload = 0x0f;
constexpr unsigned char kFourBytePayload = 0x07;
constexpr std::uint32_t kMinimumThreeByteScalar = 0x800;
constexpr std::uint32_t kLastSurrogate = 0xdfff;
constexpr unsigned int kDecimalRadix = 10;
constexpr unsigned int kHexDigitBits = 4;
constexpr std::uint64_t kHexDigitMask = 0xf;
constexpr std::size_t kMinimumScientificExponentDigits = 2;
constexpr std::size_t kMaximumExponentDigits = 4; // Largest binary64 exponent magnitude is 1074.
constexpr std::uint32_t kFirstSupplementary = 0x10000;
constexpr std::uint32_t kUnicodeMaximum = 0x10ffff;
constexpr std::uint32_t kHighSurrogate = 0xd800;
constexpr std::uint32_t kLowSurrogate = 0xdc00;
constexpr unsigned int kSurrogateShift = 10;
constexpr std::uint32_t kSurrogateMask = 0x3ff;
constexpr int kUnspecifiedPrecision = -1;

struct Sink {
    wchar_t* destination;
    std::size_t capacity;
    std::size_t size = 0;
    int error = 0;
    bool truncated = false;

    bool Put(std::uint32_t unit) noexcept {
        if (size == static_cast<std::size_t>(INT_MAX)) {
            error = EOVERFLOW;
            return false;
        }
        if (destination) {
            if (size == capacity) {
                truncated = true;
                return false;
            }
            destination[size] = static_cast<wchar_t>(unit);
        }
        ++size;
        return true;
    }

    bool Repeat(wchar_t unit, std::size_t count) noexcept {
        if (!destination) {
            if (count > static_cast<std::size_t>(INT_MAX) - size) {
                error = EOVERFLOW;
                return false;
            }
            size += count;
            return true;
        }
        while (count > 0) {
            if (!Put(static_cast<std::uint32_t>(unit))) {
                return false;
            }
            --count;
        }
        return true;
    }

    void Terminate() noexcept {
        if (destination && size < capacity) {
            destination[size] = 0;
        }
    }
};

bool EmitScalar(Sink& sink, std::uint32_t unit) noexcept {
    if (unit < kFirstSupplementary) {
        return sink.Put(unit);
    }
    if (unit > kUnicodeMaximum) {
        sink.error = EILSEQ;
        return false;
    }
    const auto scalar = unit - kFirstSupplementary;
    return sink.Put(kHighSurrogate + (scalar >> kSurrogateShift))
        && sink.Put(kLowSurrogate + (scalar & kSurrogateMask));
}

enum class Length {
    Normal, Char, Short, Long, LongLong, I32, Pointer, IntMax, Size, Difference, Wide, LongDouble
};
struct Field {
    bool left = false;
    bool plus = false;
    bool space = false;
    bool alternate = false;
    bool zero = false;
    std::size_t width = 0;
    int precision = kUnspecifiedPrecision;
    Length length = Length::Normal;
    wchar_t conversion = 0;
};

bool Decimal(wchar_t unit) noexcept {
    return unit >= L'0' && unit <= L'9';
}

bool Number(const wchar_t*& position, std::size_t& value) noexcept {
    constexpr std::size_t kRadix = 10;
    value = 0;
    while (Decimal(*position)) {
        const auto digit = static_cast<std::size_t>(*position - L'0');
        if (value > (static_cast<std::size_t>(INT_MAX) - digit) / kRadix) {
            return false;
        }
        value = value * kRadix + digit;
        ++position;
    }
    return true;
}

bool Parse(const wchar_t*& position, Field& field, va_list& arguments) noexcept {
    bool flags = true;
    while (flags) {
        switch (*position) {
        case L'-':
            field.left = true;
            break;
        case L'+':
            field.plus = true;
            break;
        case L' ':
            field.space = true;
            break;
        case L'#':
            field.alternate = true;
            break;
        case L'0':
            field.zero = true;
            break;
        default:
            flags = false;
            continue;
        }
        ++position;
    }
    if (*position == L'*') {
        ++position;
        const int width = va_arg(arguments, int);
        field.left = width < 0 || field.left;
        const auto bits = static_cast<unsigned int>(width);
        field.width = width < 0 ? 0U - bits : bits;
    } else if (!Number(position, field.width)) {
        return false;
    }
    if (*position == L'.') {
        ++position;
        if (*position == L'*') {
            ++position;
            const int precision = va_arg(arguments, int);
            field.precision = precision < 0 ? kUnspecifiedPrecision : precision;
        } else {
            std::size_t precision;
            if (!Number(position, precision)) {
                return false;
            }
            field.precision = static_cast<int>(precision);
        }
    }
    switch (*position) {
    case L'h':
        ++position;
        field.length = Length::Short;
        if (*position == L'h') {
            ++position;
            field.length = Length::Char;
        }
        break;
    case L'l':
        ++position;
        field.length = Length::Long;
        if (*position == L'l') {
            ++position;
            field.length = Length::LongLong;
        }
        break;
    case L'I':
        ++position;
        field.length = Length::Pointer;
        if (position[0] == L'6' && position[1] == L'4') {
            position += 2;
            field.length = Length::LongLong;
        } else if (position[0] == L'3' && position[1] == L'2') {
            position += 2;
            field.length = Length::I32;
        }
        break;
    case L'j':
        ++position;
        field.length = Length::IntMax;
        break;
    case L'z':
        ++position;
        field.length = Length::Size;
        break;
    case L't':
        ++position;
        field.length = Length::Difference;
        break;
    case L'w':
        ++position;
        field.length = Length::Wide;
        break;
    case L'L':
        ++position;
        field.length = Length::LongDouble;
        break;
    default:
        break;
    }
    field.conversion = *position;
    if (*position) {
        ++position;
    }
    return true;
}

bool Utf8(const unsigned char*& position, std::uint32_t& scalar) noexcept {
    const unsigned char lead = *position++;
    if (lead < kAsciiLimit) {
        scalar = lead;
        return true;
    }
    unsigned int length;
    std::uint32_t minimum;
    if (lead >= kTwoByteFirst && lead <= kTwoByteLast) {
        length = 2;
        scalar = lead & kTwoBytePayload;
        minimum = kAsciiLimit;
    } else if (lead >= kThreeByteFirst && lead <= kThreeByteLast) {
        length = 3;
        scalar = lead & kThreeBytePayload;
        minimum = kMinimumThreeByteScalar;
    } else if (lead >= kFourByteFirst && lead <= kFourByteLast) {
        length = 4;
        scalar = lead & kFourBytePayload;
        minimum = kFirstSupplementary;
    } else {
        return false;
    }
    for (unsigned int index = 1; index < length; ++index) {
        const unsigned char next = *position;
        if (next < kAsciiLimit || next > kContinuationLast) {
            return false;
        }
        scalar = (scalar << kContinuationBits) | (next & kContinuationMask);
        ++position;
    }
    return scalar >= minimum && scalar <= kUnicodeMaximum
        && (scalar < kHighSurrogate || scalar > kLastSurrogate);
}

struct TextReader {
    const wchar_t* wide = nullptr;
    const unsigned char* narrow = nullptr;
    CseLegacyWideNarrowEncoding encoding = CseLegacyWideNarrowEncoding::StrictUtf8;
    int precision = kUnspecifiedPrecision;
    std::size_t size = 0;
    std::uint32_t pendingLow = 0;
    int error = 0;

    bool Next(std::uint32_t& unit) noexcept {
        if (pendingLow) {
            if (wide && precision >= 0 && size >= static_cast<std::size_t>(precision)) {
                return false;
            }
            unit = pendingLow;
            pendingLow = 0;
            ++size;
            return true;
        }
        if (precision >= 0 && size >= static_cast<std::size_t>(precision)) {
            return false;
        }
        std::uint32_t scalar;
        if (wide) {
            if (!*wide) {
                return false;
            }
            scalar = static_cast<std::uint32_t>(*wide++);
            if (scalar > kUnicodeMaximum) {
                error = EILSEQ;
                return false;
            }
        } else {
            if (!*narrow) {
                return false;
            }
            if (encoding == CseLegacyWideNarrowEncoding::CByteIdentity) {
                scalar = *narrow++;
            } else if (!Utf8(narrow, scalar)) {
                error = EILSEQ;
                return false;
            }
        }
        if (scalar >= kFirstSupplementary) {
            scalar -= kFirstSupplementary;
            unit = kHighSurrogate + (scalar >> kSurrogateShift);
            pendingLow = kLowSurrogate + (scalar & kSurrogateMask);
        } else {
            unit = scalar;
        }
        ++size;
        return true;
    }
};

bool String(Sink& sink, const Field& field, CseLegacyWideFormatContext context, va_list& arguments) noexcept {
    const bool wide = field.length == Length::Long || field.length == Length::Wide
        || (field.length != Length::Short && field.length != Length::Char && field.conversion == L's');
    TextReader initial;
    initial.encoding = context.narrow;
    initial.precision = field.precision;
    if (wide) {
        initial.wide = va_arg(arguments, const wchar_t*);
        if (!initial.wide) {
            initial.wide = L"(null)";
        }
    } else {
        const char* text = va_arg(arguments, const char*);
        if (!text) {
            text = "(null)";
        }
        initial.narrow = reinterpret_cast<const unsigned char*>(text);
    }
    TextReader measured = initial;
    std::uint32_t unit;
    while (measured.Next(unit)) {}
    if (measured.error) {
        sink.error = measured.error;
        return false;
    }
    const std::size_t padding = field.width > measured.size ? field.width - measured.size : 0;
    if (!field.left && !sink.Repeat(L' ', padding)) {
        return false;
    }
    TextReader emitted = initial;
    while (emitted.Next(unit)) {
        if (!sink.Put(unit)) {
            return false;
        }
    }
    return !field.left || sink.Repeat(L' ', padding);
}

bool Character(Sink& sink, const Field& field, CseLegacyWideFormatContext context, va_list& arguments) noexcept {
    const bool wide = field.length == Length::Long || field.length == Length::Wide
        || (field.length != Length::Short && field.conversion == L'c');
    const int value = va_arg(arguments, int);
    std::uint32_t unit;
    if (wide) {
        constexpr unsigned int kCellMask = 0xffff;
        unit = static_cast<unsigned int>(value) & kCellMask;
    } else {
        constexpr unsigned int kByteMask = 0xff;
        unit = static_cast<unsigned int>(value) & kByteMask;
        if (unit >= kAsciiLimit && context.narrow == CseLegacyWideNarrowEncoding::StrictUtf8) {
            // UCRT skips an invalid narrow character and continues formatting.
            sink.error = EILSEQ;
            return true;
        }
    }
    const std::size_t padding = field.width > 0 ? field.width - 1 : 0;
    if (!field.left && !sink.Repeat(L' ', padding)) {
        return false;
    }
    return sink.Put(unit) && (!field.left || sink.Repeat(L' ', padding));
}

std::uint64_t Unsigned(const Field& field, va_list& arguments) noexcept {
    using UnsignedDifference = typename std::make_unsigned<std::ptrdiff_t>::type;
    switch (field.length) {
    case Length::Char:
        return static_cast<unsigned char>(va_arg(arguments, int));
    case Length::Short:
        return static_cast<unsigned short>(va_arg(arguments, int));
    case Length::Long:
        return static_cast<std::uint32_t>(va_arg(arguments, unsigned long));
    case Length::LongLong:
        return va_arg(arguments, unsigned long long);
    case Length::IntMax:
        return va_arg(arguments, std::uintmax_t);
    case Length::Size:
        return va_arg(arguments, std::size_t);
    case Length::Difference:
        return va_arg(arguments, UnsignedDifference);
    case Length::Pointer:
        return va_arg(arguments, std::uintptr_t);
    default:
        return va_arg(arguments, unsigned int);
    }
}

template<typename SignedWord, typename UnsignedWord>
std::int64_t SignedLowWord(int input) noexcept {
    const auto bits = static_cast<UnsignedWord>(static_cast<unsigned int>(input));
    SignedWord value;
    static_assert(sizeof(value) == sizeof(bits), "Legacy signed word width");
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::int64_t Signed(const Field& field, va_list& arguments) noexcept {
    using SignedSize = typename std::make_signed<std::size_t>::type;
    switch (field.length) {
    case Length::Char:
        return SignedLowWord<std::int8_t, std::uint8_t>(va_arg(arguments, int));
    case Length::Short:
        return SignedLowWord<std::int16_t, std::uint16_t>(va_arg(arguments, int));
    case Length::Long: {
        const auto bits = static_cast<std::uint32_t>(va_arg(arguments, long));
        std::int32_t value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    case Length::LongLong:
        return va_arg(arguments, long long);
    case Length::IntMax:
        return va_arg(arguments, std::intmax_t);
    case Length::Size:
        return va_arg(arguments, SignedSize);
    case Length::Difference:
        return va_arg(arguments, std::ptrdiff_t);
    case Length::Pointer:
        return va_arg(arguments, std::intptr_t);
    default:
        return va_arg(arguments, int);
    }
}

bool Integer(Sink& sink, const Field& field, va_list& arguments) noexcept {
    const bool pointer = field.conversion == L'p';
    const bool signedValue = field.conversion == L'd' || field.conversion == L'i';
    const unsigned int radix = field.conversion == L'o' ? 8U
        : (field.conversion == L'x' || field.conversion == L'X' || pointer ? 16U : 10U);
    bool negative = false;
    std::uint64_t value;
    if (pointer) {
        value = reinterpret_cast<std::uintptr_t>(va_arg(arguments, void*));
    } else if (signedValue) {
        const std::int64_t input = Signed(field, arguments);
        negative = input < 0;
        const auto bits = static_cast<std::uint64_t>(input);
        value = negative ? std::uint64_t{0} - bits : bits;
    } else {
        value = Unsigned(field, arguments);
    }
    const bool nonzero = value != 0;
    constexpr std::size_t kMaximumDigits = 64;
    wchar_t reversed[kMaximumDigits]{};
    std::size_t digits = 0;
    const wchar_t* alphabet = field.conversion == L'X' || pointer ? L"0123456789ABCDEF" : L"0123456789abcdef";
    do {
        reversed[digits++] = alphabet[value % radix];
        value /= radix;
    } while (value);
    if (!nonzero && field.precision == 0 && !pointer) {
        digits = 0;
    }
    std::size_t precision = field.precision < 0 ? 0 : static_cast<std::size_t>(field.precision);
    if (pointer) {
        precision = sizeof(void*) * 2;
    }
    if (field.alternate && radix == 8 && !pointer && precision <= digits
        && (digits == 0 || reversed[digits - 1] != L'0')) {
        precision = digits + 1;
    }
    const wchar_t sign = negative ? L'-' : (signedValue && field.plus ? L'+' : (signedValue && field.space ? L' ' : 0));
    const bool prefix = field.alternate && nonzero && radix == 16 && !pointer;
    const std::size_t zeros = precision > digits ? precision - digits : 0;
    const std::size_t core = digits + zeros + static_cast<std::size_t>(sign != 0) + (prefix ? 2U : 0U);
    const std::size_t padding = field.width > core ? field.width - core : 0;
    const bool zeroPadding = field.zero && !field.left && field.precision < 0 && !pointer;
    if (!field.left && !zeroPadding && !sink.Repeat(L' ', padding)) {
        return false;
    }
    if (sign && !sink.Put(static_cast<std::uint32_t>(sign))) {
        return false;
    }
    if (prefix && (!sink.Put(L'0') || !sink.Put(static_cast<std::uint32_t>(field.conversion)))) {
        return false;
    }
    if (!sink.Repeat(L'0', zeros + (zeroPadding ? padding : 0))) {
        return false;
    }
    while (digits > 0) {
        if (!sink.Put(static_cast<std::uint32_t>(reversed[--digits]))) {
            return false;
        }
    }
    return !field.left || sink.Repeat(L' ', padding);
}

constexpr std::size_t kBinary64Digits = std::numeric_limits<double>::digits;
constexpr int kMinimumBinaryExponent = std::numeric_limits<double>::min_exponent
    - std::numeric_limits<double>::digits;
// Each factor 5 contributes less than one decimal digit. This bound also covers
// positive binary exponents and one rounding carry, without logarithmic math.
constexpr std::size_t kExactDigits = static_cast<std::size_t>(-kMinimumBinaryExponent)
    + std::numeric_limits<double>::max_digits10 + 1;
constexpr std::uint64_t kFractionMask = 0x000fffffffffffffULL;
constexpr std::uint64_t kExponentMask = 0x7ff0000000000000ULL;
constexpr std::uint64_t kSignMask = 0x8000000000000000ULL;
constexpr unsigned int kFractionBits = 52;
constexpr int kExponentBias = std::numeric_limits<double>::max_exponent - 1;
constexpr int kMinimumNormalExponent = std::numeric_limits<double>::min_exponent - 1;
constexpr std::size_t kHexFractionDigits = kFractionBits / kHexDigitBits;

struct DecimalValue {
    char digits[kExactDigits]{};
    std::size_t size = 1;
    int point = 1;
};

class DecimalInteger {
public:
    explicit DecimalInteger(std::uint64_t value) noexcept {
        do {
            limbs_[size_++] = static_cast<std::uint32_t>(value % kBase);
            value /= kBase;
        } while (value);
    }

    void Multiply(unsigned int multiplier) noexcept {
        std::uint64_t carry = 0;
        for (std::size_t index = 0; index < size_; ++index) {
            const std::uint64_t value = static_cast<std::uint64_t>(limbs_[index]) * multiplier + carry;
            limbs_[index] = static_cast<std::uint32_t>(value % kBase);
            carry = value / kBase;
        }
        if (carry) {
            limbs_[size_++] = static_cast<std::uint32_t>(carry);
        }
    }

    void Write(DecimalValue& value) const noexcept {
        value.size = 0;
        std::uint32_t top = limbs_[size_ - 1];
        char reversed[kLimbDigits]{};
        std::size_t count = 0;
        do {
            reversed[count++] = static_cast<char>('0' + top % kDecimalRadix);
            top /= kDecimalRadix;
        } while (top);
        while (count > 0) {
            value.digits[value.size++] = reversed[--count];
        }
        for (std::size_t index = size_ - 1; index > 0; --index) {
            std::uint32_t limb = limbs_[index - 1];
            std::uint32_t place = kBase / kDecimalRadix;
            while (place > 0) {
                value.digits[value.size++] = static_cast<char>('0' + limb / place);
                limb %= place;
                place /= kDecimalRadix;
            }
        }
    }

private:
    static constexpr std::uint32_t kBase = 1000000000;
    static constexpr std::size_t kLimbDigits = 9;
    static constexpr std::size_t kLimbs = (kExactDigits + kLimbDigits - 1) / kLimbDigits;
    std::uint32_t limbs_[kLimbs]{};
    std::size_t size_ = 0;
};

DecimalValue ExactDecimal(std::uint64_t bits) noexcept {
    DecimalValue result;
    const auto exponentField = static_cast<unsigned int>((bits & kExponentMask) >> kFractionBits);
    std::uint64_t mantissa = bits & kFractionMask;
    if (exponentField) {
        mantissa |= std::uint64_t{1} << kFractionBits;
    }
    if (mantissa == 0) {
        result.digits[0] = '0';
        return result;
    }
    const int exponent = (exponentField ? static_cast<int>(exponentField) - kExponentBias
        : kMinimumNormalExponent) - static_cast<int>(kFractionBits);
    DecimalInteger integer(mantissa);
    // Largest powers below the 10^9 limb base keep each carry in one limb.
    constexpr int kPowerFiveExponent = 12;
    constexpr unsigned int kPowerFive = 244140625;
    constexpr int kPowerTwoExponent = 29;
    constexpr unsigned int kPowerTwo = 1U << kPowerTwoExponent;
    const int chunkExponent = exponent < 0 ? kPowerFiveExponent : kPowerTwoExponent;
    const unsigned int chunk = exponent < 0 ? kPowerFive : kPowerTwo;
    int remaining = exponent < 0 ? -exponent : exponent;
    while (remaining >= chunkExponent) {
        integer.Multiply(chunk);
        remaining -= chunkExponent;
    }
    while (remaining > 0) {
        integer.Multiply(exponent < 0 ? 5U : 2U);
        --remaining;
    }
    integer.Write(result);
    int scale = exponent < 0 ? -exponent : 0;
    while (scale > 0 && result.size > 1 && result.digits[result.size - 1] == '0') {
        --result.size;
        --scale;
    }
    result.point = static_cast<int>(result.size) - scale;
    return result;
}

bool RoundUp(bool negative, int mode, bool discarded, bool greaterHalf, bool exactHalf, bool odd) noexcept {
    if (!discarded) {
        return false;
    }
    if (mode == FE_UPWARD) {
        return !negative;
    }
    if (mode == FE_DOWNWARD) {
        return negative;
    }
    if (mode == FE_TOWARDZERO) {
        return false;
    }
    return greaterHalf || (exactHalf && odd);
}

DecimalValue RoundDecimal(const DecimalValue& exact, std::int64_t keep, bool negative, int mode) noexcept {
    if (keep >= static_cast<std::int64_t>(exact.size)) {
        return exact;
    }
    // UCRT DEC-P*-first-significant controls: fixed formatting discards a
    // magnitude before this position even under directed rounding.
    if (keep < 0) {
        DecimalValue zero;
        zero.digits[0] = '0';
        return zero;
    }
    const std::size_t first = keep > 0 ? static_cast<std::size_t>(keep) : 0;
    bool discarded = false;
    bool tail = false;
    for (std::size_t index = first; index < exact.size; ++index) {
        if (exact.digits[index] != '0') {
            discarded = true;
            tail = tail || index != first;
        }
    }
    const char next = keep >= 0 ? exact.digits[first] : '0';
    const bool odd = keep > 0 && (exact.digits[first - 1] - '0') % 2 != 0;
    const bool increase = RoundUp(negative, mode, discarded, next > '5' || (next == '5' && tail),
        next == '5' && !tail, odd);
    DecimalValue rounded;
    if (keep <= 0) {
        rounded.digits[0] = increase ? '1' : '0';
        rounded.point = increase ? exact.point - static_cast<int>(keep) + 1 : 1;
        return rounded;
    }
    rounded.size = static_cast<std::size_t>(keep);
    rounded.point = exact.point;
    std::memcpy(rounded.digits, exact.digits, rounded.size);
    if (increase) {
        std::size_t index = rounded.size;
        while (index > 0 && rounded.digits[index - 1] == '9') {
            rounded.digits[--index] = '0';
        }
        if (index > 0) {
            ++rounded.digits[index - 1];
        } else {
            std::memmove(rounded.digits + 1, rounded.digits, rounded.size);
            rounded.digits[0] = '1';
            ++rounded.size;
            ++rounded.point;
        }
    }
    return rounded;
}

std::size_t ExponentDigits(int exponent, bool hexadecimal) noexcept {
    unsigned int value = static_cast<unsigned int>(exponent < 0 ? -exponent : exponent);
    std::size_t digits = 1;
    while (value >= kDecimalRadix) {
        value /= kDecimalRadix;
        ++digits;
    }
    return !hexadecimal && digits < kMinimumScientificExponentDigits ? kMinimumScientificExponentDigits : digits;
}

bool EmitExponent(Sink& sink, int exponent, wchar_t marker, bool hexadecimal) noexcept {
    if (!sink.Put(static_cast<std::uint32_t>(marker)) || !sink.Put(exponent < 0 ? L'-' : L'+')) {
        return false;
    }
    unsigned int value = static_cast<unsigned int>(exponent < 0 ? -exponent : exponent);
    wchar_t digits[kMaximumExponentDigits]{};
    std::size_t count = 0;
    do {
        digits[count++] = static_cast<wchar_t>(L'0' + value % kDecimalRadix);
        value /= kDecimalRadix;
    } while (value);
    if (!hexadecimal && count < kMinimumScientificExponentDigits && !sink.Put(L'0')) {
        return false;
    }
    while (count > 0) {
        if (!sink.Put(static_cast<std::uint32_t>(digits[--count]))) {
            return false;
        }
    }
    return true;
}

bool EmitDigits(Sink& sink, const DecimalValue& value, std::int64_t start, std::size_t count) noexcept {
    if (start < 0) {
        const std::size_t leading = count < static_cast<std::size_t>(-start) ? count : static_cast<std::size_t>(-start);
        if (!sink.Repeat(L'0', leading)) {
            return false;
        }
        count -= leading;
        start += static_cast<std::int64_t>(leading);
    }
    while (count > 0 && static_cast<std::size_t>(start) < value.size) {
        if (!sink.Put(static_cast<unsigned char>(value.digits[static_cast<std::size_t>(start)]))) {
            return false;
        }
        ++start;
        --count;
    }
    return sink.Repeat(L'0', count);
}

bool Floating(Sink& sink, const Field& field, va_list& arguments) noexcept {
    static_assert(sizeof(double) == sizeof(std::uint64_t) && std::numeric_limits<double>::is_iec559
        && kBinary64Digits == 53, "Owned formatting requires IEEE754 binary64");
    std::uint64_t bits;
    if (field.length == Length::LongDouble) {
        static_assert(sizeof(long double) == sizeof(double) && std::numeric_limits<long double>::digits == 53,
            "Selected native long double is binary64");
        const long double input = va_arg(arguments, long double);
        std::memcpy(&bits, &input, sizeof(bits));
    } else {
        const double input = va_arg(arguments, double);
        std::memcpy(&bits, &input, sizeof(bits));
    }
    const bool negative = (bits & kSignMask) != 0;
    const bool uppercase = field.conversion >= L'A' && field.conversion <= L'Z';
    const wchar_t conversion = uppercase ? field.conversion - L'A' + L'a' : field.conversion;
    const bool special = (bits & kExponentMask) == kExponentMask;
    const bool specialPoint = special && field.alternate && field.precision == 0 && conversion != L'g';
    // UCRT SPECIAL-ALT0 controls place a negative token after the point and
    // retain finite-style hex prefixes/zero padding for this exact predicate.
    const bool negativeSpecialPoint = negative && specialPoint;
    const bool hexadecimal = conversion == L'a' && (!special || negativeSpecialPoint);
    const wchar_t sign = negative ? L'-' : (field.plus ? L'+' : (field.space ? L' ' : 0));
    const int mode = std::fegetround();
    constexpr std::size_t kDefaultFloatingPrecision = 6;
    constexpr int kGeneralFixedMinimumExponent = -4;
    DecimalValue value;
    std::size_t precision = 0;
    int exponent = 0;
    bool scientific = false;
    std::uint64_t hexWord = 0;
    std::size_t storedHex = kHexFractionDigits;
    const char* token = nullptr;
    std::size_t body;
    if (special) {
        constexpr std::uint64_t kQuietMask = std::uint64_t{1} << (kFractionBits - 1);
        constexpr std::uint64_t kCanonicalIndefinite = kSignMask | kExponentMask | kQuietMask;
        token = (bits & kFractionMask) == 0 ? "inf"
            : ((bits & kQuietMask) == 0 ? "nan(snan)" : (bits == kCanonicalIndefinite ? "nan(ind)" : "nan"));
        body = std::strlen(token) + static_cast<std::size_t>(specialPoint);
    } else if (hexadecimal) {
        precision = field.precision < 0 ? kHexFractionDigits : static_cast<std::size_t>(field.precision);
        const auto exponentField = static_cast<unsigned int>((bits & kExponentMask) >> kFractionBits);
        exponent = exponentField ? static_cast<int>(exponentField) - kExponentBias : kMinimumNormalExponent;
        hexWord = bits & kFractionMask;
        if (exponentField) {
            hexWord |= std::uint64_t{1} << kFractionBits;
        } else if (hexWord == 0) {
            exponent = 0;
        }
        if (precision < kHexFractionDigits) {
            storedHex = precision;
            const auto discardedBits = static_cast<unsigned int>((kHexFractionDigits - precision) * kHexDigitBits);
            const std::uint64_t mask = (std::uint64_t{1} << discardedBits) - 1;
            const std::uint64_t remainder = hexWord & mask;
            const std::uint64_t half = std::uint64_t{1} << (discardedBits - 1);
            const std::uint64_t firstDiscardedNibble = remainder >> (discardedBits - kHexDigitBits);
            hexWord >>= discardedBits;
            // UCRT HEX-P*-nibble0 controls ignore a later nonzero tail when the
            // first discarded nibble is zero, including directed modes.
            if (RoundUp(negative, mode, firstDiscardedNibble != 0, remainder > half, remainder == half,
                (hexWord & 1) != 0)) {
                ++hexWord;
            }
        }
        body = 1 + (precision > 0 || field.alternate ? 1U : 0U) + precision
            + 2 + ExponentDigits(exponent, true);
    } else {
        const DecimalValue exact = ExactDecimal(bits);
        precision = field.precision < 0 ? kDefaultFloatingPrecision : static_cast<std::size_t>(field.precision);
        if (conversion == L'g' && precision == 0) {
            precision = 1;
        }
        const std::int64_t keep = conversion == L'f' ? static_cast<std::int64_t>(exact.point)
            + static_cast<std::int64_t>(precision)
            : static_cast<std::int64_t>(precision) + (conversion == L'e' ? 1 : 0);
        value = RoundDecimal(exact, keep, negative, mode);
        exponent = value.point - 1;
        scientific = conversion == L'e' || (conversion == L'g' && (exponent < kGeneralFixedMinimumExponent
            || static_cast<std::int64_t>(exponent) >= static_cast<std::int64_t>(precision)));
        if (conversion == L'g') {
            std::size_t significant = precision;
            if (!field.alternate) {
                significant = value.size < precision ? value.size : precision;
                while (significant > 1 && value.digits[significant - 1] == '0') {
                    --significant;
                }
            }
            precision = scientific ? significant - 1
                : (static_cast<std::int64_t>(significant) > value.point
                    ? static_cast<std::size_t>(static_cast<std::int64_t>(significant) - value.point) : 0);
        }
        body = (scientific ? 1U : static_cast<std::size_t>(value.point > 0 ? value.point : 1))
            + (precision > 0 || field.alternate ? 1U : 0U) + precision
            + (scientific ? 2 + ExponentDigits(exponent, false) : 0);
    }
    const std::size_t core = body + static_cast<std::size_t>(sign != 0) + (hexadecimal ? 2U : 0U);
    const std::size_t padding = field.width > core ? field.width - core : 0;
    const bool zeroPadding = field.zero && !field.left && (!special || negativeSpecialPoint);
    if (!field.left && !zeroPadding && !sink.Repeat(L' ', padding)) {
        return false;
    }
    if (sign && !sink.Put(static_cast<std::uint32_t>(sign))) {
        return false;
    }
    if (hexadecimal && (!sink.Put(L'0') || !sink.Put(uppercase ? L'X' : L'x'))) {
        return false;
    }
    if (zeroPadding && !sink.Repeat(L'0', padding)) {
        return false;
    }
    if (special) {
        if (negativeSpecialPoint && !sink.Put(L'.')) {
            return false;
        }
        for (const char* position = token; *position; ++position) {
            const auto unit = static_cast<unsigned char>(*position);
            if (!sink.Put(uppercase && unit >= 'a' && unit <= 'z' ? unit - 'a' + 'A' : unit)) {
                return false;
            }
            if (specialPoint && !negativeSpecialPoint && position == token && !sink.Put(L'.')) {
                return false;
            }
        }
    } else if (hexadecimal) {
        const char* alphabet = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
        const auto fractionBits = static_cast<unsigned int>(storedHex * kHexDigitBits);
        if (!sink.Put(static_cast<unsigned char>(alphabet[hexWord >> fractionBits]))) {
            return false;
        }
        if ((precision > 0 || field.alternate) && !sink.Put(L'.')) {
            return false;
        }
        for (std::size_t digit = 0; digit < storedHex; ++digit) {
            const auto shift = static_cast<unsigned int>((storedHex - digit - 1) * kHexDigitBits);
            if (!sink.Put(static_cast<unsigned char>(alphabet[(hexWord >> shift) & kHexDigitMask]))) {
                return false;
            }
        }
        if (!sink.Repeat(L'0', precision - storedHex)
            || !EmitExponent(sink, exponent, uppercase ? L'P' : L'p', true)) {
            return false;
        }
    } else {
        const std::size_t integral = scientific ? 1U : static_cast<std::size_t>(value.point > 0 ? value.point : 1);
        if (!EmitDigits(sink, value, !scientific && value.point <= 0 ? -1 : 0, integral)) {
            return false;
        }
        if ((precision > 0 || field.alternate) && !sink.Put(L'.')) {
            return false;
        }
        if (!EmitDigits(sink, value, scientific ? 1 : value.point, precision)) {
            return false;
        }
        if (scientific && !EmitExponent(sink, exponent, uppercase ? L'E' : L'e', false)) {
            return false;
        }
    }
    return !field.left || sink.Repeat(L' ', padding);
}

struct Result {
    int value;
    int error;
};

Result Format(CseLegacyWideFormatContext context, wchar_t* destination, std::size_t capacity,
    const wchar_t* format, va_list& arguments) noexcept {
    Sink sink{destination, capacity};
    const wchar_t* position = format;
    bool completed = true;
    while (*position && completed) {
        if (*position != L'%') {
            completed = EmitScalar(sink, static_cast<std::uint32_t>(*position++));
            continue;
        }
        ++position;
        Field field;
        if (!Parse(position, field, arguments)) {
            sink.error = EOVERFLOW;
            completed = false;
            break;
        }
        switch (field.conversion) {
        case 0:
            break;
        case L's':
        case L'S':
            completed = String(sink, field, context, arguments);
            break;
        case L'c':
        case L'C':
            completed = Character(sink, field, context, arguments);
            break;
        case L'd':
        case L'i':
        case L'u':
        case L'o':
        case L'x':
        case L'X':
        case L'p':
            completed = Integer(sink, field, arguments);
            break;
        case L'f':
        case L'F':
        case L'e':
        case L'E':
        case L'g':
        case L'G':
        case L'a':
        case L'A':
            completed = Floating(sink, field, arguments);
            break;
        case L'n':
            sink.error = EINVAL;
            completed = false;
            break;
        default:
            completed = EmitScalar(sink, static_cast<std::uint32_t>(field.conversion));
            break;
        }
    }
    sink.Terminate();
    return {completed && !sink.truncated ? static_cast<int>(sink.size) : -1, sink.error};
}
}

int CseLegacyVsnwprintfWithContext(CseLegacyWideFormatContext context, wchar_t* destination,
    std::size_t capacity, const wchar_t* format, va_list arguments) noexcept {
    if (!format || (!destination && capacity != 0)
        || capacity > static_cast<std::size_t>((std::numeric_limits<std::ptrdiff_t>::max)()) / sizeof(wchar_t)) {
        errno = EINVAL;
        return -1;
    }
    const int originalError = errno;
    va_list copy;
    va_copy(copy, arguments);
    const Result result = Format(context, destination, capacity, format, copy);
    va_end(copy);
    errno = result.error ? result.error : originalError;
    return result.value;
}

int CseLegacySnwprintfWithContext(CseLegacyWideFormatContext context, wchar_t* destination,
    std::size_t capacity, const wchar_t* format, ...) noexcept {
    va_list arguments;
    va_start(arguments, format);
    const int result = CseLegacyVsnwprintfWithContext(context, destination, capacity, format, arguments);
    va_end(arguments);
    return result;
}

int CseLegacyVsnwprintf(wchar_t* destination, std::size_t capacity, const wchar_t* format,
    va_list arguments) noexcept {
    return CseLegacyVsnwprintfWithContext({}, destination, capacity, format, arguments);
}

int CseLegacySnwprintf(wchar_t* destination, std::size_t capacity, const wchar_t* format, ...) noexcept {
    va_list arguments;
    va_start(arguments, format);
    const int result = CseLegacyVsnwprintf(destination, capacity, format, arguments);
    va_end(arguments);
    return result;
}
