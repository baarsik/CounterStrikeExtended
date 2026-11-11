#pragma once
#include "FileSystem.h"
#include "cse_engine_bootstrap_abi.h"
#include "dedicated_api.h"
#include "game_server_data.h"
#include "idedicatedexports.h"
#include "native_module.h"

namespace cse::dedicated_abi {
constexpr char Symbol[] = "CseGetDedicatedAbiV1";
constexpr char AdminSymbol[] = "CseGetGameServerDataAbiV1";
constexpr std::uint32_t AdminFamily = 0x4441444d;
constexpr auto Host =
    engine_bootstrap_abi::DescribeDedicatedHost<IDedicatedServerAPI, IDedicatedExports, CreateInterfaceFn>();
constexpr auto Engine = engine_bootstrap_abi::RequireRole(Host, engine_bootstrap_abi::Role::EngineProvider);
static_assert(
    std::is_same<decltype(&IGameServerData::WriteDataRequest), void (IGameServerData::*)(const void*, int)>::value);
static_assert(std::is_same<decltype(&IGameServerData::ReadDataResponse), int (IGameServerData::*)(void*, int)>::value);
constexpr auto Admin = abi::Describe(
    {{AdminFamily, 0,
      abi::LayoutHash({1, filesystem_abi::DestructorSlots, sizeof(int), alignof(int), sizeof(IGameServerData),
                       alignof(IGameServerData), filesystem_abi::TypeShape<void (*)(const void*, int)>::Value,
                       filesystem_abi::TypeShape<int (*)(void*, int)>::Value})}});

inline bool Compatible(void* module, const char* symbol, const abi::Descriptor& expected) {
    const auto describe = reinterpret_cast<abi::GetModuleAbi>(cse_platform::GetSymbol(module, symbol));
    return describe && abi::CheckRequired(expected, describe()) == abi::Compatibility::Compatible;
}

template<class Interface>
inline bool ViewCompatible(void* view, CreateInterfaceFn factory, std::size_t methods) {
    if (!view || !factory || reinterpret_cast<std::uintptr_t>(view) % alignof(void*) != 0) {
        return false;
    }
    Dl_info owner{};
    Dl_info tableOwner{};
    const void* const* table = *reinterpret_cast<const void* const* const*>(view);
    if (!table || !dladdr(reinterpret_cast<void*>(factory), &owner) || !dladdr(table, &tableOwner) ||
        owner.dli_fbase != tableOwner.dli_fbase) {
        return false;
    }
    if (dynamic_cast<Interface*>(static_cast<IBaseInterface*>(view)) != view) {
        return false;
    }
    // Physical slots are certified by the non-optional owning-build compiler
    // gate. Before dispatch, every consumed slot must still belong to the
    // admitted provider image, and the returned base view must use its table.
    for (std::size_t index = filesystem_abi::DestructorSlots; index < filesystem_abi::DestructorSlots + methods;
         ++index) {
        Dl_info functionOwner{};
        if (!dladdr(table[index], &functionOwner) || functionOwner.dli_fbase != owner.dli_fbase) {
            return false;
        }
    }
    return true;
}

inline bool HostFactoryCompatible(CreateInterfaceFn factory) {
    if (!factory) {
        return false;
    }
    Dl_info information{};
    if (!dladdr(reinterpret_cast<void*>(factory), &information) || !information.dli_fname) {
        return false;
    }
    void* module = cse_platform::FindLoadedModule(information.dli_fname);
    return module && Compatible(module, Symbol, Host) &&
           cse_platform::GetSymbol(module, "CreateInterface") == reinterpret_cast<void*>(factory);
}
} // namespace cse::dedicated_abi
