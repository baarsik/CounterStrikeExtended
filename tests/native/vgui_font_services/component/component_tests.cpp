#include "VGui1FontServices.h"
#include "VGui1FontPlatform.h"
#include "VGUI_Font.h"
#include "fake_provider.h"
#include <array>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace {
using Result = CseVgui1FontResultV1;
using State = CseVgui1FontStateV1;
constexpr std::size_t kDiagnosticBytes = 512;
constexpr std::size_t kChannels = 4;
static_assert(sizeof(vgui::Font) == 32, "Existing native Font fields/virtual layout size stays unchanged");
std::array<char, kDiagnosticBytes> diagnostic{};

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s (%s)\n", message, diagnostic.data());
        std::abort();
    }
}

Result Preflight() {
    return CseVgui1FontsPreflightV1(diagnostic.data(), diagnostic.size());
}

CseVgui1FontStatusV1 Query(vgui::Font& font, Result expected) {
    CseVgui1FontStatusV1 status{};
    status.structSize = sizeof(status);
    Require(CseVgui1FontQueryV1(font.getPlat(), &status, diagnostic.data(), diagnostic.size()) == expected,
        "queried font state");
    return status;
}

Result Bind(const std::vector<std::string>& paths) {
    std::vector<const char*> borrowed;
    for (const auto& path : paths) {
        borrowed.push_back(path.c_str());
    }
    return CseVgui1FontsBindFilesV1(borrowed.data(), static_cast<std::uint32_t>(borrowed.size()),
        diagnostic.data(), diagnostic.size());
}

void ReleaseScene() {
    Require(CseVgui1FontsReleaseFilesV1(diagnostic.data(), diagnostic.size()) == Result::FilesReleased,
        "explicit stopped-consumer release");
    vgui::Font_Reset();
}

void WriteModel(const std::string& path, const char* name) {
    FILE* output = std::fopen(path.c_str(), "wb");
    Require(output != nullptr, "owned model file open");
    const std::size_t bytes = std::strlen(name);
    Require(std::fwrite(name, 1, bytes, output) == bytes && std::fclose(output) == 0, "owned model file bytes");
}

void PendingAndLateBinding(const std::string& model) {
    Require(Preflight() == Result::Ready && FakeFontSnapshot().contextsCreated == 0,
        "empty preflight does not invent a bound provider context");
    vgui::Font font("Owned Face", 20, 7, 12.375f, 600, true, true, true, false);
    Query(font, Result::PendingResources);
    Require(Preflight() == Result::PendingResources && FakeFontSnapshot().realizations == 0,
        "ordinary pending preflight does not expose fake metrics");
    Require(Bind({}) == Result::FilesBound, "explicit empty resource binding");
    Require(Preflight() == Result::Unavailable, "missing exact identity is ordinary unavailable");
    const int failedAttempts = FakeFontSnapshot().realizations;
    Require(Preflight() == Result::Unavailable && FakeFontSnapshot().realizations == failedAttempts,
        "unavailable attempts are cached only for this catalog generation");
    std::string temporary = model;
    const char* borrowed[]{temporary.c_str()};
    Require(CseVgui1FontsBindFilesV1(borrowed, 1, diagnostic.data(), diagnostic.size()) == Result::FilesBound,
        "bind a complete UTF8 path");
    temporary.assign("caller storage replaced");
    Query(font, Result::PendingRealization);
    Require(Preflight() == Result::Ready, "late binding retries the same platform request");
    Query(font, Result::Ready);
    Require(reinterpret_cast<std::uintptr_t>(font.getPlat()) > UINT32_MAX,
        "actual platform identity uses high native pointer bits");
    const auto request = FakeFontLastRequest();
    Require(request.requestedName == "Owned Face" && request.tall == 20 && request.wide == 7
        && request.rotation == 12.375f && request.rotationTenths == 123 && request.weight == 600
        && request.flags == (cse_vgui1::FontItalic | cse_vgui1::FontUnderline | cse_vgui1::FontStrikeout),
        "full immutable request is forwarded to the distinct legacy provider profile");
    int a = 0;
    int b = 0;
    int c = 0;
    font.getCharABCwide('A', a, b, c);
    Require(a == 0 && b == 4 && c == 1 && font.getTall() == 5 && font.getWide() == 6,
        "legacy ABC clamp and provider metrics");
    int textWide = 0;
    int textTall = 0;
    font.getTextSize("A\nBB", textWide, textTall);
    Require(textWide == 10 && textTall == 10, "unchanged common byte-string/newline measurement path");
    std::array<unsigned char, 8 * 8 * kChannels> rgba{};
    font.getCharRGBA('A', 2, 1, 8, 8, rgba.data());
    const unsigned char expected[]{17, 91, 203, 78};
    Require(std::memcmp(rgba.data() + (8 + 2) * kChannels, expected, sizeof(expected)) == 0
        && rgba[0] == 0, "owned legacy RGB/alpha and nonzero pen offset");
    font.getCharABCwide(-23, a, b, c);
    Require(FakeFontLastByte() == 233, "sign-extended byte callers are normalized within the byte domain");
    vgui::SurfacePlat surface;
    font.getPlat()->drawSetTextFont(&surface);
    Require(surface.selected == font.getPlat(), "required Surface binding records selected platform");
    surface.selected->getCharRGBA('A', 0, 0, 8, 8, rgba.data());
    vgui::Font duplicate("OWNED FACE", 20, 7, 12.375f, 600, true, true, true, false);
    Require(duplicate.getPlat() == font.getPlat() && duplicate.getId() == -1,
        "reference cache sharing and inherited public ID-1 behavior");
    vgui::Font copy("Owned Face", 20, 7, 12.375f, 600, true, true, true, false);
    copy = font;
    vgui::Font_Reset();
    vgui::Font fresh("Owned Face", 20, 7, 12.375f, 600, true, true, true, false);
    Require(fresh.getPlat() != font.getPlat() && fresh.getId() > font.getId() && copy.getTall() == 5
        && font.getTall() == 5 && fresh.getTall() == 5,
        "reset retires lookup while Ready borrowers and default copies remain stable");
    const auto generation = Query(font, Result::Ready).catalogGeneration;
    const int registrations = FakeFontSnapshot().registrations;
    Require(Bind({model, model}) == Result::FilesBound && FakeFontSnapshot().registrations == registrations
        && Query(font, Result::Ready).catalogGeneration == generation,
        "repeated existing file bind is idempotent without replacing Ready faces");
    ReleaseScene();
}

