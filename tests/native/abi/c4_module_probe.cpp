// An explicit production-DLL fixture. No game world, network or provider is started.
#include <array>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "owned_module.h"
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "cse_module_abi.h"
#include "game_module_entry.h"
#include "generated_game_module_layouts.h"
#include "engine_entity_fixture.h"
#include "../save_fields/owned_entity_stream.h"

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "C4 check failed at line %d: %s\n", __LINE__, #condition); \
    return false; \
} } while (false)

namespace {
std::size_t allocatedBytes;
unsigned errors;
std::array<char, 4096> stringPool{};
std::size_t nextString = 1;
edict_t *overlayDestination;
edict_t *removedEntity;
bool removedGlobalNameWasCleared;

string_t AllocateString(const char *text) {
    const auto length = std::strlen(text) + 1;
    if (length > stringPool.size() - nextString) {
        std::abort();
    }
    const auto offset = nextString;
    std::memcpy(stringPool.data() + offset, text, length);
    nextString += length;
    return static_cast<string_t>(offset);
}

edict_t *FindByString(edict_t *, const char *field, const char *value) {
    return overlayDestination && std::strcmp(field, "globalname") == 0
        && std::strcmp(value, stringPool.data() + overlayDestination->v.globalname) == 0
        ? overlayDestination : nullptr;
}

void RemoveEntity(edict_t *entity) {
    removedEntity = entity;
    removedGlobalNameWasCleared = entity->v.globalname == 0;
    entity->free = true;
}

void *AllocatePrivate(edict_t *entity, std::size_t bytes) {
    allocatedBytes = static_cast<std::size_t>(bytes);
    entity->pvPrivateData = std::calloc(1, allocatedBytes);
    return entity->pvPrivateData;
}

void *PrivateData(edict_t *entity) {
    return entity ? entity->pvPrivateData : nullptr;
}

void Alert(ALERT_TYPE type, char *format, ...) {
    if (type == at_error) {
        ++errors;
    }
    va_list arguments;
    va_start(arguments, format);
    std::vfprintf(stderr, format, arguments);
    va_end(arguments);
}

void GameDirectory(char *directory) {
    std::strcpy(directory, "cstrike");
}


bool Probe(GameModule &module) {
    const auto describe = module.Find<cse::abi::GetModuleAbi>("CseGetModuleAbi");
    const auto initialize = module.Find<CseInitializeGame>("CseInitializeGameModule");
    const auto getApi = module.Find<APIFUNCTION>("GetEntityAPI");
    using Factory = void (*)(entvars_t *);
    const auto factory = module.Find<Factory>("grenade");
    CHECK(describe && initialize && getApi && factory);
    CHECK(cse::abi::Check(cse::abi::GameModuleDescriptor, describe()) == cse::abi::Compatibility::Compatible);
    enginefuncs_t functions{};
    functions.pfnPvAllocEntPrivateData = AllocatePrivate;
    functions.pfnPvEntPrivateData = PrivateData;
    functions.pfnAlertMessage = Alert;
    functions.pfnGetGameDir = GameDirectory;
    functions.pfnAllocString = AllocateString;
    functions.pfnFindEntityByString = FindByString;
    functions.pfnRemoveEntity = RemoveEntity;
    globalvars_t globals{};
    globals.pStringBase = stringPool.data();
    globals.time = 10.0f;
    CHECK(initialize(&cse::abi::GameModuleDescriptor, &functions, &globals));
    DLL_FUNCTIONS api{};
    CHECK(getApi(&api, INTERFACE_VERSION));
    edict_t entity{};
    InitializeFixtureEntity(entity);
    entity.serialnumber = 1;
    entity.v.pContainingEntity = &entity;
    factory(&entity.v);
    std::printf("CGrenade private storage: DLL=%zu fixture=%zu\n", allocatedBytes, sizeof(CGrenade));
    CHECK(allocatedBytes == sizeof(CGrenade));
    auto *bomb = static_cast<CGrenade *>(entity.pvPrivateData);
    CHECK(bomb);
    struct Case {
        const char *sound;
        float attenuation;
        int wave;
        int phase;
    };
    const Case cases[] = {
        {nullptr, 0.0f, 0, 1},
        {"weapons/c4_beep1.wav", 1.5f, 1, 2},
        {"weapons/c4_beep2.wav", 1.0f, 2, 3},
        {"weapons/c4_beep3.wav", 0.8f, 3, 4},
        {"weapons/c4_beep4.wav", 0.5f, 4, 5},
        {"weapons/c4_beep5.wav", 0.2f, 5, 6},
        {"weapons/c4_beep5.wav", 0.2f, 19, 6}
    };
    for (const auto &item : cases) {
        bomb->m_bIsC4 = true;
        bomb->m_sBeepName = item.sound;
        bomb->m_fAttenu = item.attenuation;
        bomb->m_iCurWave = item.wave;
        bomb->m_flNextFreq = 11.0f;
        bomb->m_flNextFreqInterval = 2.0f;
        bomb->m_flNextBeep = 100.0f;
        bomb->m_flNextBlink = 100.0f;
        bomb->m_flC4Blow = 100.0f;
        SavedEntity saved(entity);
        api.pfnSave(&entity, &saved.data);
        auto groups = saved.Decode();
        bool foundPhase = false;
        for (const auto &group : groups) {
            for (const auto &field : group.fields) {
                CHECK(std::strcmp(saved.tokens[field.token], "m_sBeepName") != 0);
                if (std::strcmp(saved.tokens[field.token], "m_savedC4BeepPhase") == 0) {
                    CHECK(field.bytes.size() == sizeof(std::int32_t));
                    std::int32_t phase;
                    std::memcpy(&phase, field.bytes.data(), sizeof(phase));
                    CHECK(phase == item.phase);
                    foundPhase = true;
                }
            }
        }
        CHECK(foundPhase);
        // Restore the real typed callback by its canonical symbol. Never manufacture a member pointer.
        const char thinkName[] = "C4Think@CGrenade";
        for (auto &group : groups) {
            if (std::strcmp(saved.tokens[group.token], "BASE") == 0) {
                bool hasThink = false;
                for (const auto &field : group.fields) {
                    hasThink = hasThink || std::strcmp(saved.tokens[field.token], "m_pfnThink") == 0;
                }
                if (!hasThink) {
                    group.fields.push_back({saved.Token("m_pfnThink"), {thinkName, thinkName + sizeof(thinkName)}});
                }
            }
        }
        saved.Encode(groups);
        bomb->m_sBeepName = reinterpret_cast<const char *>(1);
        bomb->m_iCurWave = -99;
        saved.Rewind();
        api.pfnRestore(&entity, &saved.data, 0);
        CHECK(item.sound ? bomb->m_sBeepName && std::strcmp(bomb->m_sBeepName, item.sound) == 0
            : bomb->m_sBeepName == nullptr);
        CHECK(bomb->m_iCurWave == item.wave);
        CHECK(bomb->m_fAttenu == item.attenuation);
        CHECK(bomb->m_flNextFreq == 11.0f && bomb->m_flNextFreqInterval == 2.0f);
        CHECK(bomb->m_flNextBeep == 100.0f && bomb->m_flNextBlink == 100.0f && bomb->m_flC4Blow == 100.0f);
        globals.time = 11.0f;
        api.pfnThink(&entity);
        const auto &next = cases[item.wave < 5 ? item.wave + 1 : 5];
        if (bomb->m_iCurWave != item.wave + 1) {
            std::fprintf(stderr, "C4 think mismatch: before=%d after=%d time=%g deadline=%g nextthink=%g "
                "callback=%d flags=%x origin=(%g,%g,%g) velocity=(%g,%g,%g)\n",
                item.wave, bomb->m_iCurWave, globals.time, bomb->m_flNextFreq, entity.v.nextthink,
                bomb->m_pfnThink != nullptr, static_cast<unsigned int>(entity.v.flags),
                entity.v.origin.x, entity.v.origin.y, entity.v.origin.z,
                entity.v.velocity.x, entity.v.velocity.y, entity.v.velocity.z);
        }
        CHECK(bomb->m_iCurWave == item.wave + 1);
        CHECK(bomb->m_sBeepName && std::strcmp(bomb->m_sBeepName, next.sound) == 0);
        CHECK(bomb->m_fAttenu == next.attenuation);
        CHECK(bomb->m_flNextFreq == 13.0f && bomb->m_flNextFreqInterval == 1.8f);
        CHECK(entity.v.nextthink == 11.12f);
        // Calls before the next deadline retain the phase and counter.
        api.pfnThink(&entity);
        CHECK(bomb->m_iCurWave == item.wave + 1 && bomb->m_flNextFreq == 13.0f);
        globals.time = 10.0f;
        // Import an old group containing only raw pointer bits and attenuation.
        for (auto &group : groups) {
            if (std::strcmp(saved.tokens[group.token], "CGrenade") != 0) {
                continue;
            }
            for (auto field = group.fields.begin(); field != group.fields.end();) {
                const auto *name = saved.tokens[field->token];
                if (std::strcmp(name, "m_savedC4BeepPhase") == 0 || std::strcmp(name, "m_iCurWave") == 0) {
                    field = group.fields.erase(field);
                } else {
                    ++field;
                }
            }
            group.fields.push_back({saved.Token("m_sBeepName"), {'\x01', '\x00', '\x00', '\x00'}});
        }
        saved.Encode(groups);
        bomb->m_sBeepName = reinterpret_cast<const char *>(1);
        bomb->m_iCurWave = -99;
        saved.Rewind();
        api.pfnRestore(&entity, &saved.data, 0);
        CHECK(item.sound ? bomb->m_sBeepName && std::strcmp(bomb->m_sBeepName, item.sound) == 0
            : bomb->m_sBeepName == nullptr);
        CHECK(bomb->m_iCurWave == (item.wave > 5 ? 5 : item.wave));
        // A corrupt symbolic phase must fail before the engine resumes this entity.
        for (auto &group : groups) {
            if (std::strcmp(saved.tokens[group.token], "CGrenade") == 0) {
                group.fields.push_back({saved.Token("m_savedC4BeepPhase"), {'\x7f', '\x00', '\x00', '\x00'}});
            }
        }
        saved.Encode(groups);
        saved.Rewind();
        CHECK(api.pfnRestore(&entity, &saved.data, 0) == -1);
        CHECK(bomb->m_sBeepName == nullptr);
        CHECK(errors == 1);
        errors = 0;
    }
    // Other grenade kinds neither interpret nor persist the unused C4 sound pointer.
    bomb->m_bIsC4 = false;
    bomb->m_sBeepName = reinterpret_cast<const char *>(1);
    bomb->m_fAttenu = 0.3f;
    entity.v.origin = Vector(20.0f, 30.0f, 40.0f);
    SavedEntity ordinary(entity);
    api.pfnSave(&entity, &ordinary.data);
    ordinary.Rewind();
    CHECK(api.pfnRestore(&entity, &ordinary.data, 0) != -1);
    CHECK(!bomb->m_bIsC4 && bomb->m_sBeepName == nullptr && bomb->m_fAttenu == 0.3f);

    // A failed transition overlays a different, live destination entity. The engine only
    // frees its temporary source afterward, so the DLL must remove the failed destination.
    struct GlobalCount {
        int m_listCount;
    } count{1};
    TYPEDESCRIPTION countFields[] = {DEFINE_FIELD(GlobalCount, m_listCount, FIELD_INTEGER)};
    globalentity_t globalRecord{};
    std::memcpy(globalRecord.name, "c4_transition", sizeof("c4_transition"));
    std::memcpy(globalRecord.levelName, "source", sizeof("source"));
    globalRecord.state = GLOBAL_ON;
    TYPEDESCRIPTION globalFields[] = {
        DEFINE_ARRAY(globalentity_t, name, FIELD_CHARACTER, 64),
        DEFINE_ARRAY(globalentity_t, levelName, FIELD_CHARACTER, 32),
        DEFINE_FIELD(globalentity_t, state, FIELD_INTEGER)
    };
    SavedEntity globalState(entity);
    api.pfnSaveWriteFields(&globalState.data, "GLOBAL", &count, countFields, 1);
    api.pfnSaveWriteFields(&globalState.data, "GENT", &globalRecord, globalFields, 3);
    globalState.Rewind();
    api.pfnRestoreGlobalState(&globalState.data);
    globals.mapname = AllocateString("destination");
    entity.v.classname = AllocateString("grenade");
    entity.v.globalname = AllocateString("c4_transition");
    entity.v.mins = Vector(-4.0f, -4.0f, -4.0f);
    entity.v.flags |= FL_GRAPHED;
    bomb->m_bIsC4 = true;
    bomb->m_sBeepName = "weapons/c4_beep3.wav";
    bomb->m_fAttenu = 0.8f;
    SavedEntity transition(entity);
    api.pfnSave(&entity, &transition.data);
    auto transitionGroups = transition.Decode();
    for (auto &group : transitionGroups) {
        for (auto &field : group.fields) {
            if (std::strcmp(transition.tokens[field.token], "m_savedC4BeepPhase") == 0) {
                field.bytes = {'\x7f', '\x00', '\x00', '\x00'};
            }
        }
    }
    transition.Encode(transitionGroups);
    std::memcpy(transition.data.szCurrentMapName, "source", sizeof("source"));
    const Vector originalOffset(7.0f, 8.0f, 9.0f);
    transition.data.vecLandmarkOffset = originalOffset;
    edict_t destination{};
    InitializeFixtureEntity(destination);
    destination.v.pContainingEntity = &destination;
    factory(&destination.v);
    destination.v.classname = entity.v.classname;
    destination.v.globalname = entity.v.globalname;
    destination.v.mins = Vector(-8.0f, -8.0f, -8.0f);
    overlayDestination = &destination;
    transition.Rewind();
    CHECK(api.pfnRestore(&entity, &transition.data, 1) == -1);
    CHECK(transition.data.vecLandmarkOffset == originalOffset);
    CHECK(removedEntity == &destination && destination.free && removedGlobalNameWasCleared);
    CHECK(!entity.free);
    SavedEntity afterFailure(entity);
    api.pfnSaveGlobalState(&afterFailure.data);
    afterFailure.Rewind();
    count.m_listCount = 0;
    std::memset(&globalRecord, 0, sizeof(globalRecord));
    api.pfnSaveReadFields(&afterFailure.data, "GLOBAL", &count, countFields, 1);
    api.pfnSaveReadFields(&afterFailure.data, "GENT", &globalRecord, globalFields, 3);
    CHECK(count.m_listCount == 1 && std::strcmp(globalRecord.levelName, "source") == 0);
    CHECK(globalRecord.state == GLOBAL_ON);
    CHECK(errors == 1);
    errors = 0;
    ordinary.data.vecLandmarkOffset = transition.data.vecLandmarkOffset;
    ordinary.data.fUseLandmark = 1;
    ordinary.Rewind();
    CHECK(api.pfnRestore(&entity, &ordinary.data, 0) != -1);
    CHECK(entity.v.origin == Vector(27.0f, 38.0f, 49.0f));
    api.pfnResetGlobalState();
    std::free(destination.pvPrivateData);
    std::free(entity.pvPrivateData);
    CHECK(errors == 0);
    std::puts("C4 production save, legacy import, phase advance and rejection checks passed");
    return true;
}
} // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }
    GameModule module(argv[1]);
    if (!module) {
        std::fprintf(stderr, "Cannot load owned CS module: %s\n", module.Error());
        return 2;
    }
    const bool passed = Probe(module);
    return passed ? 0 : 1;
}
