#include "bzlib.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
void Check(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

void WriteU32(std::FILE* file, std::uint32_t value) {
    std::array<unsigned char, sizeof(value)> bytes{};
    constexpr unsigned int kByteBits = 8;
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<unsigned char>(value);
        value >>= kByteBits;
    }
    Check(std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size(), "Vector record write failed");
}

std::vector<char> Payload(unsigned int length, unsigned int pattern) {
    std::vector<char> data(std::max(length, 1u));
    std::uint32_t state = 0x5c16e3a9u;
    constexpr std::uint32_t kLcgMultiplier = 1664525u;
    constexpr std::uint32_t kLcgIncrement = 1013904223u;
    for (unsigned int index = 0; index < length; ++index) {
        state = state * kLcgMultiplier + kLcgIncrement;
        const unsigned char value = pattern == 0 ? 0 :
            (pattern == 1 ? static_cast<unsigned char>(index) : static_cast<unsigned char>(state >> 24));
        std::memcpy(&data[index], &value, sizeof(value));
    }
    return data;
}

void CheckFileApi(const std::string& directory) {
    const std::string path = directory + "/owned-stream.bz2";
    std::FILE* file = std::fopen(path.c_str(), "w+b");
    Check(file != nullptr, "Owned file creation failed");
    constexpr unsigned int kPayloadBytes = 17003;
    auto data = Payload(kPayloadBytes, 2);
    int error = BZ_OK;
    BZFILE* writer = BZ2_bzWriteOpen(&error, file, 1, 0, 30);
    Check(writer != nullptr && error == BZ_OK, "File codec open failed");
    constexpr unsigned int kWriteChunk = 1027;
    for (unsigned int offset = 0; offset < kPayloadBytes;) {
        const unsigned int count = std::min(kWriteChunk, kPayloadBytes - offset);
        BZ2_bzWrite(&error, writer, data.data() + offset, static_cast<int>(count));
        Check(error == BZ_OK, "File codec write failed");
        offset += count;
    }
    unsigned int inLow = 0;
    unsigned int inHigh = 0;
    unsigned int outLow = 0;
    unsigned int outHigh = 0;
    BZ2_bzWriteClose64(&error, writer, 0, &inLow, &inHigh, &outLow, &outHigh);
    Check(error == BZ_OK && inLow == kPayloadBytes && inHigh == 0 && outLow > 0 && outHigh == 0,
        "File write counts differ");
    Check(std::fseek(file, 0, SEEK_SET) == 0, "Owned file rewind failed");
    BZFILE* reader = BZ2_bzReadOpen(&error, file, 0, 0, nullptr, 0);
    Check(reader != nullptr && error == BZ_OK, "File reader open failed");
    std::vector<char> decoded;
    std::array<char, 1031> chunk{};
    while (error == BZ_OK) {
        const int count = BZ2_bzRead(&error, reader, chunk.data(), static_cast<int>(chunk.size()));
        Check(count >= 0 && static_cast<std::size_t>(count) <= chunk.size(), "File read count invalid");
        decoded.insert(decoded.end(), chunk.data(), chunk.data() + count);
    }
    Check(error == BZ_STREAM_END && decoded == data, "File decode content differs");
    BZ2_bzReadClose(&error, reader);
    Check(error == BZ_OK && std::fclose(file) == 0, "File close failed");
}
}

int main(int argc, char** argv) {
    Check(argc == 2, "Expected owned output directory");
    const std::string directory = argv[1];
    std::FILE* vectors = std::fopen((directory + "/vectors.bin").c_str(), "wb");
    Check(vectors != nullptr, "Vector output creation failed");
    constexpr unsigned int kCodecOverheadBytes = 600;
    const unsigned int lengths[]{0, 1, 3, 255, 4096, 65536, 100001, 900001};
    const int blockSizes[]{1, 9};
    std::uint32_t caseIndex = 0;
    for (unsigned int length : lengths) {
        for (unsigned int pattern = 0; pattern < 3; ++pattern) {
            auto source = Payload(length, pattern);
            for (int blockSize : blockSizes) {
                // The retained API specifies input size + 1 percent + 600 bytes.
                unsigned int compressedBytes = length + length / 100 + kCodecOverheadBytes;
                std::vector<char> compressed(compressedBytes);
                const int result = BZ2_bzBuffToBuffCompress(compressed.data(), &compressedBytes, source.data(),
                    length, blockSize, 0, 30);
                Check(result == BZ_OK, "Compression failed");
                for (int small : {0, 1}) {
                    unsigned int decodedBytes = std::max(length, 1u);
                    std::vector<char> decoded(decodedBytes);
                    Check(BZ2_bzBuffToBuffDecompress(decoded.data(), &decodedBytes, compressed.data(),
                        compressedBytes, small, 0) == BZ_OK, "Decompression failed");
                    Check(decodedBytes == length && std::memcmp(decoded.data(), source.data(), length) == 0,
                        "Decoded bytes differ");
                }
                WriteU32(vectors, caseIndex++);
                WriteU32(vectors, length);
                WriteU32(vectors, compressedBytes);
                Check(std::fwrite(compressed.data(), 1, compressedBytes, vectors) == compressedBytes,
                    "Compressed vector write failed");
            }
        }
    }
    Check(std::fclose(vectors) == 0, "Vector output close failed");
    CheckFileApi(directory);
    std::printf("PASS %u exact compression vectors, both decompression modes and owned file streaming\n", caseIndex);
}