void RequestAfterEmptyPreflight(const std::string& model) {
    Require(Bind({model}) == Result::FilesBound && Preflight() == Result::Ready,
        "earlier empty preflight with a real binding");
    const int attempts = FakeFontSnapshot().realizations;
    vgui::Font font("Owned Face", 17, 0, 0.0f, 400, false, false, false, false);
    Require(Query(font, Result::Ready).state == State::Ready && font.getTall() == 5
        && FakeFontSnapshot().realizations == attempts + 1,
        "new request is checked/realized after earlier empty preflight before its first metric use");
    ReleaseScene();
}

void BindingFailureAndRetry(const std::string& first, const std::string& second, const std::string& bad) {
    Require(Bind({first}) == Result::FilesBound, "first committed resource");
    vgui::Font ready("Owned Face", 20, 0, 0.0f, 400, false, false, false, false);
    Query(ready, Result::Ready);
    const auto generation = Query(ready, Result::Ready).catalogGeneration;
    FakeFontFailRegister(FakeFontSnapshot().registrations + 2);
    Require(Bind({second, bad}) == Result::RegistrationFailed && FakeFontLastReleaseLiveFaces() > 0,
        "rollback releases only staged ownership while old Ready faces stay alive");
    Require(Query(ready, Result::Ready).catalogGeneration == generation && ready.getTall() == 5,
        "failed binding does not publish a new generation or replace a Ready face");
    vgui::Font pending("Owned New", 20, 0, 0.0f, 400, false, false, false, false);
    Query(pending, Result::Unavailable);
    FakeFontFailRegister(0);
    Require(Bind({second}) == Result::FilesBound && Preflight() == Result::Ready,
        "successful retry publishes staged identity and resolves prior unavailability");
    const auto beforeRelease = FakeFontSnapshot();
    FakeFontFailRelease(3);
    for (int attempt = 0; attempt < 2; ++attempt) {
        Require(CseVgui1FontsReleaseFilesV1(diagnostic.data(), diagnostic.size()) == Result::ReleaseFailed,
            "ordinary release failure retains ownership/context for retry");
    }
    Require(FakeFontSnapshot().contextsDestroyed == beforeRelease.contextsDestroyed
        && FakeFontSnapshot().liveFaces == 0 && FakeFontLastReleaseLiveFaces() == 0,
        "all faces close before release; failed owner/context remains retained");
    Require(Bind({first}) == Result::ReleaseFailed, "root must refuse rebind while release is failed");
    FakeFontFailRelease(0);
    Require(CseVgui1FontsReleaseFilesV1(diagnostic.data(), diagnostic.size()) == Result::FilesReleased
        && FakeFontSnapshot().contextsDestroyed == beforeRelease.contextsDestroyed + 1,
        "successful release retry finishes context only after owners detach");
    ReleaseScene();
}

