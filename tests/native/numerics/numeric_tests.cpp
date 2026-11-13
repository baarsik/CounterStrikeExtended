#include "../../../src/common/legacy_float_to_int.h"

#include <cfenv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <initializer_list>

#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__)
#include <xmmintrin.h>
#define CSE_FIXTURE_X86 1
#else
#define CSE_FIXTURE_X86 0
#endif

namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Numeric fixture failed: %s\n", message);
        std::exit(1);
    }
}

float FromBits(std::uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void WriteInt(std::FILE* output, std::int32_t value) {
    const auto bits = static_cast<std::uint32_t>(value);
    for (unsigned int shift = 0; shift < 32; shift += 8) {
        Require(std::fputc(static_cast<unsigned char>(bits >> shift), output) != EOF, "write numeric corpus");
    }
}

void Check(std::FILE* output, std::uint32_t bits) {
    const float value = FromBits(bits);
    const std::int32_t scalar = CseScalarTruncateFloatToInt(value);
    const std::int32_t actual = CseLegacyTruncateFloatToInt(value);
    Require(actual == scalar, "native and scalar conversions agree");
#if CSE_FIXTURE_X86
    // This is the frozen production conversion instruction, including integer-indefinite for NaN/overflow.
    const int reference = _mm_cvtt_ss2si(_mm_set_ss(value));
    if (scalar != reference) {
        std::fprintf(stderr, "conversion bits=%08x scalar=%d SSE=%d\n", bits, scalar, reference);
        std::exit(1);
    }
#endif
    WriteInt(output, actual);
}
} // namespace

int main(int argc, char** argv) {
    Require(argc == 2, "one output path required");
    std::FILE* output = std::fopen(argv[1], "wb");
    Require(output != nullptr, "open numeric corpus");
    constexpr std::uint32_t Boundaries[] = {
        0, 0x80000000, 1, 0x80000001, 0x007fffff, 0x00800000,
        0x3f000000, 0xbf000000, 0x3f7fffff, 0xbf7fffff, 0x3f800000, 0xbf800000,
        0x4effffff, 0x4f000000, 0x4f000001, 0xceffffff, 0xcf000000, 0xcf000001,
        0x7f7fffff, 0xff7fffff, 0x7f800000, 0xff800000, 0x7fc00000, 0xffc00000,
        0x7f800001, 0xff800001
    };
    for (const auto bits : Boundaries) {
        Check(output, bits);
    }
    const int savedRounding = std::fegetround();
    for (int rounding : {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO}) {
        Require(std::fesetround(rounding) == 0, "set conversion rounding mode");
        for (const auto bits : Boundaries) {
            Check(output, bits);
        }
    }
    Require(std::fesetround(savedRounding) == 0, "restore conversion rounding mode");
    constexpr std::uint32_t RandomSeed = 0x6d2b79f5;
    constexpr int CorpusSamples = 1000000;
    std::uint32_t state = RandomSeed;
    for (int index = 0; index < CorpusSamples; ++index) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        Check(output, state);
    }
    constexpr std::int32_t IntegerIndefinite = std::numeric_limits<std::int32_t>::min();
    Require(CseLegacyTruncateFloatToInt(FromBits(0x4f000000)) == IntegerIndefinite, "positive overflow sentinel");
    Require(CseLegacyTruncateFloatToInt(FromBits(0x7fc00000)) == IntegerIndefinite, "NaN sentinel");
    Require(CseLegacyTruncateFloatToInt(-1.75f) == -1, "truncate toward zero");
    Require(std::fclose(output) == 0, "close numeric corpus");
    std::puts("1,000,130 binary32 patterns checked");
    return 0;
}
