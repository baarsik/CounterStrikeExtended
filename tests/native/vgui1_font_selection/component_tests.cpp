#include "VGui1NativeFontSelection.h"
#include "VGUI_Font.h"
#include "VGUI_Scheme.h"
#include "VGUI_Cursor.h"
#include "VGui1FontServices.h"
#include "VGui1FontPlatform.h"
#include "fake_provider.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <thread>

namespace selection = cse_vgui1_selection;
namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
struct RootOwner {
    std::thread::id thread = std::this_thread::get_id();
    std::uint64_t next = 0x100000000ULL;
    int pins = 0;
    int uses = 0;
    int pinCalls = 0;
    int unpinCalls = 0;
    int enters = 0;
    int leaves = 0;
    bool pinAllowed = true;
    bool unpinAllowed = true;
    bool enterAllowed = true;
    bool fileReleaseFailed = false;
    int rejectMutationCalls = 0;
    selection::Selection* reenter = nullptr;
    selection::Result reentered = selection::Result::Unavailable;
    std::map<std::uint64_t, int> tokens;
    bool TryMutation() {
        if (uses != 0 || fileReleaseFailed) {
            ++rejectMutationCalls;
            return false;
        }
        return true;
    }
    CseVgui1FontResultV1 ReleaseFiles() {
        Require(uses == 0, "root mutation must exclude every active use");
        const auto result = CseVgui1FontsReleaseFilesV1(nullptr, 0);
        fileReleaseFailed = result == CseVgui1FontResultV1::ReleaseFailed;
        return result;
    }
};
bool OwnerThread(void* pointer) {
    return static_cast<RootOwner*>(pointer)->thread == std::this_thread::get_id();
}
bool Pin(void* pointer, std::uint64_t* token) {
    auto& owner = *static_cast<RootOwner*>(pointer);
    ++owner.pinCalls;
    if (owner.reenter) {
        owner.reentered = owner.reenter->Retire();
    }
    if (!owner.pinAllowed) {
        return false;
    }
    ++owner.pins;
    *token = ++owner.next;
    owner.tokens.emplace(*token, 0);
    return true;
}
bool Unpin(void* pointer, std::uint64_t token) {
    auto& owner = *static_cast<RootOwner*>(pointer);
    ++owner.unpinCalls;
    Require(owner.tokens.count(token) == 1 && owner.tokens.at(token) == 0, "Unpin owns exact inactive token");
    if (!owner.unpinAllowed || owner.fileReleaseFailed) {
        return false; // Root retains the entire module/record lease until retry.
    }
    --owner.pins;
    owner.tokens.erase(token);
    return true;
}
bool Enter(void* pointer, std::uint64_t token) {
    auto& owner = *static_cast<RootOwner*>(pointer);
    if (!owner.enterAllowed) {
        return false;
    }
    Require(owner.tokens.count(token) == 1, "use retains exact pinned identity");
    ++owner.tokens.at(token);
    ++owner.enters;
    ++owner.uses;
    return true;
}
void Leave(void* pointer, std::uint64_t token) {
    auto& owner = *static_cast<RootOwner*>(pointer);
    Require(owner.tokens.count(token) == 1 && owner.tokens.at(token) > 0, "Leave owns exact active token");
    --owner.tokens.at(token);
    --owner.uses;
    ++owner.leaves;
}
selection::OwnerV1 Owner(RootOwner& owner) {
    return {sizeof(selection::OwnerV1), selection::OwnerVersion, &owner, OwnerThread, Pin, Unpin, Enter, Leave};
}
void Bind() {
    const char* files[] = {"/controlled-font-selection/defaults"};
    Require(CseVgui1FontsBindFilesV1(files, 1, nullptr, 0) == CseVgui1FontResultV1::FilesBound,
        "actual font owner binds controlled resources");
}
void DeleteFont(vgui::Font* font) {
    font->vgui::Font::~Font();
    ::operator delete(font);
}
class SchemeOwner : public vgui::Scheme {
public:
    ~SchemeOwner() {
        for (auto slot : {sf_primary1, sf_primary2, sf_primary3, sf_secondary1}) {
            DeleteFont(getFont(slot));
        }
        for (int index = scu_none; index < scu_last; ++index) {
            auto* cursor = getCursor(static_cast<SchemeCursor>(index));
            cursor->vgui::Cursor::~Cursor();
            ::operator delete(cursor);
        }
    }
};
void Lifecycle() {
    RootOwner root;
    auto* font = new vgui::Font("Arial", 20, 0, 0.0f, 400, false, false, false, false);
    selection::Selection selected;
    auto owner = Owner(root);
    owner.enterUse = nullptr;
    Require(selected.Capture(font, owner) == selection::Result::InvalidArgument, "no incomplete owner success");
    owner = Owner(root);
    Require(selected.Capture(font, owner) == selection::Result::Captured && root.pins == 1,
        "capture actual pending platform with required module lease");
    Require(selected.Inspect().platform == font->getPlat()
        && reinterpret_cast<std::uintptr_t>(font->getPlat()) > (std::numeric_limits<std::uint32_t>::max)(),
        "actual native allocation identity remains full width");
    owner.unpinModule = nullptr; // The original callback record is not retained by address.
    selection::PreparedFont prepared{};
    Require(selected.BeginUse(7, prepared) == selection::Result::Pending && root.uses == 0,
        "Prepare on pending resources refuses legacy use");
    Bind();
    Require(selected.BeginUse(7, prepared) == selection::Result::Ready && prepared.atlasInvalidated,
        "actual owner prepares the selected handle");
    auto* platform = static_cast<vgui::BaseFontPlat*>(const_cast<void*>(prepared.platform));
    Require(platform->getTall() == 20, "actual owner metric through real platform type while use is pinned");
    int a = 0;
    int b = 0;
    int c = 0;
    platform->getCharABCwide('A', a, b, c);
    Require(a == 0 && b == 6 && c == 2, "actual owner preserves legacy nonnegative A metric under guard");
    unsigned char rgba[4]{};
    platform->getCharRGBA('A', 0, 0, 1, 1, rgba);
    Require(rgba[0] == 'A' && rgba[1] == 21 && rgba[2] == 37 && rgba[3] == 53,
        "actual owner raster dispatch through controlled face under whole-use guard");
    Require(!root.TryMutation(), "sole owner rejects ReleaseFiles/rebind/unload mutation during active use");
    std::thread other([&] {
        Require(selected.EndUse(prepared.useSerial) == selection::Result::WrongThread,
            "wrong thread cannot leave an active owner guard");
        Require(selected.MarkAtlasReady(prepared.useSerial) == selection::Result::WrongThread,
            "wrong thread cannot acknowledge an atlas");
    });
    other.join();
    Require(root.uses == 1, "wrong-thread failure retains original active use");
    Require(selected.Retire() == selection::Result::Busy && root.pins == 1, "active face use blocks retirement");
    Require(selected.MarkAtlasReady(prepared.useSerial) == selection::Result::Ready, "renderer acknowledges real build");
    Require(selected.EndUse(prepared.useSerial) == selection::Result::Captured, "end whole-use guard");
    const auto oldSerial = prepared.useSerial;
    DeleteFont(font); // A selection must never keep or reread the facade pointer.
    Require(selected.BeginUse(7, prepared) == selection::Result::Ready && !prepared.atlasInvalidated,
        "retained identity survives facade destruction; same atlas remains admitted");
    Require(selected.EndUse(oldSerial) == selection::Result::InvalidUse && root.uses == 1,
        "stale use cannot end a later guard");
    Require(selected.MarkAtlasReady(oldSerial) == selection::Result::InvalidUse,
        "stale use cannot acknowledge atlas construction");
    Require(selected.EndUse(prepared.useSerial) == selection::Result::Captured, "end repeated use");
    Require(selected.BeginUse(8, prepared) == selection::Result::Ready && prepared.atlasInvalidated,
        "renderer generation invalidates independently");
    Require(selected.MarkAtlasReady(prepared.useSerial) == selection::Result::Ready,
        "renderer actually acknowledges new-generation atlas before resource failure");
    Require(selected.EndUse(prepared.useSerial) == selection::Result::Captured, "end new renderer use");
    const auto catalog = selected.Inspect().catalogGeneration;
    const auto saved = prepared;
    ProviderFailRelease(1);
    Require(root.ReleaseFiles() == CseVgui1FontResultV1::ReleaseFailed, "actual failed file release");
    Require(selected.BeginUse(8, prepared) == selection::Result::ReleaseFailed && !selected.Inspect().atlasValid,
        "failed release invalidates with unchanged catalog generation");
    Require(std::memcmp(&saved, &prepared, sizeof(prepared)) == 0, "failed BeginUse leaves all output bytes untouched");
    Require(selected.Inspect().catalogGeneration == catalog, "release does not fabricate catalog increment");
    Require(selected.Retire() == selection::Result::ReleaseFailed && root.pins == 1,
        "failed Unpin retains module ownership");
    Require(!root.TryMutation(), "failed release prohibits root rebind/unload success");
    Require(selected.Capture(nullptr, Owner(root)) == selection::Result::ReleaseFailed,
        "pending retirement cannot rebind as success");
    Require(root.ReleaseFiles() == CseVgui1FontResultV1::FilesReleased, "root retries actual file release");
    Require(selected.Retire() == selection::Result::Retired && root.pins == 0, "retry retirement");
    Require(root.enters == root.leaves && root.uses == 0, "balanced use guards");
}
void GuardFailures() {
    RootOwner root;
    selection::Selection selected;
    auto* font = new vgui::Font("GuardFailures", 18, 0, 0.0f, 400, false, false, false, false);
    auto owner = Owner(root);
    owner.version += 1;
    Require(selected.Capture(font, owner) == selection::Result::InvalidArgument && root.pinCalls == 0,
        "unknown owner version rejected before callbacks");
    owner = Owner(root);
    owner.structBytes -= 1;
    Require(selected.Capture(font, owner) == selection::Result::InvalidArgument && root.pinCalls == 0,
        "wrong owner record width rejected before callbacks");
    root.pinAllowed = false;
    Require(selected.Capture(font, Owner(root)) == selection::Result::Unavailable && root.pins == 0,
        "pin failure cannot claim retained identity");
    root.pinAllowed = true;
    root.enterAllowed = false;
    Require(selected.Capture(font, Owner(root)) == selection::Result::Busy && root.pins == 0,
        "capture guard failure retires acquired pin");
    root.unpinAllowed = false;
    Require(selected.Capture(font, Owner(root)) == selection::Result::ReleaseFailed && root.pins == 1,
        "failed capture rollback retains exact module lease");
    Require(selected.Inspect().state == selection::State::ReleasePending, "rollback records pending retirement");
    root.unpinAllowed = true;
    Require(selected.Retire() == selection::Result::Retired && root.pins == 0, "retry failed rollback");
    root.enterAllowed = true;
    Require(selected.Capture(font, Owner(root)) == selection::Result::Captured, "capture after successful retry");
    std::thread other([&] {
        selection::PreparedFont output{};
        Require(selected.BeginUse(1, output) == selection::Result::WrongThread, "wrong thread cannot prepare");
        Require(selected.Retire() == selection::Result::WrongThread, "wrong thread cannot unpin");
    });
    other.join();
    root.enterAllowed = false;
    selection::PreparedFont output{};
    Require(selected.BeginUse(1, output) == selection::Result::Busy && root.uses == 0,
        "unavailable whole-use guard refuses legacy access");
    root.enterAllowed = true;
    Require(selected.Retire() == selection::Result::Retired && root.pins == 0, "retire guarded failure fixture");
    Require(selected.Retire() == selection::Result::Retired, "repeat successful retirement is inert");
    root.next = (std::numeric_limits<std::uint64_t>::max)();
    Require(selected.Capture(font, Owner(root)) == selection::Result::Captured && root.tokens.count(0) == 1,
        "opaque module token zero is valid and retained");
    Require(selected.Retire() == selection::Result::Retired && root.pins == 0, "retire token zero exactly once");
    DeleteFont(font);
    unsigned char malformed = 0;
    font = new vgui::Font("MalformedBitmap", &malformed, 1, 18, 0, 0.0f, 400, false, false, false, false);
    Require(selected.Capture(font, Owner(root)) == selection::Result::InvalidHandle && root.pins == 0,
        "actual malformed bitmap null platform cannot be admitted");
    DeleteFont(font);
}
void CacheIdentity() {
    RootOwner root;
    Bind();
    auto* first = new vgui::Font("CacheIdentity", 22, 0, 0.0f, 400, false, false, false, false);
    auto* cached = new vgui::Font("CacheIdentity", 22, 0, 0.0f, 400, false, false, false, false);
    Require(cached->getId() == -1 && cached->getPlat() == first->getPlat(), "actual retained cache-hit ID quirk");
    selection::Selection selected;
    Require(selected.Capture(cached, Owner(root)) == selection::Result::Captured, "capture real cache-hit platform");
    selection::PreparedFont prepared{};
    Require(selected.BeginUse(20, prepared) == selection::Result::Ready && prepared.id == -1,
        "full platform identity admitted independently of public numeric ID");
    const void* oldPlatform = prepared.platform;
    Require(selected.MarkAtlasReady(prepared.useSerial) == selection::Result::Ready, "acknowledge cache-hit atlas");
    Require(selected.EndUse(prepared.useSerial) == selection::Result::Captured, "end cache-hit use");
    cse_vgui1::ResetLookup();
    auto* replacement = new vgui::Font("CacheIdentity", 22, 0, 0.0f, 400, false, false, false, false);
    Require(replacement->getPlat() != oldPlatform, "lookup reset creates a distinct current platform");
    Require(selected.BeginUse(20, prepared) == selection::Result::Ready && prepared.platform == oldPlatform,
        "Prepare checks retained old platform even after lookup retirement");
    Require(!prepared.atlasInvalidated, "lookup-only reset does not invalidate an unchanged retained face");
    Require(selected.EndUse(prepared.useSerial) == selection::Result::Captured, "end retired-lookup use");
    const auto catalog = selected.Inspect().catalogGeneration;
    const char* paths[] = {"/controlled-font-selection/additive"};
    Require(CseVgui1FontsBindFilesV1(paths, 1, nullptr, 0) == CseVgui1FontResultV1::FilesBound, "additive catalog bind");
    Require(selected.BeginUse(20, prepared) == selection::Result::Ready && prepared.atlasInvalidated
        && prepared.catalogGeneration > catalog, "catalog generation invalidates retained atlas conservatively");
    Require(selected.EndUse(prepared.useSerial) == selection::Result::Captured, "end catalog update use");
    const auto oldSerial = prepared.useSerial;
    Require(selected.Retire() == selection::Result::Retired, "retire cached selection");
    Require(selected.Capture(replacement, Owner(root)) == selection::Result::Captured, "reuse selection after retirement");
    Require(selected.BeginUse(20, prepared) == selection::Result::Ready && prepared.useSerial > oldSerial,
        "use serial remains monotonic across successful retirement and recapture");
    Require(selected.EndUse(oldSerial) == selection::Result::InvalidUse, "retired selection use token stays invalid");
    Require(selected.EndUse(prepared.useSerial) == selection::Result::Captured, "end recaptured use");
    Require(selected.Retire() == selection::Result::Retired, "retire recaptured identity");
    DeleteFont(first);
    DeleteFont(cached);
    DeleteFont(replacement);
    Require(root.ReleaseFiles() == CseVgui1FontResultV1::FilesReleased, "release cache fixture resources");
}
void MissingSymbol() {
    RootOwner root;
    cse_vgui1::ResetLookup();
    ProviderMissingSymbol(true);
    SchemeOwner scheme;
    Bind();
    selection::SchemeAdmission admission;
    selection::SchemeReport report{};
    Require(admission.Prepare(&scheme, Owner(root), 30, report) == selection::Result::Unavailable,
        "one unavailable actual default font refuses entire Scheme admission");
    Require(report.preparedCount == 3 && report.failedSlot == 3 && root.pins == 0 && root.uses == 0,
        "partial four-font failure rolls back every identity and whole-use guard");
    root.unpinAllowed = false;
    Require(admission.Prepare(&scheme, Owner(root), 30, report) == selection::Result::ReleaseFailed
        && root.pins == 4 && root.uses == 0, "failed partial rollback retains all four acquired module leases");
    Require(admission.Prepare(&scheme, Owner(root), 30, report) == selection::Result::ReleaseFailed,
        "failed Scheme retirement prohibits replacement admission");
    root.unpinAllowed = true;
    Require(admission.Retire() == selection::Result::Retired && root.pins == 0, "retry all failed Scheme retirements");
    ProviderMissingSymbol(false);
    Require(admission.Prepare(&scheme, Owner(root), 30, report) == selection::Result::Unavailable,
        "actual owner retains unavailable realization within the same catalog generation");
    const char* paths[] = {"/controlled-font-selection/symbol-repair"};
    Require(CseVgui1FontsBindFilesV1(paths, 1, nullptr, 0) == CseVgui1FontResultV1::FilesBound,
        "root publishes a new resource generation after controlled symbol repair");
    Require(admission.Prepare(&scheme, Owner(root), 30, report) == selection::Result::Ready,
        "Prepare admits the actual owner after new-generation symbol repair");
    Require(admission.Slot(4) == nullptr && admission.Slot(0) != nullptr, "Scheme slot bounds");
    Require(admission.Prepare(&scheme, Owner(root), 30, report) == selection::Result::Busy,
        "cannot overwrite retained Scheme leases");
    Require(admission.Retire() == selection::Result::Retired, "retire recovered Scheme");
    Require(root.ReleaseFiles() == CseVgui1FontResultV1::FilesReleased, "release symbol fixture resources");
}
void DefaultScheme() {
    RootOwner root;
    SchemeOwner scheme;
    selection::SchemeAdmission admission;
    selection::SchemeReport report{};
    Require(admission.Prepare(&scheme, Owner(root), 10, report) == selection::Result::Pending && root.pins == 0,
        "actual four-font Scheme is pending and rolls back all captured leases");
    Bind();
    Require(admission.Prepare(&scheme, Owner(root), 10, report) == selection::Result::Ready && root.pins == 4,
        "all four actual Scheme handles prepare transactionally");
    Require(report.preparedCount == 4 && root.uses == 0, "report is a prepared snapshot, not an active face use");
    constexpr int DefaultHeights[] = {20, 18, 12, 16};
    for (std::size_t index = 0; index < selection::SchemeFontCount; ++index) {
        selection::PreparedFont prepared{};
        auto* selected = admission.Slot(index);
        Require(selected->BeginUse(10, prepared) == selection::Result::Ready, "each real Scheme font begins guarded use");
        auto* platform = static_cast<vgui::BaseFontPlat*>(const_cast<void*>(prepared.platform));
        Require(platform->getTall() == DefaultHeights[index], "actual default Scheme slot request height retained");
        Require(selected->EndUse(prepared.useSerial) == selection::Result::Captured, "end actual default Scheme use");
    }
    Require(admission.Retire() == selection::Result::Retired && root.pins == 0, "retire four identities");
    Require(root.ReleaseFiles() == CseVgui1FontResultV1::FilesReleased, "release after scheme retirement");
}
void ReentrantPin() {
    RootOwner root;
    auto* font = new vgui::Font("ReentrantPin", 18, 0, 0.0f, 400, false, false, false, false);
    selection::Selection selected;
    root.reenter = &selected;
    Require(selected.Capture(font, Owner(root)) == selection::Result::Captured, "outer pin succeeds");
    Require(root.reentered == selection::Result::Busy, "in-progress pin cannot report completed retirement");
    root.reenter = nullptr;
    Require(selected.Retire() == selection::Result::Retired, "retire after reentrant guard");
    DeleteFont(font);
}
void FatalDestructor(bool failedRetirement) {
    RootOwner root;
    auto* font = new vgui::Font("FatalDestructor", 18, 0, 0.0f, 400, false, false, false, false);
    selection::Selection selected;
    Require(selected.Capture(font, Owner(root)) == selection::Result::Captured, "fatal child holds actual identity");
    DeleteFont(font);
    if (failedRetirement) {
        root.unpinAllowed = false;
        Require(selected.Retire() == selection::Result::ReleaseFailed, "fatal child retains failed retirement");
    }
    // Scope exit must diagnose and abort without invoking root callbacks.
}
}
int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "live-destructor") == 0) {
        FatalDestructor(false);
        return 99;
    }
    if (argc == 2 && std::strcmp(argv[1], "failed-retirement-destructor") == 0) {
        FatalDestructor(true);
        return 99;
    }
    ReentrantPin();
    GuardFailures();
    Lifecycle();
    DefaultScheme();
    CacheIdentity();
    MissingSymbol();
    Require(ProviderSnapshot().liveFaces == 0 && ProviderSnapshot().contexts == 0, "actual owner closes all fake faces");
    std::puts("PASS actual Scheme/Font/owner selection component; borrowed root guard model only");
    return 0;
}
