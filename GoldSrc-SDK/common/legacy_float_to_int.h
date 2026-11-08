#ifndef CSE_LEGACY_FLOAT_TO_INT_H
#define CSE_LEGACY_FLOAT_TO_INT_H

#include <cstdint>
#include <cstring>
#include <limits>

#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__)
#include <xmmintrin.h>
#endif

// CVTTSS2SI returns integer-indefinite for NaN, infinity and overflow when exceptions are masked.
// Decode binary32 before converting so the ARM path never performs an out-of-range C++ cast.
inline std::int32_t CseScalarTruncateFloatToInt(float value) {
    static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559
        && std::numeric_limits<float>::digits == 24, "Legacy conversion requires IEEE binary32");
    constexpr unsigned int FractionBits = 23;
    constexpr unsigned int ExponentBias = 127;
    constexpr unsigned int SignShift = 31;
    constexpr std::uint32_t ExponentMask = 0xff;
    constexpr std::uint32_t FractionMask = (1u << FractionBits) - 1;
    constexpr std::int32_t IntegerIndefinite = std::numeric_limits<std::int32_t>::min();
    std::uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    const unsigned int exponent = (bits >> FractionBits) & ExponentMask;
    if (exponent < ExponentBias) {
        return 0;
    }
    const unsigned int integerBits = exponent - ExponentBias;
    if (integerBits >= SignShift) {
        return IntegerIndefinite;
    }
    const std::uint32_t significand = (bits & FractionMask) | (1u << FractionBits);
    const std::uint32_t magnitude = integerBits >= FractionBits
        ? significand << (integerBits - FractionBits) : significand >> (FractionBits - integerBits);
    const auto integer = static_cast<std::int32_t>(magnitude);
    return (bits >> SignShift) != 0 ? -integer : integer;
}

inline std::int32_t CseLegacyTruncateFloatToInt(float value) {
#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__)
    return _mm_cvtt_ss2si(_mm_set_ss(value));
#else
    return CseScalarTruncateFloatToInt(value);
#endif
}
#endif
