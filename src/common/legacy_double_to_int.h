#ifndef CSE_LEGACY_DOUBLE_TO_INT_H
#define CSE_LEGACY_DOUBLE_TO_INT_H

#include <cstdint>
#include <cstring>
#include <limits>

#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__)
#include <emmintrin.h>
#endif

// Retained Win32 CVTTSD2SI returns integer-indefinite for invalid conversions
// with exceptions masked. Decode binary64 before any scalar signed conversion.
inline std::int32_t CseScalarTruncateDoubleToInt(double value) {
    constexpr unsigned int FractionBits = 52;
    constexpr unsigned int ExponentBits = 11;
    constexpr unsigned int ExponentBias = (1u << (ExponentBits - 1)) - 1;
    constexpr unsigned int SignShift = FractionBits + ExponentBits;
    constexpr unsigned int TargetMagnitudeBits = std::numeric_limits<std::int32_t>::digits;
    constexpr std::uint64_t ExponentMask = (UINT64_C(1) << ExponentBits) - 1;
    constexpr std::uint64_t FractionMask = (UINT64_C(1) << FractionBits) - 1;
    constexpr std::int32_t IntegerIndefinite = std::numeric_limits<std::int32_t>::min();
    static_assert(sizeof(double) == sizeof(std::uint64_t) && std::numeric_limits<double>::is_iec559
        && std::numeric_limits<double>::radix == 2 && std::numeric_limits<double>::digits == FractionBits + 1,
        "Legacy conversion requires IEEE binary64");
    std::uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    const unsigned int exponent = static_cast<unsigned int>((bits >> FractionBits) & ExponentMask);
    if (exponent < ExponentBias) {
        return 0;
    }
    const unsigned int integerBits = exponent - ExponentBias;
    if (integerBits >= TargetMagnitudeBits) {
        return IntegerIndefinite;
    }
    const std::uint64_t significand = (bits & FractionMask) | (UINT64_C(1) << FractionBits);
    const std::uint64_t magnitude = significand >> (FractionBits - integerBits);
    const std::int32_t integer = static_cast<std::int32_t>(magnitude);
    return (bits >> SignShift) != 0 ? -integer : integer;
}

inline std::int32_t CseLegacyTruncateDoubleToInt(double value) {
#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__)
    return _mm_cvttsd_si32(_mm_set_sd(value));
#else
    return CseScalarTruncateDoubleToInt(value);
#endif
}

#endif
