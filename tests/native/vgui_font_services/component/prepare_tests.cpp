#include "VGui1FontServices.h"
#include "VGUI_Font.h"
#include "fake_provider.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

namespace {
using Result = CseVgui1FontResultV1;
using State = CseVgui1FontStateV1;
constexpr std::size_t kDiagnosticBytes = 512;
constexpr int kRequestedTall = 20;
constexpr int kRequestedWeight = 400;
constexpr int kOwnedMetricHeight = 5;
std::array<char, kDiagnosticBytes> diagnostic{};

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAILED scoped preparation: %s (%s)\n", message, diagnostic.data());
        std::abort();
    }
}

Result Bind(const std::vector<std::string>& files) {
    std::vector<const char*> paths;
    for (const auto& file : files) {
        paths.push_back(file.c_str());
    }
    return CseVgui1FontsBindFilesV1(paths.data(), static_cast<std::uint32_t>(paths.size()),
        diagnostic.data(), diagnostic.size());
}

void Write(const std::string& path, const char* identity) {
    FILE* output = std::fopen(path.c_str(), "wb");
    Require(output != nullptr, "owned identity-model output");
    const std::size_t bytes = std::strlen(identity);
    Require(std::fwrite(identity, 1, bytes, output) == bytes && std::fclose(output) == 0, "owned model bytes");
}

CseVgui1FontStatusV1 Prepare(vgui::Font& font, Result expected) {
    CseVgui1FontStatusV1 status{};
    status.structSize = sizeof(status);
    Require(CseVgui1FontPrepareV1(font.getPlat(), &status, diagnostic.data(), diagnostic.size()) == expected,
        "checked ordinary preparation result");
    return status;
}

void Passive(vgui::Font& font, Result expected) {
    CseVgui1FontStatusV1 status{};
    status.structSize = sizeof(status);
    const int before = FakeFontSnapshot().realizations;
    Require(CseVgui1FontQueryV1(font.getPlat(), &status, diagnostic.data(), diagnostic.size()) == expected
        && FakeFontSnapshot().realizations == before, "Query remains passive");
}

void Clear() {
    Require(CseVgui1FontsReleaseFilesV1(diagnostic.data(), diagnostic.size()) == Result::FilesReleased,
        "owned resources released before scene retirement");
    vgui::Font_Reset();
}

void Retired(vgui::Font& held, const std::string& good, const std::string& missing) {
    const void* identity = held.getPlat();
    const int id = held.getId();
    Require(Bind({missing}) == Result::FilesBound, "bind an unavailable exact identity");
    const int before = FakeFontSnapshot().realizations;
    Require(CseVgui1FontsPreflightV1(nullptr, 0) == Result::Ready
        && FakeFontSnapshot().realizations == before, "global preflight still skips retired lookup generation");
    Passive(held, Result::PendingRealization);
    const auto unavailable = Prepare(held, Result::Unavailable);
    Require(unavailable.state == State::Unavailable && unavailable.nativeError != 0
        && unavailable.capabilities == 0 && FakeFontSnapshot().realizations == before + 1,
        "retired request reports missing identity ordinarily before legacy metrics");
    Passive(held, Result::Unavailable);
    Prepare(held, Result::Unavailable);
    Require(FakeFontSnapshot().realizations == before + 1, "per-catalog failure retry guard is unchanged");
    Require(Bind({good}) == Result::FilesBound, "publish a later exact identity");
    Passive(held, Result::PendingRealization);
    Require(CseVgui1FontsPreflightV1(nullptr, 0) == Result::Ready
        && FakeFontSnapshot().realizations == before + 1, "old generation is not globally reactivated");
    Prepare(held, Result::Ready);
    const auto stable = FakeFontSnapshot();
    Require(held.getTall() == kOwnedMetricHeight && held.getPlat() == identity && held.getId() == id,
        "successful scoped preparation preserves borrower identity and valid legacy use");
    Prepare(held, Result::Ready);
    Require(FakeFontSnapshot().facesCreated == stable.facesCreated
        && FakeFontSnapshot().realizations == stable.realizations, "preparing Ready face does not replace it");
    vgui::Font current("Owned Face", kRequestedTall, 0, 0.0f, kRequestedWeight, false, false, false, false);
    Require(current.getPlat() != identity && current.getId() > id,
        "preparing retired platform does not reinsert it in current lookup or change ID rules");
}

