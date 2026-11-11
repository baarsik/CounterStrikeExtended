#if !defined(__APPLE__)
#error This owned VGUI1 runtime binding is Apple-only
#endif
#include "VGui1NativeRuntime.h"
#include "VGui1NativeAtlas.h"
#include "vgui_EngineSurface.h"
#include <algorithm>
#include "../public/native_module.h"
#include <crt_externs.h>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace cse_vgui1_native {
namespace {
void RootCodeAnchor() noexcept {}

bool SamePath(const char* left, const char* right) noexcept {
    if (!left || !right || left[0] != '/' || right[0] != '/') {
        return false;
    }
    char* canonicalLeft = realpath(left, nullptr);
    char* canonicalRight = realpath(right, nullptr);
    const bool equal = canonicalLeft && canonicalRight && std::strcmp(canonicalLeft, canonicalRight) == 0;
    std::free(canonicalLeft);
    std::free(canonicalRight);
    return equal;
}

bool MatchesImage(const char* path, const void* anchor) noexcept {
    Dl_info image{};
    return dladdr(anchor, &image) != 0 && image.dli_fbase && SamePath(path, image.dli_fname);
}

bool MainImageQueue(const HostReleasesV1& host) noexcept {
    if (host.structBytes != sizeof(host) || host.version != HostReleasesVersion || !host.context || !host.deferRoot) {
        return false;
    }
    Dl_info image{};
    // A process-local inserted dylib can occupy dyld enumeration index zero on ARM64.
    return dladdr(reinterpret_cast<const void*>(host.deferRoot), &image) != 0
        && image.dli_fbase == _NSGetMachExecuteHeader();
}

bool MainImageOwner(const cse_vgui1_host::HostReleasesV2& host) noexcept {
    if (host.structBytes != sizeof(host) || host.version != cse_vgui1_host::HostReleasesVersion
        || !host.context || !host.acceptRoot) {
        return false;
    }
    Dl_info image{};
    return dladdr(reinterpret_cast<const void*>(host.acceptRoot), &image) != 0
        && image.dli_fbase == _NSGetMachExecuteHeader();
}

void* LoadExact(const char* path) noexcept {
    return cse_platform::LoadModule(path, cse_platform::ModuleFallback::None);
}

Result Translate(CseVgui1FontResultV1 value) noexcept {
    if (value == CseVgui1FontResultV1::FilesBound || value == CseVgui1FontResultV1::FilesReleased) {
        return Result::Ready;
    }
    return value == CseVgui1FontResultV1::ReleaseFailed ? Result::ReleaseFailed : Result::Unavailable;
}
}

Runtime::~Runtime() noexcept {
    if (root_ || font_ || !pins_.empty() || !rollbacks_.empty()) {
        std::fputs("VGUI1 root runtime destroyed without successful explicit Close\n", stderr);
        std::abort();
    }
}

bool Runtime::CurrentThread() const noexcept {
    return thread_ == std::this_thread::get_id();
}

bool Runtime::CodePending() const noexcept {
    if (closeReleasePending_ || !rollbacks_.empty()) {
        return true;
    }
    for (const auto& pin : pins_) {
        if (pin.second.releasePending) {
            return true;
        }
    }
    return false;
}

bool Runtime::DeferRoot(void* reference) noexcept {
    // V1 queues until root return. V2 balances extras under a genuine host
    // anchor. Both accept ownership only on true and retain code on failure.
    return reference && releaseRoot_ && releaseRoot_(hostContext_, reference);
}

bool Runtime::DrainRollbacks() noexcept {
    for (auto it = rollbacks_.begin(); it != rollbacks_.end();) {
        if (!DeferRoot(it->root)) {
            return false;
        }
        cse_platform::UnloadModule(it->font);
        it = rollbacks_.erase(it);
    }
    return true;
}

Result Runtime::Open(const Binding& binding) noexcept {
    if (!MainImageQueue(binding.host)) {
        return Result::InvalidArgument;
    }
    return OpenImpl(binding.moduleEpoch, binding.rootImagePath, binding.fontImagePath,
        binding.host.context, binding.host.deferRoot);
}

Result Runtime::Open(const BindingV2& binding) noexcept {
    if (!MainImageOwner(binding.host)) {
        return Result::InvalidArgument;
    }
    return OpenImpl(binding.moduleEpoch, binding.rootImagePath, binding.fontImagePath,
        binding.host.context, binding.host.acceptRoot);
}

