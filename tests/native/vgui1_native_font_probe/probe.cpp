#include "MacFontDiscovery.h"
#include "MacFontRegistration.h"
#include "VGui1LegacyRaster.h"
#include "digest.h"
#include <algorithm>
#include <array>
#include <climits>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <ft2build.h>
#include FT_FREETYPE_H

#if FREETYPE_MAJOR != 2 || FREETYPE_MINOR != 14 || FREETYPE_PATCH != 3
#error Requalify the probe before changing its pinned FreeType inputs
#endif

namespace {
constexpr std::size_t kMaximumFileBytes = 64U * 1024U * 1024U;
constexpr unsigned int kLegacyByteCount = 256;
constexpr int kRegularWeight = 400;
constexpr int kPixelBytes = 4;
constexpr int kPenX = 3;
constexpr int kPenY = 2;
constexpr int kCanvasMargin = 8;
constexpr const char* kOwnedFamily = "CSE Owned Raster V1";
constexpr const char* kOwnedProgram = "069885a3e451df7439b6ae8279e942b22bf4017468e4868dd94ae936bdf2659a";
// Stable token addresses survive an ordinary probe failure until process exit.
char g_FirstOwner = 0;
char g_SecondOwner = 0;

struct Request {
    const char* requested;
    const char* lookup;
    int tall;
    int weight;
    bool symbol;
};

// Only four VGUI1 families from Scheme.cpp/the pinned resource corpus. The
// final two lookups are the exact retained owner resolution-profile controls.
constexpr Request kNeeded[]{
    {"Arial", "Arial", 20, 400, false},
    {"Arial", "Arial", 12, 400, false},
    {"Arial", "Arial", 32, 700, false},
    {"Arial Narrow Bold", "Arial Narrow Bold", 36, 0, false},
    {"FixedSys", "FixedSys", 18, 400, false},
    {"Marlett", "Marlett", 16, 0, true},
    {"Arial Narrow Bold", "ArialMT", 20, 400, false},
    {"Arial", "Wingdings-Regular", 20, 400, true}};

std::string Hex(const char* value) {
    constexpr char kDigits[] = "0123456789abcdef";
    std::string result;
    if (value) {
        for (const auto* cursor = reinterpret_cast<const unsigned char*>(value); *cursor; ++cursor) {
            result += kDigits[*cursor >> 4];
            result += kDigits[*cursor & 15];
        }
    }
    return result;
}

bool Read(const char* path, std::vector<unsigned char>& bytes) {
    FILE* input = std::fopen(path, "rb");
    if (!input) {
        return false;
    }
    const bool seeked = std::fseek(input, 0, SEEK_END) == 0;
    const long length = seeked ? std::ftell(input) : -1;
    bool ok = length > 0 && static_cast<unsigned long>(length) <= kMaximumFileBytes
        && std::fseek(input, 0, SEEK_SET) == 0;
    if (ok) {
        bytes.resize(static_cast<std::size_t>(length));
        ok = std::fread(bytes.data(), 1, bytes.size(), input) == bytes.size();
    }
    return std::fclose(input) == 0 && ok;
}

std::string Hash(const std::vector<unsigned char>& bytes) {
    Digest digest;
    digest.Update(bytes.data(), bytes.size());
    return digest.Finish();
}

struct Library {
    FT_Library value = nullptr;
    ~Library() {
        if (value) {
            FT_Done_FreeType(value);
        }
    }
};

bool Candidate(const Request& request, const cse::fonts::FontFile& file,
    const std::vector<unsigned char>& bytes, FILE* output) {
    using namespace cse_vgui1::legacy;
    if (file.faceIndex != 0 || request.symbol || request.weight != kRegularWeight) {
        std::fprintf(output, "candidate\t%s\tunsupported-index-charset-or-weight\n", request.lookup);
        return true;
    }
    auto scratch = bytes;
    Result result = Result::InvalidArgument;
    int nativeError = 0;
    auto face = HarmonyCandidate::CreateOwned(scratch.data(), scratch.size(), request.tall, result, nativeError);
    std::fill(scratch.begin(), scratch.end(), 0);
    scratch.clear();
    scratch.shrink_to_fit();
    std::fprintf(output, "candidate\t%s\t%d\t%d\n", request.lookup, static_cast<int>(result), nativeError);
    if (!face) {
        return true; // Unavailable candidate math is a result, not a successful face.
    }
    const auto& metrics = face->GetMetrics();
    std::fprintf(output, "metrics\t%s\t%d\t%d\t%d\t%d\t%d\t%d\n", request.lookup, metrics.height,
        metrics.ascent, metrics.descent, metrics.internalLeading, metrics.averageWidth, metrics.maximumWidth);
    if (metrics.maximumWidth > INT_MAX - kCanvasMargin || metrics.height > INT_MAX - kCanvasMargin) {
        return false;
    }
    const int width = metrics.maximumWidth + kCanvasMargin;
    const int height = metrics.height + kCanvasMargin;
    constexpr std::size_t kMaximumCanvasBytes = 16U * 1024U * 1024U;
    if (static_cast<std::size_t>(width) > kMaximumCanvasBytes / kPixelBytes / static_cast<std::size_t>(height)) {
        return false;
    }
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * kPixelBytes, 0xa5);
    const RgbaCanvas canvas{pixels.data(), pixels.size(), width, height};
    Digest rasterDigest;
    for (unsigned int byte = 0; byte < kLegacyByteCount; ++byte) {
        GlyphAbc abc;
        if (face->GetAbc(byte, abc) != Result::Ok || face->Raster(byte, kPenX, kPenY, canvas) != Result::Ok) {
            return false;
        }
        std::fprintf(output, "abc\t%s\t%d\t%u\t%u\t%d\t%d\t%d\n", request.lookup, request.tall,
            byte, face->GetGlyphIndex(byte), abc.a, abc.b, abc.c);
        rasterDigest.Update(pixels.data(), pixels.size());
    }
    std::fprintf(output, "candidate-pixels-sha256\t%s\t%d\t%s\n", request.lookup, request.tall,
        rasterDigest.Finish().c_str());
    return true;
}

