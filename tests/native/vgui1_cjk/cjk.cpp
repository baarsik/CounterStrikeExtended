#include "VGui1MacFontProvider.h"
#include "VGui1NativeFontFace.h"
#include "VGui1FontServices.h"
#include "VGUI_Font.h"
#include "MacFontDiscovery.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <map>
#include <string>
#include <vector>

namespace cse_vgui1 {
const FontProviderV1& CseVgui1QualifiedFontProviderV1() noexcept {
    return CseVgui1MacFontProviderCandidateV1();
}
}
namespace vgui {
void CseVgui1SelectFontForSurfaceV1(SurfacePlat*, BaseFontPlat*) { std::abort(); }
}
namespace {
using namespace cse_vgui1;
std::vector<unsigned char> Raster(RealizedFace& face, unsigned int character) {
    const auto metrics = face.Metrics();
    int a = 0;
    int b = 0;
    int c = 0;
    if (!face.RawAbc(character, a, b, c)) {
        std::fprintf(stderr, "missing actual native glyph U+%04X\n", character);
        std::abort();
    }
    assert(b > 0 && b <= metrics.maximumWidth);
    std::vector<unsigned char> pixels(static_cast<std::size_t>(metrics.maximumWidth) * metrics.height * 4, 0);
    assert(face.Raster(character, 0, 0, metrics.maximumWidth, metrics.height, pixels.data()));
    assert(std::any_of(pixels.begin(), pixels.end(), [](unsigned char value) { return value != 0; }));
    static int printed = 0;
    if (printed++ < 12) {
        std::printf("U+%04X abc=%d,%d,%d cell=%dx%d\n", character, a, b, c, metrics.maximumWidth, metrics.height);
    }
    return pixels;
}
}
namespace {
int reads = 0;
int scalarLookups = 0;
std::map<std::string, int> programReads;
std::size_t bytesRead = 0;
bool CountedLookup(void*, FT_LibraryRec_* library, const FontRequest& request,
    NativeFontLocation& output) noexcept {
    cse::fonts::FontFile file;
    if (!cse::fonts::FindMacFontFile(library, request.lookupIdentity.c_str(), request.weight, false, file)) {
        return false;
    }
    output = {file.path, file.postScriptName, file.faceIndex};
    return true;
}
bool CountedRead(void*, const char* path, std::vector<unsigned char>& bytes) noexcept {
    ++reads;
    std::ifstream input(path, std::ios::binary);
    bytes.assign(std::istreambuf_iterator<char>(input), {});
    ++programReads[path];
    bytesRead += bytes.size();
    return !bytes.empty();
}
int NoAcquire(void*, const void*, const char*) noexcept { return 1; }
int NoRelease(void*, const void*) noexcept { return 1; }
bool CountedScalar(void*, FT_LibraryRec_* library, const FontRequest& request,
    unsigned int scalar, NativeFontLocation& output) noexcept {
    ++scalarLookups;
    const auto unit = static_cast<std::uint16_t>(scalar);
    cse::fonts::FontFile file;
    if (!cse::fonts::FindMacFontFallback(library, request.lookupIdentity.c_str(), request.weight, false,
        &unit, 1, file)) {
        return false;
    }
    output = {file.path, file.postScriptName, file.faceIndex};
    return true;
}
}