Result Runtime::OpenImpl(std::uint64_t epoch, const char* rootPath, const char* fontPath,
    void* context, ReleaseRoot release) noexcept {
    if (root_ || font_ || !pins_.empty() || !rollbacks_.empty()) {
        return Result::Busy;
    }
    if (!epoch || !rootPath || !fontPath || !context || !release) {
        return Result::InvalidArgument;
    }
    if (!MatchesImage(rootPath, reinterpret_cast<const void*>(&RootCodeAnchor))
        || !MatchesImage(fontPath, reinterpret_cast<const void*>(&CseVgui1FontQueryV1))) {
        return Result::Unavailable;
    }
    void* root = LoadExact(rootPath);
    if (!root) {
        return Result::Unavailable;
    }
    // Establish recoverable ownership before a second load can fail. Even an
    // incomplete Open requires explicit Close if the host queue rejects rollback.
    rootPath_ = rootPath;
    fontPath_ = fontPath;
    hostContext_ = context;
    releaseRoot_ = release;
    thread_ = std::this_thread::get_id();
    epoch_ = epoch;
    root_ = root;
    font_ = LoadExact(fontPath);
    if (!font_ || cse_platform::GetSymbol(font_, "CseVgui1FontQueryV1")
        != reinterpret_cast<void*>(&CseVgui1FontQueryV1)) {
        const auto closed = Close();
        return closed == Result::Closed ? Result::Unavailable : closed;
    }
    return Result::Ready;
}

Result Runtime::Close() noexcept {
    if (!root_ && !font_ && rollbacks_.empty()) {
        return Result::Closed;
    }
    if (!CurrentThread() || mutating_ || uses_ || !pins_.empty() || filesBound_) {
        return releaseFailed_ || CodePending() ? Result::ReleaseFailed : Result::Busy;
    }
    mutating_ = true;
    if (!DrainRollbacks() || !DeferRoot(root_)) {
        closeReleasePending_ = true;
        mutating_ = false;
        return Result::ReleaseFailed;
    }
    // Main-image host now owns the root ref through this method's return.
    void* font = font_;
    root_ = nullptr;
    font_ = nullptr;
    epoch_ = 0;
    rootPath_.clear();
    fontPath_.clear();
    closeReleasePending_ = false;
    mutating_ = false;
    hostContext_ = nullptr;
    releaseRoot_ = nullptr;
    cse_platform::UnloadModule(font);
    return Result::Closed;
}

bool Runtime::OwnerThread(void* value) noexcept {
    return static_cast<Runtime*>(value)->CurrentThread();
}

bool Runtime::PinModule(void* value, std::uint64_t* token) noexcept {
    auto& runtime = *static_cast<Runtime*>(value);
    if (!token || !runtime.CurrentThread() || !runtime.IsOpen() || runtime.mutating_ || runtime.releaseFailed_
        || runtime.CodePending() || runtime.nextToken_ == (std::numeric_limits<std::uint64_t>::max)()) {
        return false;
    }
    void* root = LoadExact(runtime.rootPath_.c_str());
    if (!root) {
        return false;
    }
    void* font = LoadExact(runtime.fontPath_.c_str());
    if (!font || cse_platform::GetSymbol(font, "CseVgui1FontQueryV1")
        != reinterpret_cast<void*>(&CseVgui1FontQueryV1)) {
        runtime.mutating_ = true;
        if (runtime.DeferRoot(root)) {
            cse_platform::UnloadModule(font);
        } else {
            runtime.rollbacks_.push_back(Pin{root, font, 0, true});
        }
        runtime.mutating_ = false;
        return false;
    }
    const auto next = ++runtime.nextToken_;
    runtime.pins_.emplace(next, Pin{root, font, 0, false});
    *token = next;
    return true;
}

bool Runtime::UnpinModule(void* value, std::uint64_t token) noexcept {
    auto& runtime = *static_cast<Runtime*>(value);
    if (!runtime.CurrentThread()) {
        return false;
    }
    const auto pin = runtime.pins_.find(token);
    if (pin == runtime.pins_.end() || pin->second.uses || runtime.mutating_
        || runtime.releaseFailed_) {
        return false;
    }
    runtime.mutating_ = true;
    if (!runtime.DeferRoot(pin->second.root)) {
        pin->second.releasePending = true;
        runtime.mutating_ = false;
        return false;
    }
    void* font = pin->second.font;
    runtime.pins_.erase(pin);
    runtime.mutating_ = false;
    cse_platform::UnloadModule(font);
    return true;
}