std::vector<unsigned char> BitmapInput() {
    constexpr int kGlyphs = 256;
    constexpr int kCellWide = 2;
    constexpr int kHeight = 2;
    constexpr int kWidth = kGlyphs * kCellWide;
    constexpr int kPacketPixels = 128;
    std::vector<unsigned char> bytes(18, 0);
    bytes[2] = 10;
    bytes[12] = static_cast<unsigned char>(kWidth & 255);
    bytes[13] = static_cast<unsigned char>(kWidth >> 8);
    bytes[14] = kHeight;
    bytes[16] = kChannels * CHAR_BIT;
    for (int y = kHeight - 1; y >= 0; --y) {
        for (int start = 0; start < kWidth; start += kPacketPixels) {
            bytes.push_back(kPacketPixels - 1);
            for (int x = start; x < start + kPacketPixels; ++x) {
                const bool set = y == 0 && x == 'A' * kCellWide + 1;
                bytes.push_back(0);
                bytes.push_back(0);
                bytes.push_back(0);
                bytes.push_back(set ? 1 : 0);
            }
        }
    }
    return bytes;
}

void BitmapOwnership() {
    auto bytes = BitmapInput();
    vgui::Font font("Owned bitmap", bytes.data(), static_cast<int>(bytes.size()), 99, 17, 45.0f, 900,
        true, true, true, true);
    bytes.assign(bytes.size(), 0);
    Require(Query(font, Result::Ready).state == State::Ready && font.getTall() == 2 && font.getWide() == 2,
        "actual bitmap decoder copies input and retains original style-independent dimensions");
    int a = 0;
    int b = 0;
    int c = 0;
    font.getCharABCwide('A', a, b, c);
    Require(a == 0 && b == 2 && c == 0, "rightmost bitmap pixel plus one");
    font.getCharABCwide(' ', a, b, c);
    Require(b == 1, "bitmap space cell-width quarter plus one");
    font.getCharABCwide(-1, a, b, c);
    Require(b == 1, "retained bitmap negative index clamp");
    std::array<unsigned char, 8 * 8 * kChannels> rgba{};
    font.getCharRGBA('A', 3, 2, 8, 8, rgba.data());
    const auto pixel = (2 * 8 + 4) * kChannels;
    Require(rgba[pixel] == 255 && rgba[pixel + 1] == 255 && rgba[pixel + 2] == 255
        && rgba[pixel + 3] == 255 && rgba[0] == 0,
        "actual bitmap any-channel mask and nonzero atlas pen offsets");
    vgui::Font_Reset();
    Require(font.getTall() == 2, "bitmap borrowers also survive lookup reset");
    unsigned char invalid[18]{};
    vgui::Font failed("Invalid bitmap", invalid, sizeof(invalid), 20, 0, 0.0f, 400,
        false, false, false, false);
    Require(failed.getPlat() == nullptr && failed.getId() == -1 && Preflight() == Result::Unavailable,
        "malformed bitmap preserves explicit failure, without successful fallback");
    ReleaseScene();
}

void ProfileAndCapabilityControls(const std::string& directory) {
    const std::array<const char*, 3> identities{{"ArialMT", "Wingdings-Regular", "Verdana-Bold"}};
    std::vector<std::string> paths;
    for (std::size_t index = 0; index < identities.size(); ++index) {
        paths.push_back(directory + "/profile-" + std::to_string(index));
        WriteModel(paths.back(), identities[index]);
    }
    Require(Bind(paths) == Result::FilesBound, "owned legacy resolution models");
    vgui::Font fallback("Arial Narrow Bold", 20, 0, 0.0f, 400, false, false, false, false);
    Query(fallback, Result::Ready);
    Require(FakeFontLastRequest().lookupIdentity == "ArialMT"
        && FakeFontLastRequest().resolutionProfile != "exact-request-v1",
        "measured GDI fallback is explicit and does not reject differing requested/selected names");
    vgui::Font symbol("Arial", 20, 0, 0.0f, 400, false, false, false, true);
    Query(symbol, Result::Ready);
    Require(FakeFontLastRequest().lookupIdentity == "Wingdings-Regular", "measured charset resolution");
    vgui::Font bold("Verdana Bold", 20, 0, 0.0f, 500, false, false, false, false);
    Query(bold, Result::Ready);
    Require(FakeFontLastRequest().lookupIdentity == "Verdana-Bold", "measured exact full-face resolution");
    vgui::Font unknown("Unproven unknown family", 20, 0, 0.0f, 400, false, false, false, false);
    Query(unknown, Result::Unavailable);
    Require(FakeFontLastRequest().lookupIdentity == "Unproven unknown family",
        "unknown family is not silently replaced by arbitrary native fallback");
    ReleaseScene();
}

