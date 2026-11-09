#ifndef CSE_DOUBLE_BINARY_IO_H
#define CSE_DOUBLE_BINARY_IO_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace double_conversion_fixture {

constexpr unsigned int BitsPerEncodedByte = 8;

inline void Require(bool condition, const char* description) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        std::exit(1);
    }
}

template<class Unsigned>
void WriteBits(std::FILE* output, Unsigned value) {
    for (unsigned int shift = 0; shift < sizeof(value) * BitsPerEncodedByte; shift += BitsPerEncodedByte) {
        Require(std::fputc(static_cast<unsigned char>(value >> shift), output) != EOF, "write encoded result");
    }
}

template<class Unsigned>
Unsigned ReadBits(std::FILE* input) {
    Unsigned value = 0;
    for (unsigned int shift = 0; shift < sizeof(value) * BitsPerEncodedByte; shift += BitsPerEncodedByte) {
        const int byte = std::fgetc(input);
        Require(byte != EOF, "read complete encoded result");
        value |= static_cast<Unsigned>(byte) << shift;
    }
    return value;
}

}

#endif
