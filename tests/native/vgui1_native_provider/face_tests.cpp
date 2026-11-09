#include "VGui1NativeFontFace.h"
#include <algorithm>
#include <array>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

namespace {
using namespace cse_vgui1;
using namespace cse_vgui1::legacy;
void Require(bool value, const char* text) {
    if (!value) {
        std::fprintf(stderr, "FAILED native face: %s\n", text);
        std::abort();
    }
}
std::vector<unsigned char> Read(const std::string& path) {
    FILE* file = nullptr;
#if defined(_MSC_VER)
    if (fopen_s(&file, path.c_str(), "rb") != 0) {
        file = nullptr;
    }
#else
    file = std::fopen(path.c_str(), "rb");
#endif
    Require(file != nullptr, "owned authored input");
    std::array<unsigned char, 32768> data{};
    const auto count = std::fread(data.data(), 1, data.size(), file);
    Require(count > 0 && count < data.size() && std::fclose(file) == 0, "bounded authored bytes");
    return {data.begin(), data.begin() + static_cast<std::ptrdiff_t>(count)};
}
FontRequest Request(int weight = 400) {
    FontRequest request;
    request.requestedName = "CSE Owned Raster V1";
    request.lookupIdentity = request.requestedName;
    request.resolutionProfile = "exact-request-v1";
    request.tall = 20;
    request.weight = weight;
    return request;
}
std::unique_ptr<NativeFontFace> Make(const std::vector<unsigned char>& input, long index,
    const char* ps, const FontRequest& request, Result expected = Result::Ok) {
    Result result = Result::Ok;
    int error = 0;
    std::string diagnostic;
    auto face = NativeFontFace::CreateOwned(input.data(), input.size(), index, ps, request,
        result, error, diagnostic);
    if (result != expected) {
        std::fprintf(stderr, "result=%d FT=%d diagnostic=%s\n", static_cast<int>(result), error, diagnostic.c_str());
    }
    Require(result == expected && (face != nullptr) == (expected == Result::Ok), "creation status/face atomicity");
    if (!face) {
        Require(!diagnostic.empty(), "ordinary rejection diagnostic");
    }
    return face;
}
void Ordinary(const std::vector<unsigned char>& bytes) {
    auto input = bytes;
    auto face = Make(input, 0, "CSEOwnedRasterV1-Regular", Request());
    input.assign(input.size(), 0);
    Result result = Result::Ok;
    int error = 0;
    auto baseline = HarmonyCandidate::CreateOwned(bytes.data(), bytes.size(), 20, result, error);
    Require(baseline && result == Result::Ok, "frozen candidate comparator");
    Require(face->FaceIndex() == 0 && face->PostScriptName() == "CSEOwnedRasterV1-Regular"
        && face->IntrinsicWeight() == 400, "owned selected face provenance");
    const auto metrics = face->DetailedMetrics();
    const auto old = baseline->GetMetrics();
    Require(metrics.height == old.height && metrics.ascent == old.ascent && metrics.descent == old.descent
        && metrics.internalLeading == old.internalLeading && metrics.averageWidth == old.averageWidth
        && metrics.maximumWidth == old.maximumWidth, "same profile ordinary metric values");
    Require(face->Metrics().capabilities == (CapabilityAbc | CapabilityLegacyRgba | CapabilityPenOffsets
        | CapabilityWeight), "only proven ordinary capabilities");
    constexpr int width = 40;
    constexpr int height = 32;
    std::vector<unsigned char> actual(width * height * 4);
    auto expected = actual;
    for (unsigned int pass = 0; pass < 3; ++pass) {
        for (unsigned int cell = 0; cell < 256; ++cell) {
            const unsigned int byte = pass == 1 ? 255 - cell : cell;
            GlyphAbc abc{};
            int a = 0;
            int b = 0;
            int c = 0;
            Require(baseline->GetAbc(byte, abc) == Result::Ok && face->RawAbc(byte, a, b, c)
                && a == abc.a && b == abc.b && c == abc.c
                && face->GlyphIndex(byte) == baseline->GetGlyphIndex(byte), "all256 raw ABC and glyph IDs");
            std::fill(actual.begin(), actual.end(), 0x5a);
            expected = actual;
            Require(face->Raster(byte, 3, 5, width, height, actual.data())
                && baseline->Raster(byte, 3, 5, {expected.data(), expected.size(), width, height}) == Result::Ok
                && actual == expected, "same legacy BGR/weighted-alpha/pen/stale-canvas output");
        }
    }
    int a = 91;
    int b = 92;
    int c = 93;
    Require(!face->RawAbc(256, a, b, c) && a == 91 && b == 92 && c == 93, "invalid ABC output atomic");
    expected = actual;
    Require(!face->Raster(256, 0, 0, width, height, actual.data())
        && !face->Raster('A', -1, 0, width, height, actual.data()) && actual == expected,
        "invalid raster does not write");
    Require(!face->Raster('A', 0, 0, INT_MAX, INT_MAX, actual.data()), "unrepresentable destination span");
}
void StylesAndIndices(const std::string& directory, const std::vector<unsigned char>& regular) {
    const auto bold = Read(directory + "/bold.ttf");
    const auto collection = Read(directory + "/indexed.ttc");
    auto defaultBold = Make(bold, 0, "CSEOwnedRasterV1-Bold", Request(0));
    Require(defaultBold->IntrinsicWeight() == 700, "weight0 preserves intrinsic named bold");
    auto explicitBold = Make(bold, 0, "CSEOwnedRasterV1-Bold", Request(700));
    auto indexed = Make(collection, 1, "CSEOwnedRasterV1-Bold", Request(700));
    Require(indexed->FaceIndex() == 1 && indexed->GlyphIndex('A') == explicitBold->GlyphIndex('A'),
        "nonzero collection index retained");
    Make(collection, 0, "CSEOwnedRasterV1-Bold", Request(700), Result::InvalidFont);
    Make(collection, 2, "CSEOwnedRasterV1-Bold", Request(700), Result::InvalidFont);
    Make(collection, 0x10001, "CSEOwnedRasterV1-Bold", Request(700), Result::InvalidFont);
    Result rejected = Result::Ok;
    int nativeError = 777;
    std::string diagnostic;
    Require(!NativeFontFace::CreateOwned(regular.data(), (std::numeric_limits<std::size_t>::max)(),
        0, "CSEOwnedRasterV1-Regular", Request(), rejected, nativeError, diagnostic)
        && rejected == Result::Unrepresentable && nativeError == 0, "oversize span rejected before copying");
    Require(!NativeFontFace::CreateOwned(nullptr, 0, 0, "CSEOwnedRasterV1-Regular", Request(),
        rejected, nativeError, diagnostic) && rejected == Result::InvalidArgument, "null span rejected");
    Make(regular, 0, "Wrong-Identity", Request(), Result::InvalidFont);
    Make(regular, -1, "CSEOwnedRasterV1-Regular", Request(), Result::InvalidArgument);
    Make(regular, 0, "CSEOwnedRasterV1-Regular", Request(700), Result::UnsupportedRequest);
    Make(bold, 0, "CSEOwnedRasterV1-Bold", Request(), Result::UnsupportedRequest);
    Make(regular, 0, "CSEOwnedRasterV1-Regular", Request(600), Result::UnsupportedRequest);
    for (auto flag : {FontItalic, FontUnderline, FontStrikeout}) {
        auto request = Request();
        request.flags = flag;
        Make(regular, 0, "CSEOwnedRasterV1-Regular", request, Result::UnsupportedRequest);
    }
    auto request = Request();
    request.wide = 2;
    Make(regular, 0, "CSEOwnedRasterV1-Regular", request, Result::UnsupportedRequest);
    request = Request();
    request.rotation = 0.01f;
    Make(regular, 0, "CSEOwnedRasterV1-Regular", request, Result::UnsupportedRequest);
    request = Request();
    request.ansiCodePage = 65001;
    Make(regular, 0, "CSEOwnedRasterV1-Regular", request, Result::UnsupportedRequest);
    request = Request();
    request.tall = INT_MAX;
    Make(regular, 0, "CSEOwnedRasterV1-Regular", request, Result::Unrepresentable);
    Make(std::vector<unsigned char>(16), 0, "invalid", Request(), Result::InvalidFont);
}
void Symbols(const std::string& directory) {
    auto request = Request(0);
    request.flags = FontSymbol;
    for (const char* mode : {"direct", "high", "both"}) {
        auto face = Make(Read(directory + "/symbol-" + mode + ".ttf"), 0, "CSEOwnedSymbolV1-Regular", request);
        Require((face->Metrics().capabilities & CapabilitySymbol) != 0, "validated symbol capability");
        for (unsigned int byte = 0; byte < 256; ++byte) {
            Require(face->GlyphIndex(byte) == (byte == 0 ? 0 : 2 + byte % 7), "actual enumerated symbol byte map");
        }
    }
    Make(Read(directory + "/symbol-ambiguous.ttf"), 0, "CSEOwnedSymbolV1-Regular", request,
        Result::UnsupportedRequest);
    Make(Read(directory + "/regular.ttf"), 0, "CSEOwnedRasterV1-Regular", request,
        Result::UnsupportedRequest);
}
}
int main(int argc, char** argv) {
    Require(argc == 2, "owned input directory");
    const std::string directory = argv[1];
    const auto regular = Read(directory + "/regular.ttf");
    Ordinary(regular);
    StylesAndIndices(directory, regular);
    Symbols(directory);
    for (int iteration = 0; iteration < 32; ++iteration) {
        Make(regular, 0, "CSEOwnedRasterV1-Regular", Request());
    }
    std::puts("PASS authored indexed/native face; no GDI or UI parity claim");
}