void RejectedCapabilities(const std::string& model) {
    Require(Bind({model}) == Result::FilesBound, "resource bind for unsupported-style control");
    FakeFontDropCapabilities(cse_vgui1::CapabilityRotation);
    vgui::Font incomplete("Owned Face", 20, 0, 12.375f, 400, false, false, false, false);
    const auto status = Query(incomplete, Result::Unavailable);
    Require(status.nativeError != 0 && status.capabilities == 0 && FakeFontSnapshot().liveFaces == 0,
        "rejected style never exposes unusable provider capabilities or keeps its face alive");
    FakeFontDropCapabilities(0);
    ReleaseScene();
    Require(Bind({model}) == Result::FilesBound, "resource bind for invalid metric control");
    FakeFontInvalidMetrics(true);
    vgui::Font invalid("Owned Face", 20, 0, 0.0f, 400, false, false, false, false);
    Require(Query(invalid, Result::Unavailable).capabilities == 0 && FakeFontSnapshot().liveFaces == 0,
        "a provider with invalid metrics cannot claim Ready");
    FakeFontInvalidMetrics(false);
    ReleaseScene();
}

void TruncatedBitmap() {
    auto bytes = BitmapInput();
    bytes.resize(18);
    vgui::Font font("Truncated bitmap", bytes.data(), static_cast<int>(bytes.size()), 20, 0, 0.0f, 400,
        false, false, false, false);
    Require(font.getPlat() == nullptr && font.getId() == -1 && Preflight() == Result::Unavailable,
        "truncated bitmap cannot publish a successful all-zero decoded font");
    ReleaseScene();
    constexpr unsigned char kMaximumByte = 255;
    bytes[12] = kMaximumByte;
    bytes[13] = kMaximumByte;
    bytes[14] = kMaximumByte;
    bytes[15] = kMaximumByte;
    vgui::Font overflow("Unrepresentable bitmap", bytes.data(), static_cast<int>(bytes.size()),
        20, 0, 0.0f, 400, false, false, false, false);
    Require(overflow.getPlat() == nullptr && Preflight() == Result::Unavailable,
        "large header is rejected before the retained signed-int decoder allocation");
    ReleaseScene();
}

