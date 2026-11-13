#ifndef CSE_VGUI1_NATIVE_FONT_SELECTION_H
#define CSE_VGUI1_NATIVE_FONT_SELECTION_H

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace vgui {
class Font;
class Scheme;
}

namespace cse_vgui1_selection {
constexpr std::uint32_t OwnerVersion = 1;
constexpr std::size_t SchemeFontCount = 4; // The four initialized, non-user default Scheme slots.
constexpr bool PublisherReady = false;
constexpr bool SurfaceReady = false;
constexpr bool ColdLoadReady = false;

enum class Result : std::int32_t {
    Ready = 1,
    Captured = 2,
    Retired = 3,
    Pending = 4,
    Unavailable = 5,
    Busy = -1,
    InvalidArgument = -2,
    InvalidHandle = -3,
    WrongThread = -4,
    ReleaseFailed = -5,
    Exhausted = -6,
    InvalidUse = -7
};
enum class State : std::uint32_t {
    Empty,
    Capturing,
    Selected,
    EnteringUse,
    InUse,
    LeavingUse,
    Retiring,
    ReleasePending,
    Retired
};

// REQUIRED borrowed root contract, not a FontServices intrinsic lease. Context
// and callback code outlive every selection, including failed retirement.
// Pin retains VGUI code/ModuleData identities; false acquires nothing. Unpin
// false retains the ENTIRE lease. Tokens are opaque uint64 values, including zero.
// Failed Unpin must also keep root unload/rebind prohibited until a successful retry.
// EnterUse/LeaveUse are nestable shared-use guards excluding EVERY root font
// ReleaseFiles/rebind/unload mutation through the whole use. Root is the sole
// mutation owner. Hooks do bookkeeping only, never pump events or throw.
// OwnerThread is safe from any thread; all other operations are owner-thread-only.
struct OwnerV1 {
    std::uint32_t structBytes;
    std::uint32_t version;
    void* context;
    bool (*ownerThread)(void*);
    bool (*pinModule)(void*, std::uint64_t*);
    bool (*unpinModule)(void*, std::uint64_t);
    bool (*enterUse)(void*, std::uint64_t);
    void (*leaveUse)(void*, std::uint64_t);
};
static_assert(std::is_standard_layout<OwnerV1>::value && std::is_trivial<OwnerV1>::value,
    "The required owner boundary is typed POD");

struct PreparedFont {
    const void* platform;
    std::int32_t id;
    std::uint64_t catalogGeneration;
    std::uint64_t rendererGeneration;
    std::uint64_t useSerial;
    std::uint32_t capabilities;
    bool atlasInvalidated;
};
struct Diagnostics {
    State state;
    bool moduleHeld;
    bool atlasValid;
    const void* platform;
    std::int32_t id;
    std::uint64_t catalogGeneration;
    std::uint64_t rendererGeneration;
    std::int32_t serviceResult;
};
struct SchemeReport {
    std::uint32_t preparedCount;
    std::uint32_t failedSlot;
    std::uint64_t catalogGeneration;
    std::int32_t serviceResult;
};

// Same-engine component, not an exported C++ interface. Explicit retirement is
// mandatory. Destruction never invokes a callback; a live lease is fatal misuse.
// A Font/Scheme argument is borrowed only for its call and is NEVER retained.
class Selection {
public:
    Selection() noexcept = default;
    ~Selection() noexcept;
    Selection(const Selection&) = delete;
    Selection& operator=(const Selection&) = delete;
    Selection(Selection&&) = delete;
    Selection& operator=(Selection&&) = delete;

    Result Capture(vgui::Font* font, const OwnerV1& owner) noexcept;
    // Prepare occurs on EVERY BeginUse while root excludes mutations. Output is
    // untouched on failure. platform is usable only through matching EndUse.
    Result BeginUse(std::uint64_t rendererGeneration, PreparedFont& output) noexcept;
    Result MarkAtlasReady(std::uint64_t useSerial) noexcept;
    Result EndUse(std::uint64_t useSerial) noexcept;
    Result Retire() noexcept;
    Diagnostics Inspect() const noexcept;

private:
    friend class SchemeAdmission;
    Result CaptureImpl(vgui::Font*, vgui::Scheme*, std::size_t, const OwnerV1&) noexcept;
    Result DropCapture(Result reason) noexcept;
    OwnerV1 owner_{};
    std::uint64_t moduleToken_ = 0;
    const void* platform_ = nullptr;
    std::int32_t id_ = 0;
    std::uint64_t catalog_ = 0;
    std::uint64_t renderer_ = 0;
    std::uint64_t serial_ = 0;
    State state_ = State::Empty;
    bool held_ = false;
    bool atlasValid_ = false;
    std::int32_t serviceResult_ = 0;
};

class SchemeAdmission {
public:
    // All four identities remain pinned only on success. Ready is a prepared
    // point-in-time snapshot, NEVER Surface readiness or permission to skip a
    // later BeginUse. Failure retires all acquired identities; failed Unpin
    // retains the affected slots and requires Retire retry before reuse/unload.
    Result Prepare(vgui::Scheme*, const OwnerV1&, std::uint64_t rendererGeneration, SchemeReport&) noexcept;
    Result Retire() noexcept;
    Selection* Slot(std::size_t index) noexcept;

private:
    Selection slots_[SchemeFontCount];
};
}
#endif
