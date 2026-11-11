#pragma once
#include <cstddef>
#include <cstdint>
#include <type_traits>
#if defined(__APPLE__)
#include <dlfcn.h>
#endif

namespace cse::vgui_completion {
constexpr std::uint32_t Version = 1;
enum class Result : std::uint32_t { Complete = 0, Busy = 1, ForeignOwner = 2 };
struct Api {
    std::uint32_t structBytes;
    std::uint32_t version;
    Result (*finish)(const void* selectedSurface) noexcept;
};
constexpr char Symbol[] = "CseNativeVguiCompletionV1";
#if defined(__APPLE__)
static_assert(std::is_standard_layout<Api>::value && std::is_trivial<Api>::value);
static_assert(sizeof(Api) == 16 && alignof(Api) == 8 && offsetof(Api, finish) == 8);
inline const Api* Admit(void* module, const void* factory) noexcept {
    if (!module || !factory) {
        return nullptr;
    }
    const auto* api = static_cast<const Api*>(dlsym(module, Symbol));
    Dl_info owner{};
    Dl_info table{};
    Dl_info function{};
    if (!api || !dladdr(factory, &owner) || !dladdr(api, &table) || owner.dli_fbase != table.dli_fbase ||
        api->structBytes != sizeof(Api) || api->version != Version || !api->finish ||
        !dladdr(reinterpret_cast<const void*>(api->finish), &function) || function.dli_fbase != owner.dli_fbase) {
        return nullptr;
    }
    return api;
}
#endif
} // namespace cse::vgui_completion