bool Runtime::EnterUse(void* value, std::uint64_t token) noexcept {
    auto& runtime = *static_cast<Runtime*>(value);
    if (!runtime.CurrentThread()) {
        return false;
    }
    const auto pin = runtime.pins_.find(token);
    constexpr auto MaximumUses = (std::numeric_limits<std::uint64_t>::max)();
    if (pin == runtime.pins_.end() || runtime.mutating_ || runtime.CodePending()
        || runtime.uses_ == MaximumUses || pin->second.uses == MaximumUses) {
        return false;
    }
    ++runtime.uses_;
    ++pin->second.uses;
    return true;
}

void Runtime::LeaveUse(void* value, std::uint64_t token) noexcept {
    auto& runtime = *static_cast<Runtime*>(value);
    if (!runtime.CurrentThread()) {
        std::fputs("Invalid VGUI1 root whole-use thread\n", stderr);
        std::abort();
    }
    const auto pin = runtime.pins_.find(token);
    if (pin == runtime.pins_.end() || !runtime.uses_ || !pin->second.uses) {
        std::fputs("Invalid VGUI1 root whole-use release\n", stderr);
        std::abort();
    }
    --runtime.uses_;
    --pin->second.uses;
}

cse_vgui1_selection::OwnerV1 Runtime::Owner() noexcept {
    return {sizeof(cse_vgui1_selection::OwnerV1), cse_vgui1_selection::OwnerVersion,
        this, OwnerThread, PinModule, UnpinModule, EnterUse, LeaveUse};
}

Result Runtime::BindFiles(const char* const* files, std::uint32_t count, char* diagnostic,
    std::size_t capacity) noexcept {
    if (!IsOpen() || !CurrentThread()) {
        return Result::Unavailable;
    }
    if (mutating_ || uses_ || releaseFailed_ || CodePending()) {
        return releaseFailed_ || CodePending() ? Result::ReleaseFailed : Result::Busy;
    }
    mutating_ = true;
    const auto result = CseVgui1FontsBindFilesV1(files, count, diagnostic, capacity);
    filesBound_ = filesBound_ || result == CseVgui1FontResultV1::FilesBound
        || result == CseVgui1FontResultV1::ReleaseFailed;
    releaseFailed_ = result == CseVgui1FontResultV1::ReleaseFailed;
    mutating_ = false;
    return Translate(result);
}

Result Runtime::ReleaseFiles(char* diagnostic, std::size_t capacity) noexcept {
    if (!IsOpen() || !CurrentThread()) {
        return Result::Unavailable;
    }
    if (mutating_ || uses_) {
        return Result::Busy;
    }
    mutating_ = true;
    const auto result = CseVgui1FontsReleaseFilesV1(diagnostic, capacity);
    releaseFailed_ = result == CseVgui1FontResultV1::ReleaseFailed;
    if (result == CseVgui1FontResultV1::FilesReleased) {
        filesBound_ = false;
    }
    mutating_ = false;
    return Translate(result);
}

Result Runtime::BeginScope() noexcept {
    if (!IsOpen() || !CurrentThread()) {
        return Result::Unavailable;
    }
    if (releaseFailed_ || CodePending()) {
        return Result::ReleaseFailed;
    }
    constexpr auto MaximumUses = (std::numeric_limits<std::uint64_t>::max)();
    if (mutating_ || uses_ == MaximumUses || scopeUses_ == MaximumUses) {
        return Result::Busy;
    }
    ++uses_;
    ++scopeUses_;
    return Result::Ready;
}

void Runtime::EndScope() noexcept {
    if (!CurrentThread() || !uses_ || !scopeUses_) {
        std::fputs("Invalid VGUI1 root layout/paint scope release\n", stderr);
        std::abort();
    }
    --uses_;
    --scopeUses_;
}

Scope::Scope(Runtime& runtime) noexcept : runtime_(runtime), status_(runtime.BeginScope()) {}
Scope::~Scope() noexcept {
    if (status_ == Result::Ready) {
        runtime_.EndScope();
    }
}
}

