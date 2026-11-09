#ifndef CSE_VGUI1_APP_PLATFORM_H
#define CSE_VGUI1_APP_PLATFORM_H

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace cse_vgui1_app {

constexpr std::uint32_t ServicesVersion = 1;
constexpr std::size_t MaximumBorrowers = 64;
constexpr bool ColdLoadReady = false;
constexpr bool NativeSurfaceReady = false;

enum class State : std::uint32_t { Unbound, Binding, Bound, Closing, Closed };
enum class Status : std::int32_t {
    Ok, InvalidArgument, AlreadyBound, WrongThread, Busy, ReleaseFailed, InvalidBorrower, Exhausted
};
enum class Failure : std::uint32_t {
    None, InvalidArgument, ClipboardUnavailable, InvalidSnapshot, BackendFailure, AllocationFailure, MissingSurface
};
enum class PumpResult : std::int32_t { Empty, Quit, Error };

struct ClipboardSnapshot {
    const unsigned char* data;
    std::uint64_t storageBytes;
    void* lease;
};

// Caller owns the record through Bind; it is copied, never retained by address.
// retainContext acquires a combined context AND callback-code lease. All calls
// except ownerThread are owner-thread-only. A false retainContext acquires no
// lease. ownerThread must be safe from any
// thread. No callback may throw. releaseContext false retains that entire lease;
// Close must be retried and module unload/rebind is forbidden until success.
struct ServicesV1 {
    std::uint32_t structBytes;
    std::uint32_t version;
    void* context;
    bool (*ownerThread)(void*);
    bool (*retainContext)(void*);
    bool (*releaseContext)(void*);
    PumpResult (*pumpEvents)(void*);
    bool (*applyArena)(void*, int, int, int, int, bool);
    // A non-null snapshot lease is released once even on acquisition failure.
    // Data stays readable through releaseClipboard; an empty snapshot still has
    // a lease. App copies raw capacity bytes, not a transcoded string.
    bool (*acquireClipboard)(void*, ClipboardSnapshot*);
    void (*releaseClipboard)(void*, void*);
    // Input is borrowed for this call only. A lazy provider must make its own
    // copy and retain a Borrower token until all callbacks/payloads are retired.
    bool (*writeClipboard)(void*, const unsigned char*, std::uint64_t);
    // Product-domain/approved encoding policy belongs to this required service.
    // Preserve the native raw query/partial-read semantics; null is size-query.
    bool (*readSetting)(void*, const char*, void*, std::uint64_t);
    bool (*writeString)(void*, const char*, const char*);
    bool (*writeInteger)(void*, const char*, std::int32_t);
};

struct Borrower {
    std::uint64_t generation;
    std::uint64_t identifier;
};

struct Diagnostics {
    State state;
    Failure failure;
    std::uint32_t activeCalls;
    std::uint32_t borrowers;
    std::uint64_t generation;
};

static_assert(std::is_standard_layout<ServicesV1>::value, "Private callback boundary is typed POD");
static_assert(std::is_trivial<ServicesV1>::value, "Callback record has no STL/implicit ownership");
static_assert(sizeof(int) == sizeof(std::int32_t), "Legacy App integer domain is signed32");

Status Bind(const ServicesV1& services) noexcept;
Status Close() noexcept;
Diagnostics Inspect() noexcept;
Status AcquireBorrower(Borrower& borrower) noexcept;
Status ReleaseBorrower(const Borrower& borrower) noexcept;
void RecordFailure(Failure failure) noexcept;
[[noreturn]] void InvalidLegacyUse(const char* operation, const char* reason) noexcept;

// Same-module guard, not an exchanged object. It pins callback code/context
// before ownerThread runs, and through every snapshot release or callback.
class Call {
public:
    explicit Call(const char* operation) noexcept;
    ~Call();
    Call(const Call&) = delete;
    Call& operator=(const Call&) = delete;
    const ServicesV1& Services() const noexcept {
        return services_;
    }

private:
    ServicesV1 services_;
};

// Clock is independent of UI binding and is safe in the App base constructor.
// The real implementation is a separate compile-only object in fake fixtures.
bool ReadColdClock(std::uint32_t& encodedMillis) noexcept;
bool EncodeClockTicks(std::uint64_t ticks, std::uint32_t numerator, std::uint32_t denominator,
    std::uint32_t& encodedMillis) noexcept;

}
#endif