int main(int argc, char** argv) {
    assert(argc >= 2);
    const auto& provider = CseVgui1MacFontProviderCandidateV1();
    void* context = provider.CreateContext();
    assert(context);
    FontRequest request;
    request.requestedName = "Arial";
    request.lookupIdentity = "Arial";
    request.tall = 16;
    int error = 0;
    std::string diagnostic;
    auto face = provider.Realize(context, request, {}, error, diagnostic);
    assert(face);
    const auto original = face->Metrics();
    const auto latin = Raster(*face, 'A');
    const auto missing = Raster(*face, 0);
    std::vector<std::vector<unsigned char>> reference;
    // Actual official SC/TC, Japanese, Korean, Russian and Polish characters.
    const unsigned int glyphs[]{0x5f97, 0x5206, 0x6b7b, 0x8a9e, 0x97f3, 0x70b9, 0xac8c, 0xc810, 0x0416, 0x0142};
    for (unsigned int glyph : glyphs) {
        auto pixels = Raster(*face, glyph);
        assert(pixels != missing);
        reference.push_back(std::move(pixels));
    }
    for (int revision = 0; revision < 80; ++revision) {
        for (std::size_t index = 0; index < reference.size(); ++index) {
            assert(Raster(*face, glyphs[index]) == reference[index]);
        }
    }
    assert(Raster(*face, 'A') == latin);
    assert(face->Metrics().height == original.height && face->Metrics().maximumWidth == original.maximumWidth);
    face.reset();
    provider.DestroyContext(context);

    // Actual fallback files/metrics through observable native discovery I/O.
    // Warm repeats must reuse owned donor programs rather than copy them again.
    context = CreateMacFontContextV1({nullptr, CountedLookup, CountedRead, NoAcquire, NoRelease, CountedScalar});
    assert(context);
    face = provider.Realize(context, request, {}, error, diagnostic);
    for (unsigned int glyph : glyphs) {
        Raster(*face, glyph);
    }
    const int warmReads = reads;
    const int warmLookups = scalarLookups;
    assert(warmReads > 1 && warmLookups > 0 && warmLookups < 10);
    for (int revision = 0; revision < 80; ++revision) {
        for (unsigned int glyph : glyphs) {
            Raster(*face, glyph);
        }
    }
    assert(reads == warmReads && scalarLookups == warmLookups);
    std::printf("warm cache: reads=%d discovery=%d unchanged across80 revisions\n", reads, scalarLookups);
    std::vector<std::unique_ptr<RealizedFace>> retained;
    for (int pixels : {12, 14, 16, 18, 20, 23, 27}) {
        for (int weight : {0, 700}) {
            for (std::uint32_t flags : {0U, FontItalic}) {
                auto varied = request;
                varied.tall = pixels;
                varied.weight = weight;
                varied.flags = flags;
                auto realized = provider.Realize(context, varied, {}, error, diagnostic);
                assert(realized);
                for (unsigned int glyph : glyphs) {
                    Raster(*realized, glyph);
                }
                retained.push_back(std::move(realized));
            }
        }
    }
    for (const auto& program : programReads) {
        assert(program.second == 1);
    }
    std::printf("shared programs: %zu files, %zu immutable bytes across%zu retained size/style faces\n",
        programReads.size(), bytesRead, retained.size() + 1);
    retained.clear();
    face.reset();
    provider.DestroyContext(context);

    // Direct shared-program lifetime uses the installed regular Arial program.
    std::ifstream arial("/System/Library/Fonts/Supplemental/Arial.ttf", std::ios::binary);
    auto program = std::make_shared<const std::vector<unsigned char>>(
        std::istreambuf_iterator<char>(arial), std::istreambuf_iterator<char>());
    std::weak_ptr<const std::vector<unsigned char>> weakProgram = program;
    legacy::Result nativeResult{};
    auto sharedFirst = legacy::NativeFontFace::CreateShared(program, 0, "ArialMT", request,
        nativeResult, error, diagnostic);
    auto sharedSecond = legacy::NativeFontFace::CreateShared(program, 0, "ArialMT", request,
        nativeResult, error, diagnostic);
    assert(sharedFirst && sharedSecond && weakProgram.use_count() == 3);
    program.reset();
    Raster(*sharedFirst, 'A');
    sharedFirst.reset();
    assert(weakProgram.use_count() == 1);
    Raster(*sharedSecond, 'A');
    sharedSecond.reset();
    assert(weakProgram.expired());

    assert(CseVgui1FontsBindFilesV1(nullptr, 0, nullptr, 0) == CseVgui1FontResultV1::FilesBound);
    for (int bitmapIndex = 1; bitmapIndex < argc; ++bitmapIndex) {
        std::ifstream input(argv[bitmapIndex], std::ios::binary);
        std::vector<unsigned char> bitmap((std::istreambuf_iterator<char>(input)), {});
        assert(!bitmap.empty());
        vgui::Font font("Arial", bitmap.data(), static_cast<int>(bitmap.size()), 16, 0, 0.0f,
            0, false, false, false, false);
        const int height = font.getTall();
        const int width = font.getWide();
        for (int rebind = 0; rebind < 4; ++rebind) {
            for (unsigned int glyph : glyphs) {
                int a = 0;
                int b = 0;
                int c = 0;
                font.getCharABCwide(static_cast<int>(glyph), a, b, c);
                assert(b > 0);
                const int canvasWidth = (std::max)(width, b);
                std::vector<unsigned char> pixels(static_cast<std::size_t>(canvasWidth) * height * 4, 0);
                font.getCharRGBA(static_cast<int>(glyph), 0, 0, canvasWidth, height, pixels.data());
                assert(std::any_of(pixels.begin(), pixels.end(), [](unsigned char value) { return value != 0; }));
            }
            assert(font.getTall() == height && font.getWide() == width);
            assert(CseVgui1FontsReleaseFilesV1(nullptr, 0) == CseVgui1FontResultV1::FilesReleased);
            assert(CseVgui1FontsBindFilesV1(nullptr, 0, nullptr, 0) == CseVgui1FontResultV1::FilesBound);
            CseVgui1FontStatusV1 status{};
            status.structSize = sizeof(status);
            assert(CseVgui1FontPrepareV1(font.getPlat(), &status, nullptr, 0) == CseVgui1FontResultV1::Ready);
            assert(CseVgui1FontsPreflightV1(nullptr, 0) == CseVgui1FontResultV1::Ready);
        }
        std::printf("bitmap cell=%dx%d, four release/rebind/retained-font preparations: PASS\n", width, height);
    }
    assert(CseVgui1FontsReleaseFilesV1(nullptr, 0) == CseVgui1FontResultV1::FilesReleased);
    std::puts("actual CoreText CJK native provider + retained bitmap bridge: PASS");
}
