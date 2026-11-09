// PREPARED ONLY. No CTest entry. A separate reviewed execution grant is required.
#include "VGui1FixedSysFontFace.h"
#include "VGui1MacFontProvider.h"
#include "MacFontDiscovery.h"
#include "digest.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <ft2build.h>
#include FT_FREETYPE_H

namespace {
using namespace cse_vgui1;
using namespace cse_vgui1::legacy;
constexpr char kCourierPath[] = "/System/Library/Fonts/Courier.ttc";
constexpr char kCourierHash[] = "76cdad2ba4e8774223ed6184f530570261f9f7fb9beb5c68368e09234c0273ec";
constexpr char kDefaultCanvasHash[] = "cfa79e1ed0df70dd52de2bf2030731b125ea07424b53ec0a44f9a8ec296ce0a7";
constexpr int kCanvasSide = 24;
constexpr int kPenX = 3;
constexpr int kPenY = 5;
constexpr std::size_t kPixelBytes = 4;
constexpr std::size_t kCanvasBytes = kCanvasSide * kCanvasSide * kPixelBytes;
constexpr unsigned int kByteCount = 256;

std::string FileHash(const char* path) {
    FILE* input = std::fopen(path, "rb");
    if (!input) {
        return {};
    }
    Digest digest;
    std::array<unsigned char, 8192> bytes{};
    for (;;) {
        const auto count = std::fread(bytes.data(), 1, bytes.size(), input);
        digest.Update(bytes.data(), count);
        if (count < bytes.size()) {
            const bool read = !std::ferror(input);
            const bool closed = std::fclose(input) == 0;
            return read && closed ? digest.Finish() : std::string{};
        }
    }
}

FontRequest Request() {
    FontRequest request;
    request.requestedName = "FixedSys";
    request.lookupIdentity = "Courier";
    request.resolutionProfile = FixedSysRoleProfileV1;
    request.tall = 18;
    request.weight = FixedSysRoleWeightV1;
    return request;
}

bool Measure(void* context, FILE* facts, FILE* cells) {
    const auto request = Request();
    const auto& provider = CseVgui1MacFontProviderCandidateV1();
    FT_Library lookup = nullptr;
    if (FT_Init_FreeType(&lookup) != 0) {
        return false;
    }
    cse::fonts::FontFile input;
    const bool found = cse::fonts::FindMacFontFile(lookup, "Courier", FixedSysRoleWeightV1, false, input);
    FT_Done_FreeType(lookup);
    std::fprintf(facts, "lookup\tfound=%d\tpath=%s\tps=%s\tindex=%ld\n", found,
        input.path.c_str(), input.postScriptName.c_str(), input.faceIndex);
    std::fflush(facts);
    if (!found || input.path != kCourierPath || input.postScriptName != "Courier" || input.faceIndex != 0) {
        std::fputs("status\tIdentityFailure\n", facts);
        return false;
    }
    int error = 0;
    std::string diagnostic;
    auto face = provider.Realize(context, request, {}, error, diagnostic);
    const std::string after = FileHash(kCourierPath);
    std::fprintf(facts, "input-after\t%s\tstable=%d\n", after.c_str(), after == kCourierHash);
    if (after != kCourierHash) {
        std::fputs("status\tIdentityFailure\n", facts);
        return false;
    }
    if (!face) {
        std::fprintf(facts, "status\tUnavailable\terror=%d\tdiagnostic=%s\n", error, diagnostic.c_str());
        return error != 0 && !diagnostic.empty();
    }
    auto* role = dynamic_cast<FixedSysFontFace*>(face.get());
    if (!role || error != 0 || !diagnostic.empty()) {
        std::fputs("status\tInvalidReady\n", facts);
        return false;
    }
    const auto metrics = role->Metrics();
    const auto info = role->Info();
    std::fprintf(facts, "role\t%s\trequested=FixedSys\tlookup=Courier\tH=%d\tW=%d\tcapabilities=%u\n",
        FixedSysRoleProfileV1, metrics.height, metrics.maximumWidth, metrics.capabilities);
    std::fprintf(facts, "metrics\tascent=%d\tdescent=%d\tinternalLeading=%d\troleWeight=%d\t"
        "numericWeightKnown=%d\tnumericWeight=%u\tppem=%d\toriginX=%d\tleft=%d\tright=%d\ttop=%d\tbottom=%d\n",
        info.ascent, info.descent, info.internalLeading, info.roleWeight, info.numericWeightKnown,
        info.numericWeight, info.fitPpem, info.originX, info.minimumBitmapLeft, info.maximumBitmapRight,
        info.maximumBitmapTop, info.maximumBitmapBottom);
    if (metrics.height != FixedSysCellHeightV1 || metrics.maximumWidth != FixedSysCellWidthV1
        || info.ascent != FixedSysAscentV1 || info.descent != FixedSysDescentV1
        || info.internalLeading != FixedSysInternalLeadingV1 || info.numericWeightKnown) {
        return false;
    }
    std::array<unsigned int, 4> routes{};
    std::array<unsigned char, kCanvasBytes> canvas{};
    for (unsigned int byte = 0; byte < kByteCount; ++byte) {
        int a = 0;
        int b = 0;
        int c = 0;
        FixedSysByteRoute route = FixedSysByteRoute::ForcedBlank;
        if (!role->RawAbc(byte, a, b, c) || a != 0 || b != FixedSysCellWidthV1 || c != 0
            || !role->Route(byte, route) || static_cast<std::size_t>(route) >= routes.size()) {
            return false;
        }
        ++routes[static_cast<std::size_t>(route)];
        canvas.fill(0x5a);
        if (!role->Raster(byte, kPenX, kPenY, kCanvasSide, kCanvasSide, canvas.data())) {
            return false;
        }
        unsigned int nonzeroAlpha = 0;
        unsigned int alphaSum = 0;
        int left = FixedSysCellWidthV1;
        int top = FixedSysCellHeightV1;
        int right = 0;
        int bottom = 0;
        for (int y = 0; y < FixedSysCellHeightV1; ++y) {
            for (int x = 0; x < FixedSysCellWidthV1; ++x) {
                const auto offset = static_cast<std::size_t>((y + kPenY) * kCanvasSide + x + kPenX)
                    * kPixelBytes;
                const auto alpha = canvas[offset + kPixelBytes - 1];
                alphaSum += alpha;
                if (alpha != 0) {
                    ++nonzeroAlpha;
                    left = x < left ? x : left;
                    top = y < top ? y : top;
                    right = x + 1 > right ? x + 1 : right;
                    bottom = y + 1 > bottom ? y + 1 : bottom;
                }
            }
        }
        Digest digest;
        digest.Update(canvas.data(), canvas.size());
        const auto hash = digest.Finish();
        if ((route == FixedSysByteRoute::ForcedBlank) != (nonzeroAlpha == 0)
            || (route == FixedSysByteRoute::AuthoredDefaultBlock && hash != kDefaultCanvasHash)
            || (route == FixedSysByteRoute::MappedGraphic && role->GlyphIndex(byte) == 0)
            || (route == FixedSysByteRoute::NativeNotdef && role->GlyphIndex(byte) != 0)) {
            return false;
        }
        std::fprintf(cells, "%u\t%u\t%u\t%d\t%d\t%d\t%u\t%u\t%d\t%d\t%d\t%d\t%s\n",
            byte, static_cast<unsigned int>(route), role->GlyphIndex(byte), a, b, c,
            nonzeroAlpha, alphaSum, left, top, right, bottom, hash.c_str());
    }
    const bool counts = routes == std::array<unsigned int, 4>{{16, 22, 2, 216}};
    std::fprintf(facts, "status\t%s\tcounts=%u/%u/%u/%u\tregistration-calls=0\n",
        counts ? "Ready" : "InvalidRoutes", routes[0], routes[1], routes[2], routes[3]);
    return counts;
}
}

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--describe") == 0) {
        std::puts("PREPARED NOT RUN: exact pinned Courier/index0 for explicit FixedSys18 role only; "
            "no registration, UI, preferences, program/raster output or qualified factory; 256 cell facts/digests");
        return 0;
    }
    if (argc != 4 || std::strcmp(argv[1], "--fixedsys-role") != 0 || std::strcmp(argv[2], kCourierPath) != 0) {
        return 64;
    }
    FILE* facts = std::fopen((std::string(argv[3]) + "/facts.tsv").c_str(), "wb");
    FILE* cells = std::fopen((std::string(argv[3]) + "/cells.tsv").c_str(), "wb");
    if (!facts || !cells) {
        if (facts) {
            std::fclose(facts);
        }
        if (cells) {
            std::fclose(cells);
        }
        return 65;
    }
    const std::string before = FileHash(kCourierPath);
    std::fprintf(facts, "input-before\t%s\t%s\n", kCourierPath, before.c_str());
    std::fflush(facts);
    // Hash mismatch fails before the first lookup/provider query.
    if (before != kCourierHash) {
        std::fputs("status\tIdentityFailure\n", facts);
        std::fclose(facts);
        std::fclose(cells);
        return 66;
    }
    const auto& provider = CseVgui1MacFontProviderCandidateV1();
    void* context = provider.CreateContext();
    const bool measured = context && Measure(context, facts, cells);
    provider.DestroyContext(context); // No registration or borrowed face outlives this call.
    const bool stable = FileHash(kCourierPath) == kCourierHash;
    std::fprintf(facts, "final-input-stable\t%d\n", stable);
    const bool factsClosed = std::fclose(facts) == 0;
    const bool cellsClosed = std::fclose(cells) == 0;
    return measured && stable && factsClosed && cellsClosed ? 0 : 1;
}
