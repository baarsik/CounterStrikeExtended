#include "../../../GoldSrc-SDK/public/cse_module_abi.h"
#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <new>
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
#include "../../../GoldSrc-SDK/common/native_entity_offsets.h"
#include "../save_fields/production_probe.h"
#include "../strings/high_offset_buffer.h"

namespace {
char emptyString[] = "";
char *stringStorage = nullptr;
std::size_t stringCursor = 0;
string_t lastAllocatedString = 0;
constexpr std::size_t StringStorageBytes = 4096;
constexpr std::size_t EntityCount = 3;
edict_t *entityStorage = nullptr;
unsigned int fixtureAlerts = 0;

void CaptureFixtureAlert(ALERT_TYPE, char *format, ...) {
    ++fixtureAlerts;
    std::va_list arguments;
    va_start(arguments, format);
    std::vfprintf(stderr, format, arguments);
    va_end(arguments);
}

edict_t *LookupFixtureEntity(EOFFSET offset) {
    std::int32_t index = 0;
    return cse::entities::IndexFromOffset(entityStorage, EntityCount, offset, index) ? entityStorage + index : nullptr;
}

EOFFSET OffsetOfFixtureEntity(const edict_t *entity) {
    EOFFSET offset = cse::entities::WorldOffset;
    if (entity && !cse::entities::OffsetFromEntity(entityStorage, EntityCount, entity, offset)) {
        std::abort();
    }
    return offset;
}

string_t AllocateFixtureString(const char *text) {
    const auto bytes = std::strlen(text) + 1;
    if (!stringStorage || bytes > StringStorageBytes - stringCursor) {
        std::abort();
    }
    char *destination = stringStorage + stringCursor;
    std::memcpy(destination, text, bytes);
    stringCursor += bytes;
    lastAllocatedString = CseStringOffset(emptyString, destination);
    return lastAllocatedString;
}

int PrecacheFixtureString(char *text) {
    if (std::strcmp(text, "models/fixture.mdl") != 0 && std::strcmp(text, "fixture.wav") != 0) {
        std::abort();
    }
    return 1;
}

bool CheckProductionKeyValues(const DLL_FUNCTIONS &api) {
    const char *names[] = {"classname", "globalname", "model", "viewmodel", "weaponmodel", "noise"};
    string_t entvars_t::*members[] = {&entvars_t::classname, &entvars_t::globalname, &entvars_t::model,
        &entvars_t::viewmodel, &entvars_t::weaponmodel, &entvars_t::noise};
    edict_t entity{};
    for (std::size_t index = 0; index < std::size(names); ++index) {
        entity.v.*members[index] = ~static_cast<string_t>(0);
        KeyValueData key{nullptr, const_cast<char *>(names[index]), const_cast<char *>("owned-name"), 0};
        api.pfnKeyValue(&entity, &key);
        if (!key.fHandled || entity.v.*members[index] != lastAllocatedString) {
            std::fprintf(stderr, "Production key-value string was truncated in %s\n", names[index]);
            return false;
        }
    }
    return true;
}
} // namespace

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
    HighOffsetBuffer storage(emptyString);
    if (!storage.get()) {
        return false;
    }
    stringStorage = storage.get();
    stringCursor = 0;
    HighOffsetBuffer entityBuffer(nullptr);
    if (!entityBuffer.get()) {
        return false;
    }
    entityStorage = reinterpret_cast<edict_t *>(entityBuffer.get());
    for (std::size_t index = 0; index < EntityCount; ++index) {
        new (entityStorage + index) edict_t{};
    }
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
    functions.pfnAllocString = AllocateFixtureString;
    functions.pfnPrecacheModel = PrecacheFixtureString;
    functions.pfnPrecacheSound = PrecacheFixtureString;
    functions.pfnPEntityOfEntOffset = LookupFixtureEntity;
    functions.pfnEntOffsetOfPEntity = OffsetOfFixtureEntity;
    functions.pfnAlertMessage = CaptureFixtureAlert;
    globals.pStringBase = emptyString;
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
    if (!CheckProductionStringFields(table, functions, globals)) {
        return false;
    }
    if (!CheckProductionEntityFields(table, functions, entityStorage)) {
        return false;
    }
    constexpr unsigned int PointerRejectionAlertsPerCase = 2;
    void *pointerCases[] = {entityStorage, nullptr};
    const auto previousAlerts = fixtureAlerts;
    for (auto *pointer : pointerCases) {
        if (!CheckProductionPointerRejection(table, pointer)) {
            return false;
        }
    }
    if (fixtureAlerts - previousAlerts != PointerRejectionAlertsPerCase * std::size(pointerCases)) {
        return false;
    }
    if (!CheckProductionKeyValues(table)) {
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
