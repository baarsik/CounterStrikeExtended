#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

using byte = unsigned char;
using word = unsigned short;

namespace {
constexpr std::size_t GuardBytes = 32;
constexpr byte GuardValue = 0xa7;
constexpr std::size_t FixturePaletteEntries = 256;
constexpr std::size_t PaletteStride = 4;
constexpr std::size_t HeaderBytes = 128;
std::vector<byte> allocation;
std::vector<byte> written;
std::string diagnostics;
int allocationCalls = 0;
int requestedBytes = 0;
int fileWrites = 0;
bool failAllocation = false;

void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "pcx_fixture_failure=%s\n", message);
        std::exit(1);
    }
}

void Reset() {
    allocation.clear();
    written.clear();
    diagnostics.clear();
    allocationCalls = 0;
    requestedBytes = 0;
    fileWrites = 0;
    failAllocation = false;
}
}

void* Hunk_TempAlloc(int bytes) {
    ++allocationCalls;
    requestedBytes = bytes;
    if (failAllocation) {
        return nullptr;
    }
    Require(bytes > 0, "temporary allocation count is positive");
    allocation.assign(static_cast<std::size_t>(bytes) + GuardBytes * 2, GuardValue);
    // Hunk_HighAllocName zeroes the entire allocation before Hunk_TempAlloc returns its payload.
    std::memset(allocation.data() + GuardBytes, 0, static_cast<std::size_t>(bytes));
    return allocation.data() + GuardBytes;
}

void Con_Printf(const char* message, ...) {
    diagnostics += message;
}

short LittleShort(short value) {
    // All qualification targets for this fixture (Windows x86/x64/ARM64, macOS ARM64) are little endian.
    const word marker = 1;
    Require(*reinterpret_cast<const byte*>(&marker) == 1, "fixture target uses little-endian PCX fields");
    return value;
}

void Q_memset(void* output, int value, std::size_t bytes) {
    std::memset(output, value, bytes);
}

void COM_WriteFile(char* filename, void* data, int bytes) {
    Require(std::string(filename) == "fixture.pcx", "filename preserved");
    Require(data == allocation.data() + GuardBytes && bytes >= 0 && bytes <= requestedBytes,
        "file output length remains within its temporary allocation");
    ++fileWrites;
    const auto* begin = static_cast<const byte*>(data);
    written.assign(begin, begin + bytes);
}

#include "production_bodies.inc"

