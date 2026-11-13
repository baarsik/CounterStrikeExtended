#pragma once

#include "cse_filesystem_abi.h"
#include "native_module.h"

namespace cse { namespace filesystem_abi {

// Only owned filesystem load sites opt into this role check. Generic module and
// third-party interface loaders keep their existing contracts.
inline CreateInterfaceFn CheckedFactory(void* module, abi::Compatibility* reason = nullptr) {
    const auto describe = reinterpret_cast<abi::GetModuleAbi>(
        cse_platform::GetSymbol(module, "CseGetModuleAbi"));
    const auto status = abi::CheckRequired(Descriptor, describe ? describe() : nullptr);
    if (reason) {
        *reason = status;
    }
    if (status != abi::Compatibility::Compatible) {
        return nullptr;
    }
    return reinterpret_cast<CreateInterfaceFn>(
        cse_platform::GetSymbol(module, cse_platform::InterfaceFactorySymbol));
}

inline bool CompatibleFactory(CreateInterfaceFn factory) {
    if (!factory) {
        return false;
    }
#if defined(_WIN32)
    HMODULE module = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(factory), &module)) {
        return false;
    }
#else
    Dl_info info{};
    if (!dladdr(reinterpret_cast<void*>(factory), &info) || !info.dli_fname) {
        return false;
    }
    void* module = cse_platform::FindLoadedModule(info.dli_fname);
#endif
    // A supplied callback must be the checked provider's actual factory export.
    return CheckedFactory(module) == factory;
}

} } // namespace cse::filesystem_abi
