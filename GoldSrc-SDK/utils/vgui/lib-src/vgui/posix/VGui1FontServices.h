#ifndef CSE_VGUI1_FONT_SERVICES_H
#define CSE_VGUI1_FONT_SERVICES_H

#include <cstddef>
#include <cstdint>

#if defined(__APPLE__)
#define CSE_VGUI1_FONT_SERVICE_API __attribute__((visibility("default")))
#else
#define CSE_VGUI1_FONT_SERVICE_API
#endif

enum class CseVgui1FontResultV1 : std::int32_t {
    Ready = 1,
    FilesBound = 2,
    PendingResources = 3,
    PendingRealization = 4,
    Unavailable = 5,
    FilesReleased = 6,
    InvalidArgument = -1,
    InvalidHandle = -2,
    RegistrationFailed = -3,
    ReleaseFailed = -4
};

enum class CseVgui1FontStateV1 : std::uint32_t {
    PendingResources,
    PendingRealization,
    Ready,
    Unavailable
};

struct CseVgui1FontStatusV1 {
    std::uint32_t structSize;
    CseVgui1FontStateV1 state;
    std::uint64_t catalogGeneration;
    std::int32_t nativeError;
    std::uint32_t capabilities;
};

// Private, versioned native boundary. Paths/diagnostic buffers are borrowed only
// for the call. FilesBound is distinct from Ready. Global preflight checks only
// the current lookup generation; retained borrowers must be prepared separately
// before the old metric/raster APIs can be used.
extern "C" {
CSE_VGUI1_FONT_SERVICE_API CseVgui1FontResultV1 CseVgui1FontsBindFilesV1(
    const char* const* resolvedUtf8Paths, std::uint32_t count, char* diagnostic, std::size_t capacity) noexcept;
CSE_VGUI1_FONT_SERVICE_API CseVgui1FontResultV1 CseVgui1FontsPreflightV1(
    char* diagnostic, std::size_t capacity) noexcept;
// Query is passive. Prepare checks a known Font::getPlat() handle regardless of
// lookup generation, without reactivating it in the current lookup cache. It
// reports ordinary Pending/Unavailable results before legacy use. A failed
// release takes priority: no realization is attempted until release succeeds.
CSE_VGUI1_FONT_SERVICE_API CseVgui1FontResultV1 CseVgui1FontQueryV1(const void* platform,
    CseVgui1FontStatusV1* status, char* diagnostic, std::size_t capacity) noexcept;
CSE_VGUI1_FONT_SERVICE_API CseVgui1FontResultV1 CseVgui1FontPrepareV1(const void* platform,
    CseVgui1FontStatusV1* status, char* diagnostic, std::size_t capacity) noexcept;
// Root must stop all metric/raster users before release. Platform/request
// identities survive; file-dependent faces become pending until a later bind.
// ReleaseFailed retains the context and owner tokens. The caller must retain
// this module, report failure and refuse unload/rebind until release is retried
// successfully. Permanent-failure unload is unsupported; process exit may let
// the OS reclaim process-owned registrations instead.
CSE_VGUI1_FONT_SERVICE_API CseVgui1FontResultV1 CseVgui1FontsReleaseFilesV1(
    char* diagnostic, std::size_t capacity) noexcept;
}

#endif
