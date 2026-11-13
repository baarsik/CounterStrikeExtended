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
void Ordinary(const std::vector<unsigned char>& bytes,
    const char* identity = "CSEOwnedRasterV1-Regular", int weight = 400) {
    auto input = bytes;
    auto face = Make(input, 0, identity, Request(weight));
    input.assign(input.size(), 0);
    Result result = Result::Ok;
    int error = 0;
    auto baseline = HarmonyCandidate::CreateOwned(bytes.data(), bytes.size(), 20, result, error);
    Require(baseline && result == Result::Ok, "frozen candidate comparator");
    Require(face->FaceIndex() == 0 && face->PostScriptName() == identity
        && face->IntrinsicWeight() == static_cast<unsigned int>(weight), "owned selected face provenance");
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
    Make(regular, 0, "CSEOwnedRasterV1-Regular", Request(700));
    Make(bold, 0, "CSEOwnedRasterV1-Bold", Request());
    Make(bold, 0, "CSEOwnedRasterV1-Bold", Request(500));
    Make(regular, 0, "CSEOwnedRasterV1-Regular", Request(600));
    auto request = Request();
    request.ansiCodePage = 65001;
    Make(regular, 0, "CSEOwnedRasterV1-Regular", request, Result::UnsupportedRequest);
    request = Request();
    request.tall = INT_MAX;
    Make(regular, 0, "CSEOwnedRasterV1-Regular", request, Result::Unrepresentable);
    request = Request();
    request.wide = INT_MIN;
    Make(regular, 0, "CSEOwnedRasterV1-Regular", request, Result::Unrepresentable);
    Make(std::vector<unsigned char>(16), 0, "invalid", Request(), Result::InvalidFont);
}

// These assertions catch dropped style/geometry requests using actual FT outlines,
// not invented GDI pixel or metric equivalence.
std::vector<unsigned char> Pixels(NativeFontFace& face, unsigned int byte) {
    constexpr int kWidth = 128;
    constexpr int kHeight = 64;
    std::vector<unsigned char> pixels(kWidth * kHeight * 4, 0);
    Require(face.Raster(byte, 0, 0, kWidth, kHeight, pixels.data()), "styled glyph raster succeeds");
    return pixels;
}

