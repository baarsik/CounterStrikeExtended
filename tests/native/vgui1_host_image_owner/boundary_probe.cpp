#include "VGui1HostImageOwner.h"
#include <cstdint>
#include <type_traits>

namespace host = cse_vgui1_host;
#if __cplusplus >= 201703L
using Accept = bool (*)(void*, void*) noexcept;
#else
// Exception specifications are not part of function pointer types before C++17.
using Accept = bool (*)(void*, void*);
#endif
static_assert(std::is_same<decltype(host::HostReleasesV2::acceptRoot), Accept>::value, "Actual typed callback");
static_assert(std::is_nothrow_default_constructible<host::ImageOwner>::value, "Nonthrowing owner construction");
static_assert(std::is_trivially_destructible<host::ImageOwner>::value, "No automatic loader callback");
static_assert(host::HostReleasesVersion == 2, "No frozen Runtime V1 alias");

extern "C" const std::uint64_t HostOwnerFacts[] = {
    sizeof(host::HostReleasesV2), alignof(host::HostReleasesV2), sizeof(host::AdmittedImageV1),
    alignof(host::AdmittedImageV1), sizeof(host::Statistics), alignof(host::Statistics),
    sizeof(host::ImageOwner), alignof(host::ImageOwner), host::HostReleasesVersion,
    sizeof(void*), sizeof(int), sizeof(Accept)
};
