#include "../../../GoldSrc-SDK/public/cse_module_abi.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <iterator>
#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

typedef float vec_t;
typedef float vec3_t[3];
#include "../../../GoldSrc-SDK/common/const.h"
#include "../../../GoldSrc-SDK/engine/progdefs.h"
#include "../../../GoldSrc-SDK/engine/edict.h"
#include "../../../GoldSrc-SDK/engine/eiface.h"
#include "../../../GoldSrc-SDK/common/game_module_entry.h"
#include "../../../GoldSrc-SDK/common/generated_game_module_layouts.h"
#include "../save_fields/production_probe.h"

class GameModule {
public:
    explicit GameModule(const char *path) {
#if defined(_WIN32)
        handle_ = LoadLibraryA(path);
#else
        handle_ = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
    }
    ~GameModule() {
        if (handle_) {
#if defined(_WIN32)
            FreeLibrary(handle_);
#else
            dlclose(handle_);
#endif
        }
    }
    explicit operator bool() const { return handle_ != nullptr; }
    template<typename Function> Function Find(const char *name) const {
#if defined(_WIN32)
        return reinterpret_cast<Function>(GetProcAddress(handle_, name));
#else
        return reinterpret_cast<Function>(dlsym(handle_, name));
#endif
    }
    GameModule(const GameModule &) = delete;
    GameModule &operator=(const GameModule &) = delete;
private:
#if defined(_WIN32)
    HMODULE handle_ = nullptr;
#else
    void *handle_ = nullptr;
#endif
};

static bool Probe(const char *path) {
    using namespace cse::abi;
    GameModule module(path);
    if (!module) {
        std::fprintf(stderr, "Cannot load game module: %s\n", path);
        return false;
    }
    const auto describe = module.Find<GetModuleAbi>("CseGetModuleAbi");
    const auto initialize = module.Find<CseInitializeGame>("CseInitializeGameModule");
    const auto getApi2 = module.Find<APIFUNCTION2>("GetEntityAPI2");
    const auto getApi1 = module.Find<APIFUNCTION>("GetEntityAPI");
    using LegacyGive = void (CSE_GAME_GIVE_CALL *)(enginefuncs_t *, globalvars_t *);
    const auto legacyGive = module.Find<LegacyGive>("GiveFnptrsToDll");
    if (!describe || !initialize || (!getApi1 && !getApi2) || !legacyGive
        || Check(GameModuleDescriptor, describe()) != Compatibility::Compatible) {
        const auto *actual = describe ? describe() : nullptr;
        std::fprintf(stderr, "Game ABI failed in %s: describe=%d initialize=%d API=%d legacy=%d status=%u\n",
            path, describe != nullptr, initialize != nullptr, (getApi1 || getApi2) ? 1 : 0, legacyGive != nullptr,
            static_cast<unsigned>(Check(GameModuleDescriptor, actual)));
        if (actual && actual->tableCount == GameModuleDescriptor.tableCount) {
            for (std::uint32_t index = 0; index < actual->tableCount; ++index) {
                if (actual->tables[index].signature != GameModuleDescriptor.tables[index].signature) {
                    std::fprintf(stderr, "Mismatched table: %u\n", actual->tables[index].identifier);
                }
            }
        }
        return false;
    }
    enginefuncs_t functions{};
    globalvars_t globals{};
    DLL_FUNCTIONS table;
    constexpr unsigned char guardByte = 0xa5;
    std::memset(&table, guardByte, sizeof(table));
    std::array<unsigned char, sizeof(table)> guard;
    std::memcpy(guard.data(), &table, sizeof(table));
    int version = INTERFACE_VERSION;
    const auto getApi = [&](DLL_FUNCTIONS *output, int *interfaceVersion) {
        return getApi2 ? getApi2(output, interfaceVersion) : getApi1(output, *interfaceVersion);
    };
    // These non-null addresses deliberately cannot be native callback tables.
    // The legacy entry must reject before reading either pointer.
    constexpr std::uintptr_t invalidAddress = 1;
    legacyGive(reinterpret_cast<enginefuncs_t *>(invalidAddress), reinterpret_cast<globalvars_t *>(invalidAddress));
    if (getApi(&table, &version) || std::memcmp(&table, guard.data(), sizeof(table)) != 0) {
        return false;
    }
    auto stale = GameModuleDescriptor;
    stale.tables[static_cast<unsigned>(GameTable::SaveField) - 1].signature ^= 1;
    if (initialize(&stale, &functions, &globals) || getApi(&table, &version)
        || std::memcmp(&table, guard.data(), sizeof(table)) != 0) {
        return false;
    }
    if (!initialize(&GameModuleDescriptor, &functions, &globals) || !getApi(&table, &version)
        || !table.pfnSave || !table.pfnRestore || !table.pfnThink) {
        return false;
    }
    if (!CheckProductionSaveFields(table)) {
        return false;
    }
    // A later legacy call must not consume another exchange or replace tables.
    legacyGive(reinterpret_cast<enginefuncs_t *>(invalidAddress), reinterpret_cast<globalvars_t *>(invalidAddress));
    if (!getApi(&table, &version)) {
        return false;
    }
    // Rejection must close an earlier binding too, then allow a valid restart.
    if (initialize(&stale, &functions, &globals) || getApi(&table, &version)
        || !initialize(&GameModuleDescriptor, &functions, &globals) || !getApi(&table, &version)) {
        return false;
    }
    std::printf("Production game ABI and native save-field checks passed: %s\n", path);
    return true;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        return 2;
    }
    for (int index = 1; index < argc; ++index) {
        if (!Probe(argv[index])) {
            return 1;
        }
    }
    return 0;
}