void InitialPending(const std::string& good, const std::string& missing) {
    vgui::Font held("Owned Face", kRequestedTall, 0, 0.0f, kRequestedWeight, false, false, false, false);
    const auto before = FakeFontSnapshot();
    Passive(held, Result::PendingResources);
    Prepare(held, Result::PendingResources);
    Require(FakeFontSnapshot().contextsCreated == before.contextsCreated
        && FakeFontSnapshot().realizations == before.realizations, "unbound preparation does not create or dispatch");
    vgui::Font_Reset();
    Retired(held, good, missing);
    Clear();
}

void FormerlyReady(const std::string& good, const std::string& missing) {
    Require(Bind({good}) == Result::FilesBound, "initial usable binding");
    vgui::Font held("Owned Face", kRequestedTall, 0, 0.0f, kRequestedWeight, false, false, false, false);
    Require(held.getTall() == kOwnedMetricHeight, "formerly Ready borrower");
    vgui::Font_Reset();
    Require(CseVgui1FontsReleaseFilesV1(nullptr, 0) == Result::FilesReleased, "release old-generation face");
    Retired(held, good, missing);
    Clear();
}

void ReleaseFailure(const std::string& good, const std::string& second, const std::string& missing) {
    Require(Bind({good}) == Result::FilesBound, "release-failure binding");
    vgui::Font held("Owned Face", kRequestedTall, 0, 0.0f, kRequestedWeight, false, false, false, false);
    const auto before = FakeFontSnapshot();
    FakeFontFailRelease(1);
    Require(CseVgui1FontsReleaseFilesV1(nullptr, 0) == Result::ReleaseFailed, "explicit release failure");
    const auto failed = Prepare(held, Result::ReleaseFailed);
    Require(failed.state == State::PendingResources && FakeFontSnapshot().realizations == before.realizations
        && FakeFontSnapshot().contextsDestroyed == before.contextsDestroyed, "release failure retains context/tokens");
    FakeFontFailRelease(0);
    Require(CseVgui1FontsReleaseFilesV1(nullptr, 0) == Result::FilesReleased, "bounded explicit release retry");
    Prepare(held, Result::PendingResources);
    Require(Bind({good}) == Result::FilesBound, "rebind after successful release");
    Prepare(held, Result::Ready);
    FakeFontFailRegister(FakeFontSnapshot().registrations + 2);
    FakeFontFailRelease(1);
    Require(Bind({second, missing}) == Result::RegistrationFailed, "rollback failure with another face Ready");
    const int attempts = FakeFontSnapshot().realizations;
    const auto ready = Prepare(held, Result::ReleaseFailed);
    Require(ready.state == State::Ready && ready.capabilities != 0
        && FakeFontSnapshot().realizations == attempts, "ReleaseFailed is authoritative without mutating Ready face");
    Passive(held, Result::Ready);
    FakeFontFailRegister(0);
    FakeFontFailRelease(0);
    Clear();
}

