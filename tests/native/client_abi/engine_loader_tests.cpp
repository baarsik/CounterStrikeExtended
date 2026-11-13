#define CSE_ABI_ENGINE_VIEW
#include "fixture_types.h"
#include "../../../src/common/client_module_contracts.h"
#include "../../../src/common/client_module_entry.h"
#include "../../../src/common/client_module_required_exports.h"
#include "../../../src/public/native_module.h"
#include "../../../src/engine/blob.h"
#include <cstdarg>
#include <stdexcept>

#if !defined(_WIN32)
using HINSTANCE = void*;
#endif
#ifndef FALSE
#define FALSE 0
#define TRUE 1
#endif
#define Q_memset std::memset
class CSysModule;
cldll_func_t cl_funcs = {};
qboolean fClientLoaded = FALSE;
HINSTANCE hClientDLL = nullptr;
modfuncs_t g_modfuncs = {};
cldll_func_dst_t g_cldstAddrs = {};
BlobFootprint_t g_blobfootprintClient = {};
static CseInitializeClient initializeOwnedClient = nullptr;
static CseExchangeClient exchangeOwnedClient = nullptr;
static CseClientStudio getOwnedClientStudio = nullptr;

void* FS_LoadLibrary(const char* name) {
    return cse_platform::LoadModule(name, cse_platform::ModuleFallback::None);
}
void Sys_UnloadModule(CSysModule* module) {
    cse_platform::UnloadModule(module);
}
void* Sys_GetProcAddress(void* module, const char* name) {
    return cse_platform::GetSymbol(module, name);
}
void Con_Printf(const char*, ...) {}
[[noreturn]] void Sys_Error(const char*, ...) {
    throw std::runtime_error("controlled engine fatal error");
}
#include "engine_loader_bodies.inc"

#define REQUIRE(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "Engine client loader failure at %d: %s\n", __LINE__, #condition); return 1; \
} } while (false)

const char* Directory() {
    return "controlled engine loader";
}
int ExpandFilename(const char* name, char* destination, int bytes) {
    if (std::strcmp(name, "filesystem_stdio.dll") != 0) {
        return 0;
    }
    const auto size = std::strlen(name) + 1;
    if (size > static_cast<std::size_t>(bytes)) {
        std::abort();
    }
    std::memcpy(destination, name, size);
    return 1;
}

int main(int argc, char** argv) {
    REQUIRE(argc == 4);
    const bool accept = std::strcmp(argv[2], "accept") == 0;
    const bool incomplete = std::strcmp(argv[2], "incomplete") == 0;
    const bool useF = std::strcmp(argv[3], "F") == 0;
    // Keep a separate owning reference only to inspect the fixture's observable
    // effects after a rejected production-loader reference has been released.
    void* observer = FS_LoadLibrary(argv[1]);
    REQUIRE(observer);
    const auto effects = reinterpret_cast<int (*)()>(Sys_GetProcAddress(observer, "CseFixtureEffects"));
    const auto exchanges = reinterpret_cast<int (*)()>(Sys_GetProcAddress(observer, "CseFixtureExchanges"));
    REQUIRE(effects && effects() == 0 && exchanges && exchanges() == 0);
    bool loaded = false;
    try {
        if (useF) {
            loaded = LoadSecureClient(argv[1]) != 0;
        } else {
            LoadInsecureClient(argv[1]);
            loaded = fClientLoaded != FALSE;
        }
    } catch (const std::runtime_error&) {
        REQUIRE(!useF && !accept);
    }
    REQUIRE(loaded == accept);
    if (accept) {
        REQUIRE(fClientLoaded && hClientDLL && initializeOwnedClient && getOwnedClientStudio);
        REQUIRE(ValidateOwnedClientFunctions(cl_funcs));
        REQUIRE(effects() == 0 && exchanges() == (useF ? 1 : 0));
        cl_enginefunc_t functions = {};
        functions.pfnGetGameDirectory = Directory;
        functions.COM_ExpandFilename = ExpandFilename;
        REQUIRE(initializeOwnedClient(&cse::abi::EngineClientDescriptor, &functions, CLDLL_INTERFACE_VERSION));
        REQUIRE(effects() == 2);
        if (sizeof(void*) > sizeof(std::uint32_t)) {
            REQUIRE(reinterpret_cast<std::uintptr_t>(cl_funcs.pInitFunc) > UINT32_MAX);
        }
        ReleaseOwnedClientLibrary();
    } else {
        REQUIRE(!fClientLoaded && !hClientDLL && !initializeOwnedClient && !exchangeOwnedClient
            && !getOwnedClientStudio && effects() == 0);
        const cldll_func_t empty = {};
        REQUIRE(std::memcmp(&cl_funcs, &empty, sizeof(empty)) == 0);
        REQUIRE(exchanges() == (incomplete ? 1 : 0));
    }
    cse_platform::UnloadModule(observer);
    REQUIRE(!cse_platform::FindLoadedModule(argv[1]));
    return 0;
}