namespace {
std::array<word, FixturePaletteEntries * PaletteStride> Palette() {
    std::array<word, FixturePaletteEntries * PaletteStride> palette{};
    for (std::size_t index = 0; index < FixturePaletteEntries; ++index) {
        palette[index * PaletteStride] = static_cast<word>(0xa000 + index);
        palette[index * PaletteStride + 1] = static_cast<word>(0xb000 + ((index + 13) & 0xff));
        palette[index * PaletteStride + 2] = static_cast<word>(0xc000 + (255 - index));
        palette[index * PaletteStride + 3] = 0xdead;
    }
    return palette;
}

void Write(byte* pixels, int width, int height, int stride, word* palette) {
    char name[] = "fixture.pcx";
    WritePCXfile(name, pixels, width, height, stride, palette);
}

void CheckGuards() {
    Require(std::all_of(allocation.begin(), allocation.begin() + GuardBytes,
        [](byte value) { return value == GuardValue; }), "leading temporary allocation canary");
    Require(std::all_of(allocation.end() - GuardBytes, allocation.end(),
        [](byte value) { return value == GuardValue; }), "trailing temporary allocation canary");
}

void StoreWord(std::vector<byte>& bytes, std::size_t offset, int value) {
    bytes[offset] = static_cast<byte>(value);
    bytes[offset + 1] = static_cast<byte>(static_cast<unsigned int>(value) >> 8);
}

std::vector<byte> Expected(int width, int height, const std::vector<byte>& encoded) {
    std::vector<byte> result(HeaderBytes);
    result[0] = 10;
    result[1] = 5;
    result[2] = 1;
    result[3] = 8;
    StoreWord(result, 8, width - 1);
    StoreWord(result, 10, height - 1);
    StoreWord(result, 12, width);
    StoreWord(result, 14, height);
    result[65] = 1;
    StoreWord(result, 66, width);
    StoreWord(result, 68, 2);
    result.insert(result.end(), encoded.begin(), encoded.end());
    result.push_back(12);
    for (std::size_t index = 0; index < FixturePaletteEntries; ++index) {
        result.push_back(static_cast<byte>(255 - index));
        result.push_back(static_cast<byte>((index + 13) & 0xff));
        result.push_back(static_cast<byte>(index));
    }
    return result;
}

void ExactBytes() {
    Reset();
    std::array<byte, 14> pixels{0, 0xbf, 0xc0, 0xc1, 0xff, 0x40, 0,
        0xc0, 0xc0, 1, 0xc1, 0xfe, 2, 0x7f};
    const auto originalPixels = pixels;
    auto palette = Palette();
    const auto originalPalette = palette;
    Write(pixels.data(), 7, 2, 7, palette.data());
    const std::vector<byte> encoded{0, 0xbf, 0xc1, 0xc0, 0xc1, 0xc1, 0xc1, 0xff, 0x40, 0,
        0xc1, 0xc0, 0xc1, 0xc0, 1, 0xc1, 0xc1, 0xc1, 0xfe, 2, 0x7f};
    Require(written == Expected(7, 2, encoded), "exact 128-byte header, single-pixel RLE and BGR palette bytes");
    Require(requestedBytes == 7 * 2 * 2 + 1000 && fileWrites == 1, "legacy allocation size and single write");
    Require(pixels == originalPixels && palette == originalPalette, "source pixel/palette canaries remain unchanged");
    CheckGuards();
}

void RowStride() {
    auto palette = Palette();
    std::array<byte, 10> pixels{0xa7, 1, 0xc0, 0xff, 0xee, 2, 3, 4, 0xdd, 0xa7};
    const auto original = pixels;
    Reset();
    Write(pixels.data() + 1, 3, 2, 4, palette.data());
    Require(written == Expected(3, 2, {1, 0xc1, 0xc0, 0xc1, 0xff, 2, 3, 4}),
        "positive source row stride skips padding");
    CheckGuards();
    Reset();
    Write(pixels.data() + 5, 3, 2, -4, palette.data());
    Require(written == Expected(3, 2, {2, 3, 4, 1, 0xc1, 0xc0, 0xc1, 0xff}),
        "negative source row stride preserves logical row order");
    Require(pixels == original, "row stride never modifies source pixels or guards");
    CheckGuards();
}

void WordFields() {
    auto palette = Palette();
    for (int extent : {32768, 65535}) {
        std::vector<byte> pixels(static_cast<std::size_t>(extent), 0xff);
        std::vector<byte> encoded;
        for (int pixel = 0; pixel < extent; ++pixel) {
            encoded.push_back(0xc1);
            encoded.push_back(0xff);
        }
        for (bool vertical : {false, true}) {
            Reset();
            const int width = vertical ? 1 : extent;
            const int height = vertical ? extent : 1;
            Write(pixels.data(), width, height, width, palette.data());
            Require(written == Expected(width, height, encoded),
                "upper 16-bit dimensions preserve exact little-endian bits");
            CheckGuards();
        }
    }
}

void AllocationFailure() {
    Reset();
    failAllocation = true;
    byte pixels[4]{};
    auto palette = Palette();
    Write(pixels, 2, 2, 2, palette.data());
    Require(allocationCalls == 1 && requestedBytes == 1008 && fileWrites == 0 &&
        diagnostics == "SCR_ScreenShot_f: not enough memory\n",
        "ordinary allocation failure retains its diagnostic and does not write");
}

void Reject(int width, int height) {
    Reset();
    failAllocation = true; // An old-code failure remains a controlled assertion rather than a huge allocation/access.
    Write(nullptr, width, height, width, nullptr);
    Require(allocationCalls == 0 && fileWrites == 0 && !diagnostics.empty(),
        "invalid or unrepresentable PCX storage rejected before allocation and input/file access");
}

void LargeCounts() {
    Reject(65535, 65535);
    Reject(std::numeric_limits<int>::max(), 2);
    constexpr std::size_t LegacyOverhead = 1000;
    constexpr std::size_t MaximumTemp =
        (static_cast<std::size_t>(std::numeric_limits<int>::max()) - sizeof(hunk_t)) / HUNK_ALIGNMENT * HUNK_ALIGNMENT;
    // Its 2*pixels+1000 request fits int, but hunk rounding plus the real header would overflow it.
    constexpr int HeaderBoundaryWidth = 32586;
    constexpr int HeaderBoundaryHeight = 32951;
    constexpr std::size_t HeaderBoundaryRequest =
        static_cast<std::size_t>(HeaderBoundaryWidth) * HeaderBoundaryHeight * 2 + LegacyOverhead;
    static_assert(HeaderBoundaryRequest <= static_cast<std::size_t>(std::numeric_limits<int>::max()) &&
        HeaderBoundaryRequest > MaximumTemp, "Fixture distinguishes int conversion from hunk storage capacity");
    Reject(HeaderBoundaryWidth, HeaderBoundaryHeight);
    constexpr int Width = 65535;
    constexpr int LastHeight = static_cast<int>((MaximumTemp - LegacyOverhead) / 2 / Width);
    Reset();
    failAllocation = true;
    Write(nullptr, Width, LastHeight, Width, nullptr);
    const std::size_t expected = static_cast<std::size_t>(Width) * LastHeight * 2 + LegacyOverhead;
    Require(allocationCalls == 1 && requestedBytes == static_cast<int>(expected) && fileWrites == 0,
        "last representable tall image reaches allocation without count truncation");
    Reject(Width, LastHeight + 1);
}

void InvalidDimensions() {
    for (const auto& dimensions : {std::array<int, 2>{0, 1}, {1, 0}, {-1, 1}, {1, -1},
                                  {65536, 1}, {1, 65536}, {std::numeric_limits<int>::min(), 1}}) {
        Reject(dimensions[0], dimensions[1]);
    }
}
}

int main(int count, char* arguments[]) {
    Require(count == 2, "named PCX fixture case");
    const std::string name = arguments[1];
    if (name == "exact-bytes") {
        ExactBytes();
    } else if (name == "row-stride") {
        RowStride();
    } else if (name == "word-fields") {
        WordFields();
    } else if (name == "allocation-failure") {
        AllocationFailure();
    } else if (name == "large-count-rejection") {
        LargeCounts();
    } else if (name == "invalid-dimensions") {
        InvalidDimensions();
    } else {
        Require(false, "known PCX case");
    }
    std::printf("pcx_fixture_passed=%s\n", name.c_str());
    return 0;
}