void ArgumentsAndBitmap() {
    CseVgui1FontStatusV1 status;
    std::memset(&status, 0xa5, sizeof(status));
    std::array<unsigned char, sizeof(status)> saved{};
    std::memcpy(saved.data(), &status, sizeof(status));
    Require(CseVgui1FontPrepareV1(nullptr, &status, nullptr, 0) == Result::InvalidArgument
        && std::memcmp(saved.data(), &status, sizeof(status)) == 0, "invalid status version is atomic");
    status.structSize = sizeof(status);
    std::memcpy(saved.data(), &status, sizeof(status));
    char foreign = 0;
    Require(CseVgui1FontPrepareV1(&foreign, &status, nullptr, 0) == Result::InvalidHandle
        && CseVgui1FontPrepareV1(nullptr, &status, nullptr, 0) == Result::InvalidHandle
        && CseVgui1FontPrepareV1(&foreign, &status, nullptr, 1) == Result::InvalidArgument
        && CseVgui1FontPrepareV1(&foreign, nullptr, nullptr, 0) == Result::InvalidArgument
        && std::memcmp(saved.data(), &status, sizeof(status)) == 0, "foreign/null/output arguments are atomic");
    const long pageBytes = sysconf(_SC_PAGESIZE);
    Require(pageBytes > 0, "owned guard-page size");
    void* inaccessible = mmap(nullptr, static_cast<std::size_t>(pageBytes), PROT_NONE,
        MAP_PRIVATE | MAP_ANON, -1, 0);
    Require(inaccessible != MAP_FAILED, "owned inaccessible membership-control page");
    Require(CseVgui1FontPrepareV1(inaccessible, &status, nullptr, 0) == Result::InvalidHandle
        && std::memcmp(saved.data(), &status, sizeof(status)) == 0,
        "foreign platform membership never dereferences the supplied pointer");
    Require(munmap(inaccessible, static_cast<std::size_t>(pageBytes)) == 0, "guard-page release");
    vgui::Font pending("Owned Face", kRequestedTall, 0, 0.0f, kRequestedWeight, false, false, false, false);
    char tiny = 'X';
    Require(CseVgui1FontPrepareV1(pending.getPlat(), &status, &tiny, 1) == Result::PendingResources && tiny == 0,
        "bounded diagnostic is terminated");
    // TGA RLE true-color header: 512 x 1 pixels, 32 bits per pixel. Four
    // 128-pixel run packets provide an authored bitmap without a font provider.
    constexpr std::array<unsigned char, 18> kBitmapHeader{
        0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 0, 32, 0};
    constexpr std::array<unsigned char, 5> kRunPacket{255, 0, 0, 0, 1};
    constexpr int kRunPacketCount = 4;
    std::vector<unsigned char> bitmap(kBitmapHeader.begin(), kBitmapHeader.end());
    for (int packet = 0; packet < kRunPacketCount; ++packet) {
        bitmap.insert(bitmap.end(), kRunPacket.begin(), kRunPacket.end());
    }
    const auto before = FakeFontSnapshot();
    vgui::Font ready("Owned bitmap", bitmap.data(), static_cast<int>(bitmap.size()), kRequestedTall, 0, 0.0f, kRequestedWeight,
        false, false, false, false);
    Prepare(ready, Result::Ready);
    Require(ready.getTall() == 1 && FakeFontSnapshot().contextsCreated == before.contextsCreated
        && FakeFontSnapshot().realizations == before.realizations, "bitmap preparation is file-service independent");
    vgui::Font invalid("\xed\xa0\x80", kRequestedTall, 0, 0.0f, kRequestedWeight, false, false, false, false);
    Require(Prepare(invalid, Result::Unavailable).nativeError != 0
        && FakeFontSnapshot().realizations == before.realizations, "invalid request remains ordinary unavailable");
    Clear();
}
}

int main(int argc, char** argv) {
    Require(argc == 2, "owned model directory");
    const std::string good = std::string(argv[1]) + "/prepare-face";
    const std::string second = std::string(argv[1]) + "/prepare-second";
    const std::string missing = std::string(argv[1]) + "/prepare-missing";
    Write(good, "Owned Face");
    Write(second, "Other Face");
    Write(missing, "Missing Face");
    InitialPending(good, missing);
    FormerlyReady(good, missing);
    ReleaseFailure(good, second, missing);
    ArgumentsAndBitmap();
    const auto stats = FakeFontSnapshot();
    Require(stats.liveFaces == 0 && stats.facesCreated == stats.facesDestroyed
        && stats.contextsCreated == stats.contextsDestroyed, "scoped preparation ownership balances");
    std::puts("PASS scoped preparation: old generations, ordinary errors/retry, stable identities and release priority");
}
