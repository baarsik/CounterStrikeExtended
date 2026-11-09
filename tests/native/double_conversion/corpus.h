#ifndef CSE_DOUBLE_CONVERSION_CORPUS_H
#define CSE_DOUBLE_CONVERSION_CORPUS_H

#include <cstdint>
#include <cstring>

namespace double_conversion_fixture {

// Binary64 encodings pin zeros, subnormals, fractions, binary32 precision loss,
// signed32 boundaries, non-finite values, and payload/sign variants independently.
constexpr std::uint64_t BoundaryBits[] = {
    UINT64_C(0x0000000000000000),
    UINT64_C(0x8000000000000000),
    UINT64_C(0x0000000000000001),
    UINT64_C(0x8000000000000001),
    UINT64_C(0x0010000000000000),
    UINT64_C(0x8010000000000000),
    UINT64_C(0x3fe0000000000000),
    UINT64_C(0xbfe0000000000000),
    UINT64_C(0x3fefffffffffffff),
    UINT64_C(0xbfefffffffffffff),
    UINT64_C(0x3ff0000000000000),
    UINT64_C(0xbff0000000000000),
    UINT64_C(0x3ffc000000000000),
    UINT64_C(0xbffc000000000000),
    UINT64_C(0x4170000010000000),
    UINT64_C(0xc170000010000000),
    UINT64_C(0x41dfffffffc00000),
    UINT64_C(0x41dfffffffe00000),
    UINT64_C(0x41dfffffffffffff),
    UINT64_C(0x41e0000000000000),
    UINT64_C(0x41e0000000000001),
    UINT64_C(0xc1e0000000000000),
    UINT64_C(0xc1e0000000000001),
    UINT64_C(0xc1dfffffffffffff),
    UINT64_C(0xc1e0000000180000),
    UINT64_C(0xc1e0000000200000),
    UINT64_C(0x7ff0000000000000),
    UINT64_C(0xfff0000000000000),
    UINT64_C(0x7ff8000000000001),
    UINT64_C(0xfff8000000000001),
    UINT64_C(0x7ff0000000000001),
    UINT64_C(0xfff0000000000001),
    UINT64_C(0x7ff8123456789abc),
    UINT64_C(0x7fefffffffffffff),
    UINT64_C(0xffefffffffffffff),
};
constexpr int SeededSamples = 1000000;
constexpr std::uint64_t RandomSeed = UINT64_C(0x6d2b79f5a5c3e217);

inline double FromBits(std::uint64_t bits) {
    double value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

inline std::uint64_t ToBits(double value) {
    std::uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

template<class Callback>
void ForEachBits(Callback callback) {
    for (std::uint64_t bits : BoundaryBits) {
        callback(bits);
    }
    std::uint64_t state = RandomSeed;
    for (int index = 0; index < SeededSamples; ++index) {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        callback(state);
    }
}

}

#endif
