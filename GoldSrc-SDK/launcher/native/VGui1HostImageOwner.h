#ifndef CSE_VGUI1_HOST_IMAGE_OWNER_H
#define CSE_VGUI1_HOST_IMAGE_OWNER_H

#if !defined(__APPLE__)
#error This unselected host image owner currently implements only the Apple loader boundary
#endif

#include <cstddef>
#include <cstdint>
#include <limits.h>
#include <pthread.h>
#include <type_traits>
#include "../../public/native_runtime_host.h"

namespace cse_vgui1_host {
constexpr std::uint32_t AdmittedImageVersion = 1;

// NEW semantics, deliberately distinct from the frozen Runtime HostReleasesV1.
// True transfers one genuine owning root reference. The main-image owner keeps
// one exact-image anchor through the outermost root return and may immediately
// balance additional references while that anchor is held. False accepts NONE.

enum class AnchorMode : std::uint32_t { AdoptInitialReference = 1, FirstTransferredReference = 2 };
enum class State : std::uint32_t { Closed, Open, Closing };
enum class Result : std::int32_t {
    Ready = 1, Closed = 2, InvalidArgument = -1, WrongThread = -2, Busy = -3,
    InvalidImage = -4, InvalidCallbackImage = -5, MissingAnchor = -6,
    ReleaseFailed = -7, Exhausted = -8, InvalidEpoch = -9
};

// Pre-load ABI admission is EXTERNAL and is not implemented by this owner.
// The borrowed reference is genuine and remains owned for this call. Paths must
// name the actual immutable admitted file. A new loaded cohort needs a globally
// nonreused epoch from the process host; this object also rejects local reuse.
struct AdmittedImageV1 {
    std::uint32_t structBytes;
    std::uint32_t version;
    std::uint64_t epoch;
    void* borrowedReference;
    const char* absolutePath;
};

constexpr std::size_t ReleaseDiagnosticBytes = 256;
struct Statistics {
    State state;
    std::uint32_t rootDepth;
    std::uint32_t retainedReferences;
    std::uint32_t maximumRetainedReferences;
    std::uint64_t epoch;
    std::uint64_t acceptedTransfers;
    std::uint64_t balancedTransfers;
    bool releasePending;
    std::int32_t nativeReleaseCode;
    std::int32_t nativeErrno;
    char nativeError[ReleaseDiagnosticBytes];
};

// Main-image, owner-thread component. No normal component allocation, no queue,
// no public C++ ABI or readiness promise. All handles are REAL separately owned
// loads, even when the loader represents multiple references by the same value.
// The OS cannot prove ownership from a handle: borrowed/arbitrary/duplicate-close
// inputs violate the required ownership contract and are never accepted usage.
//
// No destructor calls the loader. Caller MUST retain this object/context through
// every active entry or failed release, retry explicitly, and refuse rebind or
// unload. Permanent failure may retain the entire owner until process exit; it
// must not destroy this context while any root callback can still address it.
class ImageOwner {
public:
    ImageOwner() noexcept;
    ImageOwner(const ImageOwner&) = delete;
    ImageOwner& operator=(const ImageOwner&) = delete;

    // initialOwnedReference transfers ONLY on Ready, and is required only in
    // AdoptInitialReference mode. First-transfer mode initially owns no handle.
    // Open verifies the direct handle using one RTLD_NOLOAD reference. Failure
    // to close that OWN validation reference retains it in Closing/ReleaseFailed;
    // no caller reference was adopted. Inspect and explicit Close are mandatory.
    Result Open(const AdmittedImageV1&, AnchorMode, void* initialOwnedReference) noexcept;
    HostReleasesV2 Contract() noexcept;
    // Enter BEFORE calling root code, Leave AFTER it returns. If no anchor is
    // owned, supply an exact-image borrowed reference whose genuine owner keeps
    // it alive through the entry, or until it is transferred as the anchor.
    // A merely observed mapping/old function pointer is NOT entry retention.
    Result EnterRoot(void* borrowedRetainedReference = nullptr) noexcept;
    Result LeaveRoot() noexcept;
    Result RetryBalance() noexcept;
    // Close releases the anchor only at depth0, after all root frames returned.
    // Before Close, root shutdown must separately confirm its users are quiescent.
    Result Close() noexcept;
    Result Inspect(Statistics&) const noexcept;
    bool OnOwnerThread() const noexcept;

private:
    constexpr static std::size_t UuidBytes = 16;
    struct Identity {
        const void* base;
        const void* factory;
        unsigned char uuid[UuidBytes];
        char path[PATH_MAX];
    };
    static bool Accept(void*, void*) noexcept;
    static bool ReadIdentity(void*, const char*, Identity&) noexcept;
    static bool ReadUuid(const void*, unsigned char*) noexcept;
    bool SameImage(void*) const noexcept;
    bool AcceptReference(void*) noexcept;
    bool ReleaseChecked(void*) noexcept;
    Result BalancePending() noexcept;
    void ObserveRetained() noexcept;

    const pthread_t thread_;
    Identity identity_{};
    void* directHandle_ = nullptr;
    void* anchor_ = nullptr;
    void* pending_ = nullptr;
    void* entryProof_ = nullptr;
    State state_ = State::Closed;
    std::uint32_t depth_ = 0;
    std::uint32_t maximumRetained_ = 0;
    std::uint64_t epoch_ = 0;
    std::uint64_t lastEpoch_ = 0;
    std::uint64_t accepted_ = 0;
    std::uint64_t balanced_ = 0;
    bool busy_ = false;
    bool releaseFailed_ = false;
    int releaseCode_ = 0;
    int releaseErrno_ = 0;
    char releaseDiagnostic_[ReleaseDiagnosticBytes]{};
};

static_assert(std::is_standard_layout<HostReleasesV2>::value && std::is_trivial<HostReleasesV2>::value,
    "Versioned host transfer boundary is POD");
static_assert(std::is_standard_layout<AdmittedImageV1>::value && std::is_trivial<AdmittedImageV1>::value,
    "Admitted image input is a borrowed POD record");
static_assert(std::is_standard_layout<Statistics>::value && std::is_trivial<Statistics>::value,
    "Diagnostics own no callback or allocation");
}
#endif