bool Inspect(FT_Library library, const Request& request, FILE* output) {
    cse::fonts::FontFile file;
    if (!cse::fonts::FindMacFontFile(library, request.lookup, request.weight, false, file)) {
        std::fprintf(output, "unavailable\t%s\t%s\t%d\t%d\t%d\n", request.requested,
            request.lookup, request.tall, request.weight, request.symbol);
        return true; // Missing required resources must remain visible ordinary results.
    }
    std::vector<unsigned char> bytes;
    if (!Read(file.path.c_str(), bytes)) {
        return false;
    }
    FT_Face metadata = nullptr;
    if (FT_New_Memory_Face(library, bytes.data(), static_cast<FT_Long>(bytes.size()), file.faceIndex, &metadata)) {
        return false;
    }
    const char* postscript = FT_Get_Postscript_Name(metadata);
    const bool sameIdentity = postscript && file.postScriptName == postscript;
    std::fprintf(output, "resolved\t%s\t%s\t%d\t%d\t%d\t%ld\t%s\t%s\t%s\t%s\t%ld\n",
        request.requested, request.lookup, request.tall, request.weight, request.symbol, file.faceIndex,
        Hex(file.path.c_str()).c_str(), Hex(postscript).c_str(), Hex(metadata->family_name).c_str(),
        Hash(bytes).c_str(), metadata->num_glyphs);
    FT_Done_Face(metadata);
    cse::fonts::FontFile repeated;
    const bool stable = cse::fonts::FindMacFontFile(library, request.lookup, request.weight, false, repeated)
        && repeated.path == file.path && repeated.postScriptName == file.postScriptName
        && repeated.faceIndex == file.faceIndex;
    return sameIdentity && stable && Candidate(request, file, bytes, output);
}

bool Registration(FT_Library library, const char* path, FILE* output) {
    std::vector<unsigned char> bytes;
    if (!Read(path, bytes) || Hash(bytes) != kOwnedProgram) {
        std::fputs("Only the exact authored fixture is accepted for registration\n", stderr);
        return false;
    }
    cse::fonts::FontFile file;
    if (cse::fonts::FindMacFontFile(library, kOwnedFamily, kRegularWeight, false, file)) {
        std::fputs("Authored identity already exists; this control refuses to assume its ownership\n", stderr);
        return false;
    }
    const bool acquired = CseMacFontRegistrationAcquire(&g_FirstOwner, path)
        && CseMacFontRegistrationAcquire(&g_FirstOwner, path)
        && CseMacFontRegistrationAcquire(&g_SecondOwner, path);
    const bool before = acquired
        && cse::fonts::FindMacFontFile(library, kOwnedFamily, kRegularWeight, false, file);
    const bool firstReleased = CseMacFontRegistrationReleaseOwner(&g_FirstOwner) != 0;
    const bool peer = firstReleased
        && cse::fonts::FindMacFontFile(library, kOwnedFamily, kRegularWeight, false, file);
    const bool secondReleased = CseMacFontRegistrationReleaseOwner(&g_SecondOwner) != 0;
    const bool absent = secondReleased
        && !cse::fonts::FindMacFontFile(library, kOwnedFamily, kRegularWeight, false, file);
    std::fprintf(output, "owned-registration\t%d\t%d\t%d\t%d\t%d\t%d\n",
        acquired, before, firstReleased, peer, secondReleased, absent);
    return acquired && before && peer && absent;
}
}

int main(int argc, char** argv) {
    const bool discovery = argc == 3 && std::strcmp(argv[1], "--discover-needed") == 0;
    const bool registration = argc == 4 && std::strcmp(argv[1], "--owned-registration") == 0;
    if (!discovery && !registration) {
        std::fputs("Reviewed runtime modes: --discover-needed output.tsv | --owned-registration authored.ttf output.tsv\n",
            stderr);
        return 2;
    }
    FILE* output = std::fopen(argv[argc - 1], "wb");
    if (!output) {
        return 2;
    }
    Library library;
    bool ok = FT_Init_FreeType(&library.value) == 0;
    if (ok && discovery) {
        for (const auto& request : kNeeded) {
            ok = Inspect(library.value, request, output) && ok;
        }
    } else if (ok) {
        ok = Registration(library.value, argv[2], output);
    }
    return std::fclose(output) == 0 && ok ? 0 : 1;
}
