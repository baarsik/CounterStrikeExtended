#include "VGui1LegacyRaster.h"
#include "digest.h"
#include <array>
#include <cfenv>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace {
using namespace cse_vgui1::legacy;
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAILED legacy transfer: %s\n", message);
        std::abort();
    }
}

void ActualSourcePixel(const unsigned char* pixel, bool tab, unsigned char* destination) {
    using uchar = unsigned char;
    const int ch = tab ? '\t' : 'A';
    const float byteToUnit = 1.0f / 255.0f;
    const int outX = 0;
    const int outY = 0;
    const int rgbaWide = 1;
    constexpr int VGUI_RGBA_BYTES = 4;
    uchar* rgba = destination;
#include "actual_pixel.inc"
}

void CompleteRgbDomain() {
    Require(std::fegetround() == FE_TONEAREST, "recorded default rounding mode");
    Digest digest;
    constexpr unsigned int kByteValues = 256;
    std::array<unsigned char, 64 * 1024> block{};
    std::size_t used = 0;
    for (unsigned int blue = 0; blue < kByteValues; ++blue) {
        for (unsigned int green = 0; green < kByteValues; ++green) {
            for (unsigned int red = 0; red < kByteValues; ++red) {
                const unsigned char source[]{static_cast<unsigned char>(blue), static_cast<unsigned char>(green),
                    static_cast<unsigned char>(red)};
                unsigned char reference[4]{};
                unsigned char actual[4]{};
                ActualSourcePixel(source, false, reference);
                Require(ConvertLegacyBgr(source, false, actual) == Result::Ok
                    && std::memcmp(reference, actual, sizeof(actual)) == 0,
                    "complete RGB domain equals the extracted production pixel expressions");
                std::memcpy(block.data() + used, actual, sizeof(actual));
                used += sizeof(actual);
                if (used == block.size()) {
                    digest.Update(block.data(), used);
                    used = 0;
                }
            }
        }
    }
    Require(used == 0 && std::fegetround() == FE_TONEAREST, "streamed RGB output and rounding mode preserved");
    std::printf("FULL-RGB-SHA256\t%s\n", digest.Finish().c_str());
}

void BoundsAndStride() {
    const unsigned char source[]{1, 31, 103, 0, 17, 91, 203, 0, 0xcd, 0xcd, 0xcd, 0xcd,
        9, 42, 207, 0, 250, 100, 3, 0, 0xcd, 0xcd, 0xcd, 0xcd};
    std::array<unsigned char, 3 * 3 * 4> bytes;
    bytes.fill(0x5a);
    RgbaCanvas output{bytes.data(), bytes.size(), 3, 3};
    BgrView bottomUp{source, sizeof(source), 2, 2, -12};
    Require(TransferLegacyBgr(bottomUp, 2, 2, 1, 1, output, false) == Result::Ok,
        "negative pitch and nonzero pen");
    unsigned char expected[4]{};
    ActualSourcePixel(source + 12, false, expected);
    Require(std::memcmp(bytes.data() + (3 + 1) * 4, expected, 4) == 0 && bytes[0] == 0x5a,
        "bottom stored row becomes the logical top without touching canvas guards");
    const auto saved = bytes;
    for (const auto pitch : {std::ptrdiff_t{3}, (std::numeric_limits<std::ptrdiff_t>::min)()}) {
        auto invalid = bottomUp;
        invalid.pitch = pitch;
        Require(TransferLegacyBgr(invalid, 2, 2, 0, 0, output, false) == Result::InvalidArgument
            && bytes == saved, "invalid pitch has no partial output");
    }
    auto shortInput = bottomUp;
    shortInput.capacity = 19;
    auto shortOutput = output;
    shortOutput.capacity = bytes.size() - 1;
    Require(TransferLegacyBgr(shortInput, 2, 2, 0, 0, output, false) == Result::InvalidArgument
        && TransferLegacyBgr(bottomUp, 2, 2, 0, 0, shortOutput, false) == Result::InvalidArgument
        && TransferLegacyBgr(bottomUp, 3, 2, 0, 0, output, false) == Result::InvalidArgument
        && TransferLegacyBgr(bottomUp, 2, 2, -1, 0, output, false) == Result::InvalidArgument
        && bytes == saved, "capacity, source rectangle and negative pen fail atomically");
    BgrView alias{bytes.data(), bytes.size(), 1, 1, 4};
    Require(TransferLegacyBgr(alias, 1, 1, 0, 0, output, false) == Result::InvalidArgument && bytes == saved,
        "overlapping source/destination spans are outside the transfer contract");
    auto huge = output;
    huge.width = INT_MAX;
    huge.height = INT_MAX;
    huge.capacity = (std::numeric_limits<std::size_t>::max)();
    Require(TransferLegacyBgr(bottomUp, 2, 2, 0, 0, huge, false) == Result::InvalidArgument && bytes == saved,
        "unrepresentable canvas cannot narrow or wrap into a write");
    Require(TransferLegacyBgr(bottomUp, 2, 2, INT_MAX, 0, output, false) == Result::Ok && bytes == saved,
        "off-canvas positive pen clips without signed addition overflow");
    Require(TransferLegacyBgr(bottomUp, 2, 2, 2, 2, output, false) == Result::Ok,
        "right/bottom clipped rectangle");
    ActualSourcePixel(source + 12, false, expected);
    Require(std::memcmp(bytes.data() + 8 * 4, expected, 4) == 0, "one clipped source pixel remains correctly placed");
    const BgrView empty{nullptr, 0, 0, 0, 0};
    const RgbaCanvas emptyOutput{nullptr, 0, 0, 0};
    Require(TransferLegacyBgr(empty, 0, 0, 0, 0, emptyOutput, false) == Result::Ok,
        "empty transfer is a defined no-op");
}
}

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "rgb") == 0) {
        CompleteRgbDomain();
        return 0;
    }
    const unsigned char source[]{17, 91, 203, 99};
    unsigned char pixel[4]{};
    Require(ConvertLegacyBgr(source, false, pixel) == Result::Ok
        && pixel[0] == 17 && pixel[1] == 91 && pixel[2] == 203 && pixel[3] == 78,
        "source BGR and weighted alpha retain the actual legacy precision/order");
    std::array<unsigned char, 5 * 4 * 4> canvas;
    canvas.fill(0x5a);
    BgrView input{source, sizeof(source), 1, 1, 4};
    RgbaCanvas output{canvas.data(), canvas.size(), 5, 4};
    Require(TransferLegacyBgr(input, 1, 1, 2, 1, output, false) == Result::Ok
        && std::memcmp(canvas.data() + (5 + 2) * 4, pixel, 4) == 0 && canvas[0] == 0x5a,
        "nonzero pen writes only the copied rectangle");
    Require(TransferLegacyBgr(input, 1, 1, 2, 1, output, true) == Result::Ok
        && canvas[(5 + 2) * 4] == 0 && canvas[(5 + 2) * 4 + 3] == 0,
        "tab zeroes only its copied region");
    Require(ConvertLegacyBgr(nullptr, false, pixel) == Result::InvalidArgument
        && ConvertLegacyBgr(source, false, nullptr) == Result::InvalidArgument, "null pixel arguments rejected");
    BoundsAndStride();
    std::puts("PASS legacy pixel transfer");
}