void RequestedStyles(const std::vector<unsigned char>& regular) {
    auto plain = Make(regular, 0, "CSEOwnedRasterV1-Regular", Request());
    const auto ordinary = Pixels(*plain, 'A');
    Require(std::any_of(ordinary.begin(), ordinary.end(), [](unsigned char pixel) { return pixel != 0; }),
        "ordinary geometry has ink for style comparisons");
    for (int weight : {600, 700}) {
        auto face = Make(regular, 0, "CSEOwnedRasterV1-Regular", Request(weight));
        Require(Pixels(*face, 'A') != ordinary, "missing intrinsic bold is synthesized");
    }
    for (auto flag : {FontItalic, FontUnderline, FontStrikeout}) {
        auto request = Request();
        request.flags = flag;
        auto face = Make(regular, 0, "CSEOwnedRasterV1-Regular", request);
        Require(Pixels(*face, 'A') != ordinary, "each requested style changes actual glyph pixels");
        if (flag != FontItalic) {
            for (unsigned int byte : {'A', ' '}) {
                int plainA = 0;
                int plainB = 0;
                int plainC = 0;
                int styledA = 0;
                int styledB = 0;
                int styledC = 0;
                Require(plain->RawAbc(byte, plainA, plainB, plainC)
                    && face->RawAbc(byte, styledA, styledB, styledC)
                    && plainA == styledA && plainB == styledB && plainC == styledC,
                    "decoration preserves the original ABC and cropped cell");
                const auto pixels = Pixels(*face, byte);
                constexpr int kWidth = 128;
                constexpr int kHeight = 64;
                constexpr int kPixelBytes = 4;
                for (int row = 0; row < kHeight; ++row) {
                    for (int column = plainB; column < kWidth; ++column) {
                        Require(pixels[(row * kWidth + column) * kPixelBytes + 3] == 0,
                            "decoration never extends beyond the retained glyph cell");
                    }
                }
            }
        }
    }
    auto wideRequest = Request();
    wideRequest.wide = 20;
    auto wide = Make(regular, 0, "CSEOwnedRasterV1-Regular", wideRequest);
    int a = 0;
    int b = 0;
    int c = 0;
    Require(plain->RawAbc('A', a, b, c), "plain advance");
    const int ordinaryAdvance = a + b + c;
    Require(wide->RawAbc('A', a, b, c) && a + b + c > ordinaryAdvance
        && wide->DetailedMetrics().averageWidth == wideRequest.wide, "requested average width scales advances");
    const int wideAdvance = a + b + c;
    Require(wide->RawAbc('L', a, b, c) && a + b + c > wideAdvance,
        "average width request preserves proportional advances");
    Require(Pixels(*wide, 'A') != ordinary, "requested width scales rendered glyph");
    auto rotatedRequest = Request();
    rotatedRequest.rotation = 30.0f;
    rotatedRequest.rotationTenths = 300;
    auto rotated = Make(regular, 0, "CSEOwnedRasterV1-Regular", rotatedRequest);
    Require(Pixels(*rotated, 'A') != ordinary, "escapement rotates actual outline");
    int rotatedA = 0;
    int rotatedB = 0;
    int rotatedC = 0;
    Require(plain->RawAbc('A', a, b, c) && rotated->RawAbc('A', rotatedA, rotatedB, rotatedC)
        && a == rotatedA && b == rotatedB && c == rotatedC, "rotation preserves untransformed ABC contract");
    // A positive right angle rotates top-aligned text above its unrotated cell.
    // Baseline-pivot rotation instead leaves a conspicuous strip inside the cell.
    auto uprightRequest = Request();
    uprightRequest.rotation = 90.0f;
    uprightRequest.rotationTenths = 900;
    auto upright = Make(regular, 0, "CSEOwnedRasterV1-Regular", uprightRequest);
    const auto uprightPixels = Pixels(*upright, 'H'); // Authored rectangle with positive left bearing.
    Require(std::all_of(uprightPixels.begin(), uprightPixels.end(), [](unsigned char pixel) { return pixel == 0; }),
        "positive escapement rotates around the original top-aligned pen");
    rotatedRequest.rotation = -30.0f;
    rotatedRequest.rotationTenths = -300;
    auto clockwise = Make(regular, 0, "CSEOwnedRasterV1-Regular", rotatedRequest);
    Require(Pixels(*clockwise, 'A') != ordinary && Pixels(*clockwise, 'A') != Pixels(*rotated, 'A'),
        "negative escapement rotates in the opposite direction");
    auto quantizedRequest = Request();
    quantizedRequest.rotation = 0.01f;
    auto quantized = Make(regular, 0, "CSEOwnedRasterV1-Regular", quantizedRequest);
    Require(Pixels(*quantized, 'A') == ordinary, "sub-tenth rotation retains original integer quantization");
    auto combinedRequest = wideRequest;
    combinedRequest.rotation = -30.0f;
    combinedRequest.rotationTenths = -300;
    combinedRequest.flags = FontItalic | FontUnderline | FontStrikeout;
    auto combined = Make(regular, 0, "CSEOwnedRasterV1-Regular", combinedRequest);
    Require(Pixels(*combined, 'A') != Pixels(*wide, 'A'), "combined styles and clockwise rotation rasterize");
    const auto tab = Pixels(*combined, '\t');
    Require(std::all_of(tab.begin(), tab.end(), [](unsigned char pixel) { return pixel == 0; }),
        "styled tab remains blank");
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
    Ordinary(Read(directory + "/bold.ttf"), "CSEOwnedRasterV1-Bold", 700);
    RequestedStyles(regular);
    StylesAndIndices(directory, regular);
    Symbols(directory);
    for (int iteration = 0; iteration < 32; ++iteration) {
        Make(regular, 0, "CSEOwnedRasterV1-Regular", Request());
    }
    std::puts("PASS authored indexed/native face; no GDI or UI parity claim");
}