#if defined(__APPLE__)
namespace cse_vgui1_native {
namespace {
struct EmbeddedSurface {
    IEngineSurface* backend;
    std::unique_ptr<Atlas> atlas;
};
struct EmbeddedState {
    Runtime runtime;
    std::map<EngineSurfaceWrap*, std::unique_ptr<EmbeddedSurface>> surfaces;
    std::vector<EngineSurface*> backends;
    std::uint64_t rendererEpoch = 0;
    bool rendererLive = false;
};
// No late static-destructor callbacks. A failed release retains this entire
// heap-owned state and actual image refs; process exit can reclaim it. Normal
// explicit Close plus renderer shutdown destroys empty state before unload.
EmbeddedState* embeddedState = nullptr;
EmbeddedState& State() {
    if (!embeddedState) {
        embeddedState = new EmbeddedState;
    }
    return *embeddedState;
}
void DeleteEmptyState() {
    if (embeddedState && !embeddedState->runtime.HasReferences() && !embeddedState->rendererLive
        && embeddedState->surfaces.empty() && embeddedState->backends.empty()) {
        delete embeddedState;
        embeddedState = nullptr;
    }
}
std::uint64_t Generation(void*) noexcept {
    return State().rendererEpoch;
}
bool Live(void*, std::uint64_t epoch) noexcept {
    return State().rendererLive && epoch == State().rendererEpoch;
}
int Create(void* context) noexcept {
    return static_cast<EmbeddedSurface*>(context)->backend->createNewTextureID();
}
bool Upload(void* context, int id, const unsigned char* pixels, std::size_t bytes) noexcept {
    return UploadEngineFontPage(static_cast<EmbeddedSurface*>(context)->backend, id, pixels, bytes);
}
bool Retire(void* context, int id, std::uint64_t, bool live) noexcept {
    return RetireEngineFontPage(static_cast<EmbeddedSurface*>(context)->backend, id, live);
}
void GetPen(void* context, int& x, int& y) noexcept {
    static_cast<EmbeddedSurface*>(context)->backend->drawGetTextPos(x, y);
}
void SetPen(void* context, int x, int y) noexcept {
    static_cast<EmbeddedSurface*>(context)->backend->drawSetTextPos(x, y);
}
void Draw(void* context, const Quad& quad) noexcept {
    auto* backend = static_cast<EmbeddedSurface*>(context)->backend;
    backend->drawSetTexture(quad.texture);
    backend->drawPrintChar(quad.x, quad.y, quad.wide, quad.tall,
        quad.uv[0], quad.uv[1], quad.uv[2], quad.uv[3]);
}
}

Runtime& EmbeddedRuntime() noexcept {
    return State().runtime;
}
Result ConfigureEmbedded(const Binding& binding) noexcept {
    return State().runtime.Open(binding);
}
Result ConfigureEmbedded(const BindingV2& binding) noexcept {
    return State().runtime.Open(binding);
}
Result BindEmbeddedFiles(const char* const* paths, std::uint32_t count, char* diagnostic,
    std::size_t capacity) noexcept {
    return State().runtime.BindFiles(paths, count, diagnostic, capacity);
}
Result ReleaseEmbeddedFiles(char* diagnostic, std::size_t capacity) noexcept {
    return State().runtime.ReleaseFiles(diagnostic, capacity);
}
cse_vgui1_selection::Result PrepareEmbeddedScheme(vgui::Scheme* scheme,
    cse_vgui1_selection::SchemeReport& report) noexcept {
    auto& state = State();
    if (!state.runtime.IsOpen() || !state.rendererLive) {
        return cse_vgui1_selection::Result::Unavailable;
    }
    cse_vgui1_selection::SchemeAdmission admission;
    const auto prepared = admission.Prepare(scheme, state.runtime.Owner(), state.rendererEpoch, report);
    const auto retired = admission.Retire();
    return retired == cse_vgui1_selection::Result::Retired ? prepared : retired;
}

Result CloseEmbedded() noexcept {
    if (!embeddedState) {
        return Result::Closed;
    }
    if (!embeddedState->surfaces.empty()) {
        return embeddedState->runtime.ReleasePending() ? Result::ReleaseFailed : Result::Busy;
    }
    const auto released = embeddedState->runtime.ReleaseFiles(nullptr, 0);
    if (released != Result::Ready && embeddedState->runtime.IsOpen()) {
        return released;
    }
    const auto closed = embeddedState->runtime.Close();
    DeleteEmptyState();
    return closed;
}
void RememberEngineSurface(EngineSurface* backend) noexcept {
    auto& state = State();
    if (std::find(state.backends.begin(), state.backends.end(), backend) == state.backends.end()) {
        state.backends.push_back(backend);
    }
}
void ForgetEngineSurface(EngineSurface* backend) noexcept {
    if (!embeddedState) {
        return;
    }
    for (const auto& surface : embeddedState->surfaces) {
        if (surface.second->backend == backend) {
            InvalidEmbeddedUse("backend destruction while an embedded atlas still owns it");
        }
    }
    auto& backends = embeddedState->backends;
    backends.erase(std::remove(backends.begin(), backends.end(), backend), backends.end());
    DeleteEmptyState();
}
EngineSurface* FindOwnedEngineSurface(IEngineSurface* backend) noexcept {
    if (!embeddedState) {
        return nullptr;
    }
    for (auto* owned : embeddedState->backends) {
        if (static_cast<IEngineSurface*>(owned) == backend) {
            return owned;
        }
    }
    return nullptr;
}
Result AttachEmbedded(EngineSurfaceWrap* key, IEngineSurface* backend) noexcept {
    auto& state = State();
    if (!key || !backend || !state.runtime.IsOpen() || !state.rendererLive
        || !FindOwnedEngineSurface(backend)) {
        return Result::Unavailable;
    }
    if (state.surfaces.count(key)) {
        return Result::Busy;
    }
    std::unique_ptr<EmbeddedSurface> surface(new EmbeddedSurface{backend, {}});
    const RendererV1 renderer{surface.get(), Generation, Live, Create, Upload, Retire, GetPen, SetPen, Draw};
    surface->atlas.reset(new Atlas(state.runtime, renderer));
    state.surfaces.emplace(key, std::move(surface));
    return Result::Ready;
}
Result DetachEmbedded(EngineSurfaceWrap* key) noexcept {
    if (!embeddedState) {
        return Result::Ready;
    }
    if (embeddedState->runtime.Uses()) {
        return Result::Busy;
    }
    const auto surface = embeddedState->surfaces.find(key);
    if (surface == embeddedState->surfaces.end()) {
        return Result::Ready;
    }
    const auto result = surface->second->atlas->Retire();
    if (result == Result::Ready) {
        embeddedState->surfaces.erase(surface);
    }
    return result;
}
Result SelectEmbedded(EngineSurfaceWrap* key, vgui::Font* font) noexcept {
    if (!embeddedState) {
        return Result::Unavailable;
    }
    const auto surface = embeddedState->surfaces.find(key);
    if (surface == embeddedState->surfaces.end()) {
        return Result::InvalidHandle;
    }
    return font ? surface->second->atlas->Select(font) : surface->second->atlas->Retire();
}
Result DrawEmbedded(EngineSurfaceWrap* key, const char* text, int length) noexcept {
    if (!embeddedState) {
        return Result::Unavailable;
    }
    const auto surface = embeddedState->surfaces.find(key);
    return surface == embeddedState->surfaces.end() ? Result::InvalidHandle
        : surface->second->atlas->Draw(text, length);
}
Result RendererCreated() noexcept {
    auto& state = State();
    if (state.rendererLive || state.runtime.Uses()) {
        return Result::Busy;
    }
    if (state.rendererEpoch == (std::numeric_limits<std::uint64_t>::max)()) {
        return Result::Exhausted;
    }
    ++state.rendererEpoch;
    state.rendererLive = true;
    return Result::Ready;
}
Result RendererWillDestroy(bool lost) noexcept {
    if (!embeddedState) {
        return Result::Ready;
    }
    auto& state = *embeddedState;
    if (lost) {
        state.rendererLive = false; // Known loss is immediate, even while a use drains.
    }
    if (state.runtime.Uses()) {
        return Result::Busy;
    }
    for (auto& surface : state.surfaces) {
        const auto retired = surface.second->atlas->Invalidate();
        if (retired != Result::Ready) {
            return retired;
        }
    }
    state.rendererLive = false;
    DeleteEmptyState();
    return Result::Ready;
}
[[noreturn]] void InvalidEmbeddedUse(const char* operation) noexcept {
    std::fprintf(stderr, "Unadmitted native VGUI1 operation: %s\n", operation);
    std::fflush(stderr);
    std::abort();
}
}
#endif

namespace cse_vgui1_native {
bool UploadOwnedInterface(IEngineSurface* surface, int id, const unsigned char* pixels,
    int side) noexcept {
    // This allocating virtual-call lane is compiled Sync EH. The public legacy
    // interface is unchanged; exceptions terminate here before a NoEH caller.
    surface->drawSetTextureRGBA(id, reinterpret_cast<const char*>(pixels), side, side, 0, 0);
    return surface->isTextureIdValid(id) != 0;
}
}
