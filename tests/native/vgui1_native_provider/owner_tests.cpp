#include "VGui1MacFontProvider.h"
#include "VGui1FontServices.h"
#include "VGUI_Font.h"
#include <array>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {
using namespace cse_vgui1;
using Result = CseVgui1FontResultV1;
struct Model {
    std::vector<unsigned char> bytes;
    std::vector<unsigned char> bold;
    std::map<const void*, std::set<std::string>> owners;
    int lookups = 0;
    int releases = 0;
    bool unavailable = false;
    bool releaseFailed = false;
    bool acquireFailed = false;
};
Model* model = nullptr;
const std::string path = "/owned/字体/regular.ttf";
void Require(bool ok, const char* message) {
    if (!ok) {
        std::fprintf(stderr, "FAILED frozen owner/native provider: %s\n", message);
        std::abort();
    }
}
bool Lookup(void*, FT_LibraryRec_*, const FontRequest& request, NativeFontLocation& result) noexcept {
    ++model->lookups;
    if (!model->unavailable && request.lookupIdentity == "Verdana-Bold") {
        Require(request.weight == 500, "measured Verdana request retains original weight 500");
        result = {path + ".bold", "Verdana-Bold", 0};
        return true;
    }
    if (model->unavailable || request.lookupIdentity != "CSE Owned Raster V1") {
        return false;
    }
    result = {path, "CSEOwnedRasterV1-Regular", 0};
    return true;
}
bool Read(void*, const char* filename, std::vector<unsigned char>& result) noexcept {
    Require(path == filename || path + ".bold" == filename, "exact bound path");
    result = path == filename ? model->bytes : model->bold;
    return true;
}
int Acquire(void*, const void* owner, const char* filename) noexcept {
    if (model->acquireFailed) {
        return 0;
    }
    model->owners[owner].insert(filename);
    return 1;
}
int Release(void*, const void* owner) noexcept {
    ++model->releases;
    if (model->releaseFailed) {
        return 0;
    }
    model->owners.erase(owner);
    return 1;
}
void* CreateContext() noexcept {
    return CreateMacFontContextV1({nullptr, Lookup, Read, Acquire, Release});
}
CseVgui1FontStatusV1 Prepare(vgui::Font& font, Result expected) {
    CseVgui1FontStatusV1 status{};
    status.structSize = sizeof(status);
    std::array<char, 256> diagnostic{};
    const auto result = CseVgui1FontPrepareV1(font.getPlat(), &status, diagnostic.data(), diagnostic.size());
    if (result != expected) {
        std::fprintf(stderr, "prepare=%d diagnostic=%s\n", static_cast<int>(result), diagnostic.data());
    }
    Require(result == expected, "scoped checked preparation");
    return status;
}
Result Bind() {
    const char* files[]{path.c_str()};
    return CseVgui1FontsBindFilesV1(files, 1, nullptr, 0);
}
std::vector<unsigned char> Bitmap() {
    constexpr int glyphs = 256;
    constexpr int cellWidth = 2;
    constexpr int width = glyphs * cellWidth;
    constexpr int height = 2;
    constexpr int run = 128;
    std::vector<unsigned char> bytes(18, 0);
    bytes[2] = 10;
    bytes[12] = static_cast<unsigned char>(width & 255);
    bytes[13] = static_cast<unsigned char>(width >> CHAR_BIT);
    bytes[14] = height;
    bytes[16] = 32;
    for (int row = height - 1; row >= 0; --row) {
        for (int start = 0; start < width; start += run) {
            bytes.push_back(run - 1);
            for (int column = start; column < start + run; ++column) {
                bytes.push_back(0);
                bytes.push_back(0);
                bytes.push_back(0);
                bytes.push_back(row == 0 && column == 'A' * cellWidth + 1 ? 1 : 0);
            }
        }
    }
    return bytes;
}
}
namespace cse_vgui1 {
// Fixture-only link binding. Production still has no qualified factory.
const FontProviderV1& CseVgui1QualifiedFontProviderV1() noexcept {
    static FontProviderV1 provider = CseVgui1MacFontProviderCandidateV1();
    provider.CreateContext = CreateContext;
    return provider;
}
}
namespace vgui {
void CseVgui1SelectFontForSurfaceV1(SurfacePlat*, BaseFontPlat*) {
    std::abort(); // No fixture ever calls the unbound real Surface path.
}
}
int main(int argc, char** argv) {
    Require(argc == 2, "owned input directory");
    Model storage;
    model = &storage;
    FILE* input = std::fopen((std::string(argv[1]) + "/regular.ttf").c_str(), "rb");
    Require(input != nullptr, "authored regular input");
    std::array<unsigned char, 8192> buffer{};
    const auto count = std::fread(buffer.data(), 1, buffer.size(), input);
    Require(count > 0 && count < buffer.size() && std::fclose(input) == 0, "bounded owned input");
    model->bytes.assign(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(count));
    input = std::fopen((std::string(argv[1]) + "/verdana-bold.ttf").c_str(), "rb");
    Require(input != nullptr, "authored Verdana identity fixture");
    const auto boldCount = std::fread(buffer.data(), 1, buffer.size(), input);
    Require(boldCount > 0 && boldCount < buffer.size() && std::fclose(input) == 0, "bounded bold input");
    model->bold.assign(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(boldCount));
    auto bitmap = Bitmap();
    vgui::Font raster("Owned bitmap", bitmap.data(), static_cast<int>(bitmap.size()),
        99, 17, 45.0f, 900, true, true, true, true);
    Require(Prepare(raster, Result::Ready).capabilities != 0 && raster.getTall() == 2
        && model->lookups == 0 && model->owners.empty(), "valid TGA precedes native provider and resource binding");
    bitmap.assign(bitmap.size(), 0);
    int a = 0;
    int b = 0;
    int c = 0;
    raster.getCharABCwide('A', a, b, c);
    Require(a == 0 && b == 2 && c == 0, "owned bitmap width behavior retained");
    vgui::Font font("CSE Owned Raster V1", 20, 0, 0.0f, 400, false, false, false, false);
    Require(Prepare(font, Result::PendingResources).capabilities == 0, "before-bind is an ordinary pending request");
    vgui::Font_Reset();
    model->acquireFailed = true;
    Require(Bind() == Result::RegistrationFailed && model->owners.empty()
        && CseVgui1FontsReleaseFilesV1(nullptr, 0) == Result::FilesReleased,
        "first-acquire failure publishes no batch/token and releases its unused context");
    model->acquireFailed = false;
    Require(Bind() == Result::FilesBound && Prepare(font, Result::Ready).capabilities != 0,
        "retired initial borrower can be prepared through real native face");
    vgui::Font verdana("Verdana Bold", 20, 0, 0.0f, 500, false, false, false, false);
    Require(Prepare(verdana, Result::Ready).capabilities != 0 && verdana.getTall() > 0,
        "original weight500 Verdana-Bold profile realizes through actual owner and provider");
    vgui::Font styled("CSE Owned Raster V1", 20, 20, 30.0f, 600, true, true, true, false);
    Require(Prepare(styled, Result::Ready).capabilities != 0 && styled.getWide() > 0,
        "owner accepts advertised style and geometry capabilities");
    void* platform = font.getPlat();
    const auto ready = Prepare(font, Result::Ready);
    const int attempts = model->lookups;
    Require(Bind() == Result::FilesBound && Prepare(font, Result::Ready).capabilities == ready.capabilities
        && font.getPlat() == platform && model->lookups == attempts, "additive binding preserves ready face identity");
    font.getCharABCwide('@', a, b, c);
    Require(a == 0 && b > 0, "actual owner applies retained negative-A clamp");
    std::array<unsigned char, 40 * 32 * 4> pixels{};
    font.getCharRGBA('A', 3, 5, 40, 32, pixels.data());
    Require(font.getTall() > 0 && font.getWide() > 0, "actual legacy API exposes checked native metrics");
    model->releaseFailed = true;
    Require(CseVgui1FontsReleaseFilesV1(nullptr, 0) == Result::ReleaseFailed && !model->owners.empty(),
        "failed release retains actual native context and token references");
    const int failedAttempts = model->lookups;
    Prepare(font, Result::ReleaseFailed);
    Require(model->lookups == failedAttempts && Bind() == Result::ReleaseFailed,
        "failed release prevents realization and rebind");
    model->releaseFailed = false;
    Require(CseVgui1FontsReleaseFilesV1(nullptr, 0) == Result::FilesReleased && model->owners.empty(),
        "explicit retry releases all owner tokens normally");
    Require(Prepare(font, Result::PendingResources).capabilities == 0, "released old borrower is not ready");
    model->unavailable = true;
    Require(Bind() == Result::FilesBound, "bind resources for unavailable control");
    Prepare(font, Result::Unavailable);
    const int unavailableAttempts = model->lookups;
    Prepare(font, Result::Unavailable);
    Require(model->lookups == unavailableAttempts, "per-catalog failed realization is not repeated");
    model->unavailable = false;
    const std::string additionalPath = path + ".additional";
    const char* additionalFiles[]{additionalPath.c_str()};
    Require(CseVgui1FontsBindFilesV1(additionalFiles, 1, nullptr, 0) == Result::FilesBound,
        "new additive file advances catalog for checked retry; duplicate bind does not");
    Prepare(font, Result::Ready);
    Require(font.getPlat() == platform, "formerly ready borrower identity survives release/rebind");
    Require(CseVgui1FontsReleaseFilesV1(nullptr, 0) == Result::FilesReleased && model->owners.empty(),
        "all contexts released before fixture callback storage dies");
    vgui::Font_Reset(); // Isolate the next lookup generation from the added styled borrowers.
    unsigned char invalid[18]{};
    const int beforeInvalidBitmap = model->lookups;
    vgui::Font malformed("Present invalid TGA", invalid, sizeof(invalid),
        20, 0, 0.0f, 400, false, false, false, false);
    Require(malformed.getPlat() == nullptr && malformed.getId() == -1
        && CseVgui1FontsPreflightV1(nullptr, 0) == Result::Unavailable
        && model->lookups == beforeInvalidBitmap, "malformed present TGA cannot trigger a font substitution");
    std::puts("PASS actual frozen owner with authored native face; Surface/GPU/UI not exercised");
}
