#include "../../../src/public/cse_module_abi.h"
#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <memory>
#include <new>
#include "owned_module.h"
#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

typedef float vec_t;
typedef float vec3_t[3];
#include "../../../src/common/const.h"
#include "../../../src/engine/progdefs.h"
#include "../../../src/engine/edict.h"
#include "../../../src/engine/eiface.h"
#include "../../../src/common/game_module_entry.h"
#include "../../../src/common/generated_game_module_layouts.h"
#include "../../../src/common/c4_beep_state.h"
#include "engine_entity_fixture.h"
#include "../../../src/common/native_entity_offsets.h"
#include "../save_fields/owned_entity_stream.h"
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
unsigned int fixtureSymbolLookups = 0;

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

int PrecacheFixtureSound(char *text) {
    constexpr char sparkPrefix[] = "buttons/spark";
    if (std::strncmp(text, sparkPrefix, sizeof(sparkPrefix) - 1) == 0) {
        return 1;
    }
    return PrecacheFixtureString(text);
}

float FixtureRandomFloat(float low, float) {
    return low;
}

void *AllocateFixturePrivateData(edict_t *entity, std::size_t bytes) {
    entity->pvPrivateData = std::calloc(1, bytes);
    return entity->pvPrivateData;
}

void *FixturePrivateData(edict_t *entity) {
    return entity ? entity->pvPrivateData : nullptr;
}

const char *FixtureNameForFunction(CseSymbolAddress) {
    ++fixtureSymbolLookups;
    return nullptr;
}

CseSymbolAddress FixtureFunctionFromName(const char *) {
    ++fixtureSymbolLookups;
    return 0;
}

bool CheckProductionKeyValues(const DLL_FUNCTIONS &api) {
    const char *names[] = {"classname", "globalname", "model", "viewmodel", "weaponmodel", "noise"};
    string_t entvars_t::*members[] = {&entvars_t::classname, &entvars_t::globalname, &entvars_t::model,
        &entvars_t::viewmodel, &entvars_t::weaponmodel, &entvars_t::noise};
    edict_t entity{};
    InitializeFixtureEntity(entity);
    for (std::size_t index = 0; index < std::size(names); ++index) {
        entity.v.*members[index] = ~static_cast<string_t>(0);
        KeyValueData key{nullptr, const_cast<char *>(names[index]), const_cast<char *>("owned-name"), 0};
        api.pfnKeyValue(&entity, &key);
        if (!key.fHandled || entity.v.*members[index] != lastAllocatedString) {
            std::fprintf(stderr, "Production key-value string was truncated in %s\n", names[index]);
            return false;
        }
    }
    constexpr int originalBody = 9;
    constexpr int expectedSkin = -17;
    entity.v.body = originalBody;
    KeyValueData skin{nullptr, const_cast<char *>("skin"), const_cast<char *>("-17"), 0};
    api.pfnKeyValue(&entity, &skin);
    if (!skin.fHandled || entity.v.skin != expectedSkin || entity.v.body != originalBody) {
        std::fputs("Production integer key-value write changed an adjacent field\n", stderr);
        return false;
    }
    return true;
}

bool CheckSparkCallbacks(const SavedEntity &save, bool sparking) {
    for (const auto &group : save.Decode()) {
        if (std::strcmp(save.tokens[group.token], "BASE") != 0) {
            continue;
        }

        bool think = false;
        bool use = false;
        for (const auto &field : group.fields) {
            const char *fieldName = save.tokens[field.token];
            const char *expected = nullptr;
            if (std::strcmp(fieldName, "m_pfnThink") == 0) {
                think = true;
                expected = "SparkThink@CEnvSpark";
            } else if (std::strcmp(fieldName, "m_pfnUse") == 0) {
                use = true;
                expected = sparking ? "SparkStop@CEnvSpark" : "SparkStart@CEnvSpark";
            }
            if (expected && (field.bytes.size() != std::strlen(expected) + 1
                || std::memcmp(field.bytes.data(), expected, field.bytes.size()) != 0)) {
                return false;
            }
        }
        return use && think == sparking;
    }
    return false;
}

