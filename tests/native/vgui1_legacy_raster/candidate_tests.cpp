#include "VGui1LegacyRaster.h"
#include <algorithm>
#include <array>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <limits>
#include <vector>

namespace {
using namespace cse_vgui1::legacy;
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAILED Harmony candidate fixture: %s\n", message);
        std::abort();
    }
}

std::vector<unsigned char> Read(const char* path) {
    FILE* input = std::fopen(path, "rb");
    Require(input != nullptr, "owned input file");
    constexpr std::size_t kMaximumAuthoredFontBytes = 16 * 1024;
    std::array<unsigned char, kMaximumAuthoredFontBytes> bytes{};
    const std::size_t length = std::fread(bytes.data(), 1, bytes.size(), input);
    Require(length > 0 && length < bytes.size() && std::fclose(input) == 0, "bounded authored font bytes");
    return {bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(length)};
}

void Probe(const std::vector<unsigned char>& bytes, const char* directory, const char* order,
    const std::vector<int>& sequence) {
    auto input = bytes;
    Result result = Result::InvalidArgument;
    int nativeError = 0;
    auto face = HarmonyCandidate::CreateOwned(input.data(), input.size(), 20, result, nativeError);
    if (!face) {
        std::fprintf(stderr, "candidate result=%d FTerror=%d\n", static_cast<int>(result), nativeError);
    }
    Require(face != nullptr && result == Result::Ok, "owned ordinary candidate creation");
    input.assign(input.size(), 0);
    const auto metrics = face->GetMetrics();
    constexpr int kPenX = 3;
    constexpr int kPenY = 5;
    constexpr int kGuardCells = 4;
    constexpr std::size_t kPixelBytes = 4;
    const int width = metrics.maximumWidth + kPenX + kGuardCells;
    const int height = metrics.height + kPenY + kGuardCells;
    Require(width > 0 && width < 2048 && height > 0 && height < 2048, "bounded candidate geometry");
    std::vector<unsigned char> output(static_cast<std::size_t>(width) * height * kPixelBytes);
    const std::string prefix = std::string(directory) + "/" + order;
    FILE* meta = std::fopen((prefix + "-metrics.tsv").c_str(), "wb");
    FILE* abc = std::fopen((prefix + "-abc.tsv").c_str(), "wb");
    FILE* rgba = std::fopen((prefix + "-rgba.bin").c_str(), "wb");
    Require(meta && abc && rgba, "owned candidate output files");
    std::fprintf(meta, "%d\t%d\t%d\t%d\t0\t%d\t%d\t400\t0\t%d\t%d\t%d\t%d\t%zu\n",
        metrics.height, metrics.ascent, metrics.descent, metrics.internalLeading, metrics.averageWidth,
        metrics.maximumWidth, width, height, metrics.maximumWidth, metrics.height * 2, sequence.size());
    for (std::size_t index = 0; index < sequence.size(); ++index) {
        const auto byte = static_cast<unsigned int>(sequence[index]);
        GlyphAbc raw{};
        Require(face->GetAbc(byte, raw) == Result::Ok, "owned ABC query");
        const int a = raw.a < 0 ? 0 : raw.a;
        std::fprintf(abc, "%zu\t%u\t%u\t%u\t%d\t%d\t%d\t%d\t%d\t%d\n", index, byte,
            static_cast<unsigned int>(LegacyByteToUnicode(static_cast<unsigned char>(byte))),
            face->GetGlyphIndex(byte), raw.a, raw.b, raw.c, a, raw.b, raw.c);
        std::fill(output.begin(), output.end(), static_cast<unsigned char>(0x5a));
        const RgbaCanvas canvas{output.data(), output.size(), width, height};
        Require(face->Raster(byte, kPenX, kPenY, canvas) == Result::Ok, "owned Harmony raster operation");
        Require(std::fwrite(output.data(), 1, output.size(), rgba) == output.size(), "owned candidate RGBA bytes");
    }
    for (FILE* file : {meta, abc, rgba}) {
        Require(std::fclose(file) == 0, "candidate output close");
    }
}

void OwnershipAndFailureControls(const std::vector<unsigned char>& bytes) {
    for (int iteration = 0; iteration < 32; ++iteration) {
        Result result = Result::Ok;
        int nativeError = 0;
        auto face = HarmonyCandidate::CreateOwned(bytes.data(), bytes.size(), 20, result, nativeError);
        Require(face && result == Result::Ok && nativeError == 0, "repeated independent owned FT contexts");
        GlyphAbc poisoned{91, 92, 93};
        Require(face->GetAbc(256, poisoned) == Result::InvalidArgument
            && poisoned.a == 91 && poisoned.b == 92 && poisoned.c == 93,
            "invalid byte query cannot truncate or change ABC output");
        std::array<unsigned char, 8 * 8 * 4> pixels;
        pixels.fill(0x5a);
        const auto saved = pixels;
        const RgbaCanvas canvas{pixels.data(), pixels.size(), 8, 8};
        Require(face->Raster(256, 0, 0, canvas) == Result::InvalidArgument
            && face->Raster('A', -1, 0, canvas) == Result::InvalidArgument && pixels == saved,
            "invalid raster request cannot partially write the output");
    }
    Result result = Result::Ok;
    int error = 777;
    Require(!HarmonyCandidate::CreateOwned(nullptr, 0, 20, result, error)
        && result == Result::InvalidArgument && error == 0, "null/empty input fails without native dispatch");
    Require(!HarmonyCandidate::CreateOwned(bytes.data(), bytes.size(), 0, result, error)
        && result == Result::InvalidArgument, "zero height is outside this ordinary candidate");
    Require(!HarmonyCandidate::CreateOwned(bytes.data(), bytes.size(), -20, result, error)
        && result == Result::InvalidArgument, "negative-height policy is deliberately unimplemented");
    Require(!HarmonyCandidate::CreateOwned(bytes.data(), (std::numeric_limits<std::size_t>::max)(), 20, result, error)
        && result == Result::Unrepresentable, "unrepresentable font size fails before copying or allocating");
    Require(!HarmonyCandidate::CreateOwned(bytes.data(), bytes.size(), INT_MAX, result, error)
        && result == Result::Unrepresentable, "unrepresentable canvas height fails before pixel allocation");
    const std::array<unsigned char, 16> invalid{};
    Require(!HarmonyCandidate::CreateOwned(invalid.data(), invalid.size(), 20, result, error)
        && result == Result::InvalidFont && error != 0, "malformed font is an ordinary FT failure");
}
}

int main(int argc, char** argv) {
    Require(argc == 3, "owned input and output directory arguments");
    const auto bytes = Read(argv[1]);
    OwnershipAndFailureControls(bytes);
    std::vector<int> ascending;
    std::vector<int> descending;
    constexpr int kByteCount = 256;
    for (int byte = 0; byte < kByteCount; ++byte) {
        ascending.push_back(byte);
        descending.push_back(kByteCount - 1 - byte);
    }
    Probe(bytes, argv[2], "ascending", ascending);
    Probe(bytes, argv[2], "descending", descending);
    Probe(bytes, argv[2], "sequence", {'A', ' ', 'C', ' ', 'I', 'C', 'A', '\t', 'B', 'A', 'A', ' '});
    std::puts("PASS measured owned Harmony candidate; this is not a parity or capability assertion");
}
