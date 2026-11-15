#include <array>
#include <cstdio>
#include <cstring>

#include "bzlib.h"

extern "C" {
#include "bzlib_private.h"
}

namespace {

bool CheckSelectorCount(int count, int expectedResult) {
    constexpr int SelectorCountBits = 15; // Width of the bzip2 selector-count field.
    constexpr int MinimumHuffmanGroups = 2;
    bz_stream stream{};
    DState state{};
    state.strm = &stream;
    state.state = BZ_X_SELECTOR_2;
    state.bsBuff = static_cast<UInt32>(count);
    state.bsLive = SelectorCountBits;
    state.save_nGroups = MinimumHuffmanGroups;

    // Resume at the count field with no selector payload. Even the unfixed decoder
    // can only request more input; this fixture never reaches an out-of-bounds write.
    const int result = BZ2_decompress(&state);
    if (result != expectedResult) {
        std::fprintf(stderr, "selector count %d: expected %d, got %d\n", count, expectedResult, result);
        return false;
    }
    return true;
}

bool CheckRoundTrips() {
    constexpr std::size_t InputBytes = 4096;
    constexpr std::size_t CompressionOverheadBytes = 1024;
    constexpr int ByteValueCount = 256;
    constexpr int MinimumBlockSize = 1;
    constexpr int MaximumBlockSize = 9;
    constexpr int DefaultWorkFactor = 30;
    std::array<char, InputBytes> input{};
    std::array<char, InputBytes + CompressionOverheadBytes> compressed{};
    std::array<char, InputBytes> decoded{};
    for (std::size_t index = 0; index < input.size(); ++index) {
        input[index] = static_cast<char>(index % ByteValueCount);
    }

    for (const int blockSize : {MinimumBlockSize, MaximumBlockSize}) {
        unsigned int compressedBytes = static_cast<unsigned int>(compressed.size());
        const int compressionResult = BZ2_bzBuffToBuffCompress(compressed.data(), &compressedBytes, input.data(),
            static_cast<unsigned int>(input.size()), blockSize, 0, DefaultWorkFactor);
        if (compressionResult != BZ_OK) {
            std::fprintf(stderr, "compression failed: %d\n", compressionResult);
            return false;
        }
        for (const int smallDecompress : {0, 1}) {
            decoded.fill(0);
            unsigned int decodedBytes = static_cast<unsigned int>(decoded.size());
            const int result = BZ2_bzBuffToBuffDecompress(decoded.data(), &decodedBytes, compressed.data(),
                compressedBytes, smallDecompress, 0);
            if (result != BZ_OK || decodedBytes != input.size() || decoded != input) {
                std::fprintf(stderr, "round trip failed: block=%d small=%d result=%d bytes=%u\n",
                    blockSize, smallDecompress, result, decodedBytes);
                return false;
            }
        }
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        return 2;
    }
    const char* scenario = argv[1];
    if (std::strcmp(scenario, "selector-zero") == 0) {
        return CheckSelectorCount(0, BZ_DATA_ERROR) ? 0 : 1;
    }
    if (std::strcmp(scenario, "selector-minimum") == 0) {
        return CheckSelectorCount(1, BZ_OK) ? 0 : 1;
    }
    if (std::strcmp(scenario, "selector-maximum") == 0) {
        return CheckSelectorCount(BZ_MAX_SELECTORS, BZ_OK) ? 0 : 1;
    }
    if (std::strcmp(scenario, "selector-overflow") == 0) {
        return CheckSelectorCount(BZ_MAX_SELECTORS + 1, BZ_DATA_ERROR) ? 0 : 1;
    }
    if (std::strcmp(scenario, "roundtrip") == 0) {
        return CheckRoundTrips() ? 0 : 1;
    }
    return 2;
}
