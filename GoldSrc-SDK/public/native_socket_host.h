#pragma once
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace cse::socket_lifetime {
constexpr std::uint32_t Version = 1;
constexpr std::uint64_t Contract = 0x435345534f434b31ull; // CSESOCK1.
constexpr std::uint64_t HostContract = 0x435345534f434831ull; // CSESOCH1.
constexpr char Symbol[] = "CseNativeSocketLifetimeV1";
constexpr char HostSymbol[] = "CseNativeSocketHostV1";
enum class Status : std::uint32_t {
    Ready = 0,
    Retired = 1,
    Invalid = 2,
    WrongProcess = 3,
    WrongThread = 4,
    Busy = 5,
    LiveSockets = 6,
    NativeFailure = 7,
    MissingOwner = 8
};
enum class CloseDisposition : std::uint32_t { NotAttempted = 0, Retained = 1, Consumed = 2 };
struct CreateResult {
    std::int32_t descriptor;
    std::int32_t nativeError;
};
struct CloseResult {
    CloseDisposition disposition;
    std::int32_t nativeError;
};
using OwnsFn = bool (*)(void* context, const void* imageBase, std::uint64_t epoch) noexcept;
using CreateUdpFn = Status (*)(void* context, const void* imageBase, std::uint64_t epoch,
                              CreateResult* output) noexcept;
using CloseUdpFn = Status (*)(void* context, const void* imageBase, std::uint64_t epoch,
                             std::int32_t descriptor, CloseResult* output) noexcept;
using QuiescentFn = Status (*)(void* context, const void* imageBase, std::uint64_t epoch) noexcept;
struct Binding {
    std::uint32_t size;
    std::uint32_t version;
    std::uint64_t epoch;
    const void* imageBase;
    void* context;
    std::int32_t originPID;
    std::uint32_t reserved;
    OwnsFn owns;
    CreateUdpFn createUdp;
    CloseUdpFn closeUdp;
    QuiescentFn quiescent;
};
struct API {
    std::uint32_t size;
    std::uint32_t version;
    std::uint64_t contract;
    Status (*bind)(const Binding*) noexcept;
    Status (*retire)() noexcept;
};
struct HostTable {
    std::uint32_t size;
    std::uint32_t version;
    std::uint64_t contract;
    OwnsFn owns;
    CreateUdpFn createUdp;
    CloseUdpFn closeUdp;
    QuiescentFn quiescent;
};
#if defined(__APPLE__)
static_assert(sizeof(void*) == 8 && sizeof(Status) == 4 && sizeof(CloseDisposition) == 4);
static_assert(std::is_standard_layout_v<Binding> && std::is_trivial_v<Binding>);
static_assert(sizeof(Binding) == 72 && alignof(Binding) == 8);
static_assert(offsetof(Binding, size) == 0 && offsetof(Binding, version) == 4 && offsetof(Binding, epoch) == 8);
static_assert(offsetof(Binding, imageBase) == 16 && offsetof(Binding, context) == 24);
static_assert(offsetof(Binding, originPID) == 32 && offsetof(Binding, reserved) == 36);
static_assert(offsetof(Binding, owns) == 40 && offsetof(Binding, createUdp) == 48);
static_assert(offsetof(Binding, closeUdp) == 56 && offsetof(Binding, quiescent) == 64);
static_assert(sizeof(API) == 32 && alignof(API) == 8 && std::is_standard_layout_v<API>);
static_assert(offsetof(API, size) == 0 && offsetof(API, version) == 4 && offsetof(API, contract) == 8);
static_assert(offsetof(API, bind) == 16 && offsetof(API, retire) == 24);
static_assert(sizeof(HostTable) == 48 && alignof(HostTable) == 8 && std::is_standard_layout_v<HostTable>);
static_assert(offsetof(HostTable, size) == 0 && offsetof(HostTable, version) == 4 && offsetof(HostTable, contract) == 8);
static_assert(offsetof(HostTable, owns) == 16 && offsetof(HostTable, createUdp) == 24);
static_assert(offsetof(HostTable, closeUdp) == 32 && offsetof(HostTable, quiescent) == 40);
static_assert(sizeof(CreateResult) == 8 && alignof(CreateResult) == 4 && offsetof(CreateResult, descriptor) == 0);
static_assert(offsetof(CreateResult, nativeError) == 4 && std::is_standard_layout_v<CreateResult>);
static_assert(sizeof(CloseResult) == 8 && alignof(CloseResult) == 4 && offsetof(CloseResult, disposition) == 0);
static_assert(offsetof(CloseResult, nativeError) == 4 && std::is_standard_layout_v<CloseResult>);
#endif
} // namespace cse::socket_lifetime