bool CheckProductionSavedCallbacks(const GameModule &module, const DLL_FUNCTIONS &api) {
    const auto factory = module.Find<void (*)(entvars_t *)>("env_spark");
    if (!factory || !api.pfnSpawn || !api.pfnUse) {
        return false;
    }

    constexpr int sparkToggle = 1 << 5;
    constexpr int sparkStartOn = 1 << 6;
    edict_t spark{};
    InitializeFixtureEntity(spark);
    spark.v.classname = AllocateFixtureString("env_spark");
    spark.v.spawnflags = sparkToggle | sparkStartOn;
    factory(&spark.v);
    const std::unique_ptr<void, decltype(&std::free)> privateData(spark.pvPrivateData, std::free);
    if (!privateData || api.pfnSpawn(&spark) != 0) {
        return false;
    }

    const auto previousAlerts = fixtureAlerts;
    const auto previousLookups = fixtureSymbolLookups;
    SavedEntity active(spark);
    api.pfnSave(&spark, &active.data);
    if (fixtureAlerts != previousAlerts || fixtureSymbolLookups != previousLookups
        || !CheckSparkCallbacks(active, true)) {
        return false;
    }

    api.pfnUse(&spark, &spark);
    SavedEntity stopped(spark);
    api.pfnSave(&spark, &stopped.data);
    if (fixtureAlerts != previousAlerts || !CheckSparkCallbacks(stopped, false)) {
        return false;
    }

    active.Rewind();
    if (api.pfnRestore(&spark, &active.data, 0) != 0) {
        return false;
    }
    SavedEntity restored(spark);
    api.pfnSave(&spark, &restored.data);
    if (fixtureAlerts != previousAlerts || !CheckSparkCallbacks(restored, true)) {
        return false;
    }

    api.pfnUse(&spark, &spark);
    SavedEntity stoppedAfterRestore(spark);
    api.pfnSave(&spark, &stoppedAfterRestore.data);
    return fixtureAlerts == previousAlerts && fixtureSymbolLookups == previousLookups
        && CheckSparkCallbacks(stoppedAfterRestore, false);
}

bool CheckProductionC4Beep(const GameModule &module, const DLL_FUNCTIONS &api) {
    using EntityFactory = void (*)(entvars_t *);
    if (!module.Find<EntityFactory>("weapon_c4")) {
        return true;
    }
    const auto factory = module.Find<EntityFactory>("grenade");
    if (!factory) {
        return false;
    }

    edict_t bomb{};
    InitializeFixtureEntity(bomb);
    bomb.v.classname = AllocateFixtureString("grenade");
    factory(&bomb.v);
    const std::unique_ptr<void, decltype(&std::free)> privateData(bomb.pvPrivateData, std::free);
    if (!privateData) {
        return false;
    }

    SavedEntity legacy(bomb);
    api.pfnSave(&bomb, &legacy.data);
    auto groups = legacy.Decode();
    const auto grenade = std::find_if(groups.begin(), groups.end(), [&](const Group &group) {
        return std::strcmp(legacy.tokens[group.token], "CGrenade") == 0;
    });
    if (grenade == groups.end()) {
        return false;
    }
    constexpr char planted = 1;
    constexpr float beep3Attenuation = 0.8f;
    const auto *attenuation = reinterpret_cast<const char *>(&beep3Attenuation);
    grenade->fields = {
        {legacy.Token("m_bIsC4"), {planted, 0, 0, 0}},
        {legacy.Token("m_fAttenu"), {attenuation, attenuation + sizeof(beep3Attenuation)}}
    };
    legacy.Encode(groups);
    legacy.Rewind();
    if (api.pfnRestore(&bomb, &legacy.data, 0) != 0) {
        return false;
    }

    SavedEntity restored(bomb);
    api.pfnSave(&bomb, &restored.data);
    constexpr std::int32_t expectedWave = 3;
    constexpr auto expectedPhase = static_cast<std::int32_t>(cse::save::C4BeepPhase::Beep3);
    bool wave = false;
    bool phase = false;
    for (const auto &group : restored.Decode()) {
        for (const auto &field : group.fields) {
            std::int32_t value = 0;
            if (field.bytes.size() != sizeof(value)) {
                continue;
            }
            std::memcpy(&value, field.bytes.data(), sizeof(value));
            const char *name = restored.tokens[field.token];
            wave = wave || (std::strcmp(name, "m_iCurWave") == 0 && value == expectedWave);
            phase = phase || (std::strcmp(name, "m_savedC4BeepPhase") == 0 && value == expectedPhase);
        }
    }
    return wave && phase;
}
} // namespace

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
        InitializeFixtureEntity(entityStorage[index]);
    }
    GameModule module(path);
    if (!module) {
        std::fprintf(stderr, "Cannot load game module %s: %s\n", path, module.Error());
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
    functions.pfnPrecacheSound = PrecacheFixtureSound;
    functions.pfnRandomFloat = FixtureRandomFloat;
    functions.pfnPvAllocEntPrivateData = AllocateFixturePrivateData;
    functions.pfnPvEntPrivateData = FixturePrivateData;
    functions.pfnPEntityOfEntOffset = LookupFixtureEntity;
    functions.pfnEntOffsetOfPEntity = OffsetOfFixtureEntity;
    functions.pfnNameForFunction = FixtureNameForFunction;
    functions.pfnFunctionFromName = FixtureFunctionFromName;
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
    if (!CheckProductionSavedCallbacks(module, table)) {
        std::fprintf(stderr, "Production saved callback round trip failed in %s\n", path);
        return false;
    }
    if (!CheckProductionC4Beep(module, table)) {
        std::fprintf(stderr, "Production C4 beep state did not survive restore in %s\n", path);
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
