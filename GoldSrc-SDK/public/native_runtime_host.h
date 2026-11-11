#pragma once

#include <cstdint>
#include <type_traits>

namespace cse_vgui1_host {
constexpr std::uint32_t HostReleasesVersion = 2;

// The executable retains an exact-image anchor through the outermost root
// return. True accepts one genuine reference; additional references may be
// balanced immediately while that anchor is held. False accepts nothing.
struct HostReleasesV2 {
    std::uint32_t structBytes;
    std::uint32_t version;
    void* context;
    bool (*acceptRoot)(void*, void* ownedRootReference) noexcept;
};
}

namespace cse {
namespace native_runtime {
constexpr std::uint32_t BindingVersion = 1;
constexpr int Ready = 1;
constexpr int Closed = 2;

struct BindingV1 {
    std::uint32_t structBytes;
    std::uint32_t version;
    std::uint64_t epoch;
    const char* enginePath;
    const char* fontPath;
    cse_vgui1_host::HostReleasesV2 host;
};

// The implementations are nonthrowing. Keep function-pointer aliases usable by
// the retained C++14 component consumers as well as the C++17 native runtime.
using Open = int (*)(const BindingV1*);
using Close = int (*)();
constexpr char OpenSymbol[] = "CseNativeRuntimeOpenV1";
constexpr char CloseSymbol[] = "CseNativeRuntimeCloseV1";
static_assert(std::is_standard_layout<BindingV1>::value && std::is_trivial<BindingV1>::value,
    "Runtime binding crosses only a versioned POD boundary");
} // namespace native_runtime
} // namespace cse
