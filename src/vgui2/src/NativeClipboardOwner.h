#ifndef CSE_VGUI2_NATIVE_CLIPBOARD_OWNER_H
#define CSE_VGUI2_NATIVE_CLIPBOARD_OWNER_H

#include "NativeClipboard.h"
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace cse_vgui2_clipboard_owner {

constexpr std::uint32_t ContractVersion = 1;
constexpr bool GeneralClipboardReady = false;
constexpr bool PublisherReady = false;
constexpr bool ColdLoadReady = false;

enum class Status : std::int32_t {
    Ok,
    PrivateOnly,
    InvalidArgument,
    Unavailable,
    WrongThread,
    NoMemory,
    MissingFormat,
    MalformedBundle,
    CodecUnavailable,
    Unrepresentable,
    HostFailure,
    FailedAfterClear,
    StateUncertain,
    OwnershipChanged,
    Busy,
    ReleaseFailed,
    Closed,
    Exhausted
};

// A REQUIRED host-root contract, not an intrinsic module lease. The caller
// holds genuine loader references while entering this component. retainRoot
// retains ALL context/backend/allocator/codec code and data. False retains
// nothing. deferRootRelease false retains the ENTIRE reference; true transfers
// it to a main-image host queue/root owner that cannot release the final code
// reference until ALL component calls/destructors have returned. It must never
// dlclose/unload executing component code from inside this callback.
// All callbacks are nonthrowing and may not reenter or pump events. retainRoot
// and ownerThread are safe from any thread; everything else is owner-thread-only.
// Lifecycle calls must be externally serialized: foreign-thread rejection is not
// a concurrent destruction/Open/Close or object-lifetime synchronization scheme.
// The host must
// issue a nonzero, never-reused epoch for each owner instance. These obligations
// require real loader/host integration; a table of successful counters is not it.
struct DependenciesV1 {
    std::uint32_t structBytes;
    std::uint32_t version;
    void* context;
    std::uint64_t ownerEpoch;
    bool (*ownerThread)(void*);
    bool (*retainRoot)(void*, std::uint64_t*);
    bool (*deferRootRelease)(void*, std::uint64_t);
    void* (*allocate)(void*, std::size_t);
    void (*deallocate)(void*, void*);
    // Explicit injected strict UTF16LE -> public UTF8 codec. No charset default.
    // Null destination plans; a second call must write exactly that many bytes.
    // The explicit unit span can contain NUL/tails; never scan/trim implicitly.
    Status (*encodePublic)(void*, cse_vgui2_clipboard::ByteSpan,
        cse_vgui2_clipboard::ByteBuffer, std::uint64_t*);
    // Optional bounded diagnostic, including only the newly created board name.
    void (*diagnostic)(void*, const char*, cse_vgui2_clipboard::ByteSpan);
};

struct PreparedViews {
    bool hasRaw8;
    cse_vgui2_clipboard::ByteSpan raw8;
    std::uint32_t raw8EncodingTag; // Caller-declared opaque identity, never autodetected.
    bool hasUnicode;
    cse_vgui2_clipboard::ByteSpan unicodeStorage;
    std::uint64_t unicodeUnitExtentBytes;
    bool publishPublic;
    std::uint64_t publicUnitExtentBytes; // Explicit prefix; no product NUL policy implied.
};

// Byte pointers borrow immutable owner storage until matching EndBorrow. A
// copied ticket does not add a borrow. EndBorrow consumes it even if retirement
// fails; the owner then retains the whole snapshot/context/code for retry.
struct Snapshot {
    std::uint64_t epoch;
    std::uint64_t ticket;
    cse_vgui2_clipboard::ByteSpan raw8;
    std::uint32_t raw8EncodingTag;
    cse_vgui2_clipboard::SnapshotView unicode;
    cse_vgui2_clipboard::Status raw8Status;
    cse_vgui2_clipboard::Status unicodeBytesStatus;
    cse_vgui2_clipboard::Status unicodeCellsStatus;
};

struct BackendV1;

// Same-module private component, not a published ABI. No default OS service.
// Open transfers backend ownership ONLY after retainRoot succeeds. A subsequent
// Open failure can retain resources; Close/retry is then mandatory. Close never
// invalidates live borrows. A failed retirement forbids successful close/rebind/
// unload. Destruction performs no callback and aborts if ownership remains.
#if !defined(_WIN32) && (defined(__clang__) || defined(__GNUC__))
class __attribute__((visibility("hidden"))) Owner {
#else
class Owner {
#endif
public:
    Owner() noexcept = default;
    ~Owner() noexcept;
    Owner(const Owner&) = delete;
    Owner& operator=(const Owner&) = delete;
    Owner(Owner&&) = delete;
    Owner& operator=(Owner&&) = delete;

    Status Open(const DependenciesV1&, const BackendV1&) noexcept;
    Status Publish(const PreparedViews&) noexcept;
    // Failure leaves the output unchanged. A malformed shared bundle is rejected
    // wholly; malformed Unicode content does not invalidate raw8/count/byte views.
    Status Acquire(Snapshot&) noexcept;
    Status EndBorrow(Snapshot&) noexcept;
    Status RetryRetirements() noexcept;
    Status Close() noexcept;
    bool HoldsRoot() const noexcept;

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

static_assert(std::is_standard_layout<DependenciesV1>::value && std::is_trivial<DependenciesV1>::value,
    "The required owner services use typed POD, not STL ownership");
static_assert(std::is_standard_layout<Snapshot>::value && std::is_trivial<Snapshot>::value,
    "Snapshot views do not own pointers or module references");

} // namespace cse_vgui2_clipboard_owner
#endif
