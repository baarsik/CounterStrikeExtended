// PREPARED ONLY. Not registered with CTest. Explicit reviewed execution needed.
#include "VGui1MacFontProvider.h"
#include "VGui1NativeFontFace.h"
#include "MacFontDiscovery.h"
#include "digest.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <ft2build.h>
#include FT_FREETYPE_H

namespace {
using namespace cse_vgui1;
constexpr std::size_t kCanvasBytes = 256 * 256 * 4;
constexpr int kCanvasSide = 256;
constexpr int kPenX = 3;
constexpr int kPenY = 5;
constexpr const char* kGameMarlettSha256 = "3b6192eca4451835f4d634b0b6b0841da321ec65c1f3f1aa22d186da45787b9c";

std::string FileHash(const char* path) {
    FILE* input = std::fopen(path, "rb");
    if (!input) {
        return {};
    }
    Digest digest;
    std::array<unsigned char, 8192> buffer{};
    for (;;) {
        const std::size_t count = std::fread(buffer.data(), 1, buffer.size(), input);
        digest.Update(buffer.data(), count);
        if (count < buffer.size()) {
            const bool ok = !std::ferror(input);
            const bool closed = std::fclose(input) == 0;
            return ok && closed ? digest.Finish() : std::string{};
        }
    }
}

FontRequest Request(const char* name, int tall, int weight, bool symbol = false) {
    FontRequest request;
    request.requestedName = name;
    request.lookupIdentity = name;
    request.resolutionProfile = "exact-request-v1";
    request.tall = tall;
    request.weight = weight;
    request.flags = symbol ? FontSymbol : 0U;
    return request;
}

bool Measure(const FontRequest& request, void* context, FT_Library lookupLibrary,
    const std::vector<std::string>& files, FILE* metrics, FILE* cells) {
    // Pin the separately discovered input facts, then call the actual default
    // provider (which performs its own lookup/read). Verify PS/index agreement
    // and the discovered file hash before/after. This is not byte access to the
    // face's private copy, nor a claim about a concurrently replaced file.
    cse::fonts::FontFile source;
    const bool found = cse::fonts::FindMacFontFile(lookupLibrary, request.lookupIdentity.c_str(),
        request.weight, false, source);
    const std::string before = found ? FileHash(source.path.c_str()) : std::string{};
    int error = 0;
    std::string diagnostic;
    const auto& provider = CseVgui1MacFontProviderCandidateV1();
    auto face = provider.Realize(context, request, files, error, diagnostic);
    auto* native = dynamic_cast<legacy::NativeFontFace*>(face.get());
    const std::string after = found ? FileHash(source.path.c_str()) : std::string{};
    const bool stable = before == after;
    std::fprintf(metrics, "request\t%s\t%d\t%d\t%u\tfound=%d\tready=%d\terror=%d\t%s\n",
        request.requestedName.c_str(), request.tall, request.weight, request.flags,
        found, face != nullptr, error, diagnostic.c_str());
    if (found) {
        std::fprintf(metrics, "input\t%s\t%s\t%ld\t%s\tstable=%d\n", source.path.c_str(),
            source.postScriptName.c_str(), source.faceIndex, before.c_str(), stable);
    }
    if (!face) {
        return error != 0 && !diagnostic.empty() && stable;
    }
    if (!native || !found || before.empty() || !stable || native->PostScriptName() != source.postScriptName
        || native->FaceIndex() != source.faceIndex) {
        return false;
    }
    const auto value = native->DetailedMetrics();
    std::fprintf(metrics, "metrics\t%s\t%s\t%ld\t%u\t%d\t%d\t%d\t%d\t%d\t%d\t%u\n",
        request.requestedName.c_str(), native->PostScriptName().c_str(), native->FaceIndex(),
        native->IntrinsicWeight(), value.height, value.ascent, value.descent, value.internalLeading,
        value.averageWidth, value.maximumWidth, native->Metrics().capabilities);
    if (value.height + kPenY > kCanvasSide || value.maximumWidth + kPenX > kCanvasSide) {
        return false;
    }
    std::array<unsigned char, kCanvasBytes> output{};
    for (unsigned int byte = 0; byte < 256; ++byte) {
        int a = 0;
        int b = 0;
        int c = 0;
        if (!native->RawAbc(byte, a, b, c)) {
            return false;
        }
        output.fill(0x5a);
        if (!native->Raster(byte, kPenX, kPenY, kCanvasSide, kCanvasSide, output.data())) {
            return false;
        }
        Digest digest;
        digest.Update(output.data(), output.size());
        const std::string hash = digest.Finish();
        std::fprintf(cells, "%s\t%d\t%d\t%u\t%u\t%d\t%d\t%d\t%s\n", request.requestedName.c_str(),
            request.tall, request.weight, byte, native->GlyphIndex(byte), a, b, c, hash.c_str());
    }
    return true;
}
}

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--describe") == 0) {
        std::puts("PREPARED NOT RUN: fixed seven family tuples OR one pinned bound game symbol resource; "
            "process registration only; metrics/IDs/hashes only; no UI/preferences/font-program output");
        return 0;
    }
    const bool families = argc == 3 && std::strcmp(argv[1], "--families") == 0;
    const bool resource = argc == 4 && std::strcmp(argv[1], "--bound-symbol") == 0;
    if (!families && !resource) {
        return 64;
    }
    const std::string directory = argv[resource ? 3 : 2];
    FILE* metrics = std::fopen((directory + "/metrics.tsv").c_str(), "wb");
    FILE* cells = std::fopen((directory + "/cells.tsv").c_str(), "wb");
    if (!metrics || !cells) {
        if (metrics) {
            std::fclose(metrics);
        }
        if (cells) {
            std::fclose(cells);
        }
        return 65;
    }
    const auto& provider = CseVgui1MacFontProviderCandidateV1();
    void* context = provider.CreateContext();
    FT_Library lookupLibrary = nullptr;
    if (!context || FT_Init_FreeType(&lookupLibrary) != 0) {
        provider.DestroyContext(context);
        std::fclose(metrics);
        std::fclose(cells);
        return 66;
    }
    bool ok = true;
    bool released = true;
    int ownerToken = 0;
    if (families) {
        const FontRequest requests[]{Request("Arial", 20, 400), Request("Arial", 12, 400),
            Request("Arial", 32, 700), Request("Arial Narrow Bold", 36, 0),
            Request("FixedSys", 18, 400), Request("Courier", 18, 400), Request("Marlett", 16, 0, true)};
        for (const auto& request : requests) {
            ok = Measure(request, context, lookupLibrary, {}, metrics, cells) && ok;
        }
    } else {
        const char* path = argv[2];
        const std::string before = FileHash(path);
        if (before != kGameMarlettSha256 || provider.RegisterFile(context, &ownerToken, path) != 0) {
            std::fprintf(metrics, "bound-resource-rejected\t%s\n", before.c_str());
            ok = false;
        } else {
            ok = Measure(Request("Marlett", 16, 0, true), context, lookupLibrary, {path}, metrics, cells);
            ok = before == FileHash(path) && ok;
        }
        released = provider.ReleaseOwner(context, &ownerToken) == 0;
        std::fprintf(metrics, "released\t%d\n", released);
    }
    FT_Done_FreeType(lookupLibrary);
    if (released) {
        provider.DestroyContext(context);
    } else {
        // ReleaseFailed retains context/token/module through process exit.
        // Never force-unregister, unload or claim success after a failed release.
        std::fputs("PREPARED probe release failed; context retained until process exit\n", stderr);
    }
    const bool metricsClosed = std::fclose(metrics) == 0;
    const bool cellsClosed = std::fclose(cells) == 0;
    return ok && released && metricsClosed && cellsClosed ? 0 : 1;
}
