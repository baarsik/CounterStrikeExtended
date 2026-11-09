#ifndef CSE_VGUI1_NATIVE_RUNTIME_H
#define CSE_VGUI1_NATIVE_RUNTIME_H

#include "VGui1NativeFontSelection.h"
#include "../utils/vgui/lib-src/vgui/posix/VGui1FontServices.h"
#include <cstdint>
#include <map>
#include <string>
#include <thread>
#include <vector>

class EngineSurfaceWrap;
class IEngineSurface;
class EngineSurface;
namespace vgui {
    class Font;
}

namespace cse_vgui1_native {
enum class Result : std::int32_t {
    Ready = 1, Closed = 2, Busy = -1, InvalidArgument = -2, Unavailable = -3,
    ReleaseFailed = -4, Exhausted = -5, InvalidHandle = -6, UploadFailed = -7, Capacity = -8
};

// Main-image host queue accepts ownership only on true and MUST retain each
// genuine root handle until the outermost root call returns. Draining occurs in
// host code, never inside a root callback. False keeps the whole lease in Runtime.
struct HostReleasesV1 {
    std::uint32_t structBytes;
    std::uint32_t version;
    void* context;
    bool (*deferRoot)(void*, void* ownedRootReference) noexcept;
};
constexpr std::uint32_t HostReleasesVersion = 1;
static_assert(std::is_standard_layout<HostReleasesV1>::value && std::is_trivial<HostReleasesV1>::value,
    "Private host deferred ownership boundary is typed POD");

// Process owner supplies a NEVER reused module epoch and exact loaded-image paths
// after its pre-load ABI check. Initial Open requires an external root load.
// Close/Unpin NEVER dlclose their own root code; the required host queue defers it.
struct Binding {
    std::uint64_t moduleEpoch;
    const char* rootImagePath;
    const char* fontImagePath;
    HostReleasesV1 host;
};

class Scope;
class Runtime {
public:
    Runtime() noexcept = default;
    ~Runtime() noexcept;
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    Result Open(const Binding&) noexcept;
    Result Close() noexcept;
    cse_vgui1_selection::OwnerV1 Owner() noexcept;
    Result BindFiles(const char* const*, std::uint32_t, char*, std::size_t) noexcept;
    Result ReleaseFiles(char*, std::size_t) noexcept;
    std::uint64_t ModuleEpoch() const noexcept {
        return epoch_;
    }
    bool IsOpen() const noexcept {
        return root_ && font_;
    }
    bool HasReferences() const noexcept {
        return root_ || font_ || !pins_.empty() || !rollbacks_.empty();
    }
    bool OnOwnerThread() const noexcept {
        return CurrentThread();
    }
    std::size_t Pins() const noexcept {
        return pins_.size();
    }
    std::uint64_t Uses() const noexcept {
        return uses_;
    }
    bool ReleasePending() const noexcept {
        return releaseFailed_ || CodePending();
    }

private:
    friend class Scope;
    Result BeginScope() noexcept;
    void EndScope() noexcept;
    struct Pin {
        void* root;
        void* font;
        std::uint64_t uses;
        bool releasePending;
    };
    static bool OwnerThread(void*) noexcept;
    static bool PinModule(void*, std::uint64_t*) noexcept;
    static bool UnpinModule(void*, std::uint64_t) noexcept;
    static bool EnterUse(void*, std::uint64_t) noexcept;
    static void LeaveUse(void*, std::uint64_t) noexcept;
    bool CurrentThread() const noexcept;
    bool CodePending() const noexcept;
    bool DeferRoot(void*) noexcept;
    bool DrainRollbacks() noexcept;
    void* root_ = nullptr;
    void* font_ = nullptr;
    std::string rootPath_;
    std::string fontPath_;
    std::thread::id thread_{};
    std::map<std::uint64_t, Pin> pins_;
    std::vector<Pin> rollbacks_;
    HostReleasesV1 host_{};
    std::uint64_t epoch_ = 0;
    std::uint64_t nextToken_ = 0;
    std::uint64_t uses_ = 0;
    std::uint64_t scopeUses_ = 0;
    bool mutating_ = false;
    bool filesBound_ = false;
    bool releaseFailed_ = false;
    bool closeReleasePending_ = false;
};
// Layout/paint scope uses the genuine baseline image references from Open;
// active scopes block Close and all resource mutations, including nested scopes.
class Scope {
public:
    explicit Scope(Runtime&) noexcept;
    ~Scope() noexcept;
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    Result Status() const noexcept {
        return status_;
    }
private:
    Runtime& runtime_;
    Result status_;
};
Runtime& EmbeddedRuntime() noexcept;

// Root bootstrap entrypoints. No automatic epoch, provider factory or readiness
// promotion is supplied here. Expected failures return ordinary status before
// the old void VGUI entrypoints are called.
Result ConfigureEmbedded(const Binding&) noexcept;
Result BindEmbeddedFiles(const char* const*, std::uint32_t, char*, std::size_t) noexcept;
Result ReleaseEmbeddedFiles(char*, std::size_t) noexcept;
Result CloseEmbedded() noexcept;
Result AttachEmbedded(EngineSurfaceWrap*, IEngineSurface*) noexcept;
Result DetachEmbedded(EngineSurfaceWrap*) noexcept;
Result SelectEmbedded(EngineSurfaceWrap*, vgui::Font*) noexcept;
Result DrawEmbedded(EngineSurfaceWrap*, const char*, int) noexcept;
Result RendererCreated() noexcept;
Result RendererWillDestroy(bool lost = false) noexcept;
void RememberEngineSurface(EngineSurface*) noexcept;
void ForgetEngineSurface(EngineSurface*) noexcept;
EngineSurface* FindOwnedEngineSurface(IEngineSurface*) noexcept;
bool UploadOwnedInterface(IEngineSurface*, int, const unsigned char*, int) noexcept;
// Private metadata/device boundary, defined by the selected HW or SW owner.
bool UploadEngineFontPage(IEngineSurface*, int, const unsigned char*, std::size_t) noexcept;
bool RetireEngineFontPage(IEngineSurface*, int, bool) noexcept;
[[noreturn]] void InvalidEmbeddedUse(const char*) noexcept;
}
#endif
