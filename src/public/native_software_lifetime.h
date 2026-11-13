#pragma once
#include "../external/steamworks/steamnetworkingtypes.h"
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace cse::software_lifetime {
constexpr std::uint32_t Version = 1;
constexpr std::uint64_t Contract = 0x43534553574c4631ull; // CSE software lifetime, revision 1.
enum class Mode : std::uint32_t { Client = 1, Dedicated = 2 };
enum class Result : std::uint32_t {
    Ready = 0,
    Retired = 1,
    Invalid = 2,
    WrongThread = 3,
    Busy = 4,
    MissingOwner = 5,
    ResourcesLive = 6,
    RegistryFailure = 7
};
struct Binding {
    std::uint32_t structBytes;
    std::uint32_t version;
    std::uint64_t epoch;
    const void* imageBase;
    void* context;
    bool (*exclusive)(void* context, const void* imageBase, std::uint64_t epoch) noexcept;
    FSteamNetworkingSocketsDebugOutput networkLog;
    Mode mode;
    std::uint32_t reserved;
};
struct Api {
    std::uint32_t structBytes;
    std::uint32_t version;
    std::uint64_t contract;
    Result (*bind)(const Binding*) noexcept;
    Result (*retire)() noexcept;
};
constexpr char Symbol[] = "CseNativeSoftwareLifetimeV1";
#if defined(__APPLE__)
static_assert(sizeof(void*) == 8 && sizeof(Mode) == 4 && sizeof(Result) == 4);
static_assert(std::is_standard_layout<Binding>::value && std::is_trivial<Binding>::value);
static_assert(sizeof(Binding) == 56 && alignof(Binding) == 8);
static_assert(offsetof(Binding, epoch) == 8 && offsetof(Binding, imageBase) == 16 && offsetof(Binding, context) == 24 &&
              offsetof(Binding, exclusive) == 32 && offsetof(Binding, networkLog) == 40 &&
              offsetof(Binding, mode) == 48);
static_assert(std::is_standard_layout<Api>::value && std::is_trivial<Api>::value);
static_assert(sizeof(Api) == 32 && alignof(Api) == 8 && offsetof(Api, bind) == 16 && offsetof(Api, retire) == 24);
#endif
} // namespace cse::software_lifetime
