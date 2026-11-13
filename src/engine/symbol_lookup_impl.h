#pragma once

#include <cstring>
#include "../common/legacy_symbol_names.h"
#include "../public/cse_symbol.h"

namespace {
const char *ConvertNameToLocalPlatform(const char *name) {
    if (!name) {
        return "unknown";
    }
    if (std::strchr(name, '@')) {
        return name;
    }
    constexpr std::size_t FunctionNameBytes = 512;
    static char canonical[FunctionNameBytes];
    return cse::symbols::CopyItaniumName(name, canonical, sizeof(canonical)) ? canonical : "unknown";
}
} // namespace

const char *FindAddressInTable(extensiondll_t *dll, CseSymbolAddress address) {
#if defined(_WIN32)
    for (int index = 0; index < dll->functionCount; ++index) {
        if (dll->functionTable[index].pFunction == address) {
            return dll->functionTable[index].pFunctionName;
        }
    }
    return nullptr;
#else
    (void)dll;
    Dl_info info{};
    return dladdr(reinterpret_cast<void *>(address), &info) != 0 ? info.dli_sname : nullptr;
#endif
}

CseSymbolAddress FindNameInTable(extensiondll_t *dll, const char *name) {
#if defined(_WIN32)
    return reinterpret_cast<CseSymbolAddress>(GetProcAddress(dll->lDLLHandle, name));
#else
    return reinterpret_cast<CseSymbolAddress>(dlsym(dll->lDLLHandle, name));
#endif
}

CseSymbolAddress FunctionFromName(const char *name) {
#if !defined(_WIN32)
    // POSIX modules expose native names directly; no PE export table is constructed.
    if (name) {
        for (int index = 0; index < g_iextdllMac; ++index) {
            const auto address = FindNameInTable(&g_rgextdll[index], name);
            if (address != 0) {
                return address;
            }
        }
    }
#endif
    name = ConvertNameToLocalPlatform(name);
    for (int dllIndex = 0; dllIndex < g_iextdllMac; ++dllIndex) {
        const auto &dll = g_rgextdll[dllIndex];
        for (int index = 0; index < dll.functionCount; ++index) {
            const auto &function = dll.functionTable[index];
            if (std::strcmp(name, function.pFunctionName) == 0) {
                if (function.pFunction != 0) {
                    return function.pFunction;
                }
                break;
            }
        }
    }
    Con_Printf("Can't find proc: %s\n", name);
    return 0;
}

const char *NameForFunction(CseSymbolAddress address) {
    for (int index = 0; index < g_iextdllMac; ++index) {
        const auto *name = FindAddressInTable(&g_rgextdll[index], address);
        if (name) {
            return name;
        }
    }
    Con_Printf("Can't find address: %p\n", reinterpret_cast<void *>(address));
    return nullptr;
}