float FromBits(std::uint32_t bits) {
    float value = 0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void ApiAndRequestControls(const std::string& model) {
    const auto before = FakeFontSnapshot();
    const char* relative[]{"owned-face"};
    const char* invalidUtf8[]{"/owned/\xed\xa0\x80"};
    Require(CseVgui1FontsBindFilesV1(nullptr, 1, nullptr, 0) == Result::InvalidArgument
        && CseVgui1FontsBindFilesV1(relative, 1, nullptr, 0) == Result::InvalidArgument
        && CseVgui1FontsBindFilesV1(invalidUtf8, 1, nullptr, 0) == Result::InvalidArgument
        && CseVgui1FontsPreflightV1(nullptr, 1) == Result::InvalidArgument
        && CseVgui1FontsReleaseFilesV1(nullptr, 1) == Result::InvalidArgument
        && FakeFontSnapshot().contextsCreated == before.contextsCreated,
        "invalid API inputs fail before context/registration side effects");
    char single = 'X';
    Require(CseVgui1FontsBindFilesV1(relative, 1, &single, 1) == Result::InvalidArgument && single == 0,
        "one-byte diagnostic is always terminated");
    CseVgui1FontStatusV1 poisoned;
    std::memset(&poisoned, 0xa5, sizeof(poisoned));
    auto saved = poisoned;
    Require(CseVgui1FontQueryV1(&single, &poisoned, nullptr, 0) == Result::InvalidArgument
        && std::memcmp(&saved, &poisoned, sizeof(saved)) == 0,
        "invalid status version leaves caller output untouched");
    poisoned.structSize = sizeof(poisoned);
    saved = poisoned;
    Require(CseVgui1FontQueryV1(&single, &poisoned, nullptr, 0) == Result::InvalidHandle
        && std::memcmp(&saved, &poisoned, sizeof(saved)) == 0,
        "foreign platform address is rejected without dereference or output mutation");
    Require(Bind({model}) == Result::FilesBound, "valid request-control binding");
    vgui::Font valid("Owned Face", 20, 0, 0.0f, 400, false, false, false, false);
    Query(valid, Result::Ready);
    const int attempts = FakeFontSnapshot().realizations;
    constexpr std::uint32_t kUnsupportedRotationBits[]{0x7fc00000, 0x7f800000, 0xff800000, 0x7f7fffff};
    for (const auto bits : kUnsupportedRotationBits) {
        vgui::Font unsupported("Owned Face", 20, 0, FromBits(bits), 400, false, false, false, false);
        Require(Query(unsupported, Result::Unavailable).nativeError != 0
            && unsupported.getPlat() != valid.getPlat(), "invalid rotation cannot hit a Ready cached request");
    }
    vgui::Font badName("\xf4\x90\x80\x80", 20, 0, 0.0f, 400, false, false, false, false);
    Query(badName, Result::Unavailable);
    vgui::Font nullName(nullptr, 20, 0, 0.0f, 400, false, false, false, false);
    Query(nullName, Result::Unavailable);
    Require(FakeFontSnapshot().realizations == attempts,
        "invalid UTF8/scalar/rotation requests do not dispatch native font realization");
    vgui::Font negative("Owned Face", -20, -7, -12.375f, 600, false, false, false, false);
    Query(negative, Result::Ready);
    Require(FakeFontLastRequest().rotationTenths == -123 && FakeFontLastRequest().wide == -7
        && FakeFontLastRequest().tall == -20, "valid negative legacy dimensions and quantized angle survive");
    ReleaseScene();
}

void FailedRollbackRetainsOwner(const std::string& first, const std::string& second) {
    FakeFontFailRegister(FakeFontSnapshot().registrations + 2);
    FakeFontFailRelease(1);
    Require(Bind({first, second}) == Result::RegistrationFailed,
        "failed staged registration with failed rollback remains an ordinary error");
    const int destroyed = FakeFontSnapshot().contextsDestroyed;
    Require(Bind({first}) == Result::ReleaseFailed && Preflight() == Result::Ready
        && FakeFontSnapshot().contextsDestroyed == destroyed,
        "unpublished failed batch keeps context and prevents rebinding even with no current requests");
    FakeFontFailRegister(0);
    FakeFontFailRelease(0);
    ReleaseScene();
}

void ContextFailure(const std::string& model) {
    FakeFontFailCreate(true);
    const auto before = FakeFontSnapshot();
    Require(Bind({model}) == Result::RegistrationFailed
        && FakeFontSnapshot().contextsCreated == before.contextsCreated
        && FakeFontSnapshot().registrations == before.registrations,
        "context creation failure cannot publish a bound resource catalog");
    FakeFontFailCreate(false);
    Require(Bind({model}) == Result::FilesBound, "context creation can be explicitly retried");
    vgui::Font font("Owned Face", 20, 0, 0.0f, 400, false, false, false, false);
    Query(font, Result::Ready);
    ReleaseScene();
}
}

int main(int argc, char** argv) {
    Require(argc == 2, "owned input directory argument");
    const std::string directory(argv[1]);
    const std::string model = directory + "/owned-face";
    const std::string second = directory + "/owned-new";
    const std::string bad = directory + "/owned-bad";
    WriteModel(model, "Owned Face");
    WriteModel(second, "Owned New");
    WriteModel(bad, "Owned Bad");
    Require(model.size() > 260, "actual long UTF8 input path control");
    PendingAndLateBinding(model);
    RequestAfterEmptyPreflight(model);
    BindingFailureAndRetry(model, second, bad);
    BitmapOwnership();
    ProfileAndCapabilityControls(directory);
    RejectedCapabilities(model);
    TruncatedBitmap();
    ApiAndRequestControls(model);
    FailedRollbackRetainsOwner(model, second);
    ContextFailure(model);
    const auto stats = FakeFontSnapshot();
    Require(stats.liveFaces == 0 && stats.facesCreated == stats.facesDestroyed
        && stats.contextsCreated == stats.contextsDestroyed, "all explicit scene ownership balances");
    std::puts("PASS adapter/owner: pending, dynamic preflight, retry, copied paths, borrowers, bitmap and pinned profile");
}
