// Explicit owned game-DLL fixture. Calls save/restore directly; no world, browser, network or media starts.
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "cse_module_abi.h"
#include "game_module_entry.h"
#include "generated_game_module_layouts.h"
#include "../save_fields/owned_entity_stream.h"

enginefuncs_t g_engfuncs{};
globalvars_t *gpGlobals = nullptr;

// Owned buffer views; serialization and entity Save/Restore execute inside the loaded production DLL.
CSaveRestoreBuffer::CSaveRestoreBuffer(SAVERESTOREDATA *data) : m_pdata(data) {}
CSaveRestoreBuffer::~CSaveRestoreBuffer() {}

namespace {
std::array<char, 8192> strings{};
std::size_t stringCursor = 1;
unsigned recoveryCalls = 0;
unsigned errors = 0;

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "World string check failed at line %d: %s\n", __LINE__, #condition); \
        return false; \
    } \
} while (false)

string_t AllocateString(const char *text) {
    const auto bytes = std::strlen(text) + 1;
    if (bytes > strings.size() - stringCursor) {
        std::abort();
    }
    const auto offset = stringCursor;
    std::memcpy(strings.data() + offset, text, bytes);
    stringCursor += bytes;
    return static_cast<string_t>(offset);
}

void *AllocatePrivate(edict_t *entity, int32 bytes) {
    entity->pvPrivateData = std::calloc(1, static_cast<std::size_t>(bytes));
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

string_t RecoverWorldString(const char *key) {
    if (std::strcmp(key, "mapteams") != 0) {
        std::abort();
    }
    ++recoveryCalls;
    return AllocateString("bsp-red;bsp-blue");
}

bool Probe(HMODULE module) {
    const auto initialize = reinterpret_cast<CseInitializeGame>(GetProcAddress(module, "CseInitializeGameModule"));
    const auto describe = reinterpret_cast<cse::abi::GetModuleAbi>(GetProcAddress(module, "CseGetModuleAbi"));
    const auto factory = reinterpret_cast<void (*)(entvars_t *)>(GetProcAddress(module, "worldspawn"));
    CHECK(initialize && describe && factory);
    CHECK(cse::abi::Check(cse::abi::GameModuleDescriptor, describe()) == cse::abi::Compatibility::Compatible);
    g_engfuncs.pfnPvAllocEntPrivateData = AllocatePrivate;
    g_engfuncs.pfnPvEntPrivateData = PrivateData;
    g_engfuncs.pfnAllocString = AllocateString;
    g_engfuncs.pfnAlertMessage = Alert;
    g_engfuncs.pfnGetWorldspawnString = RecoverWorldString;
    globalvars_t globals{};
    globals.pStringBase = strings.data();
    gpGlobals = &globals;
    CHECK(initialize(&cse::abi::GameModuleDescriptor, &g_engfuncs, &globals));
    edict_t entity{};
    entity.v.pContainingEntity = &entity;
    entity.v.classname = AllocateString("worldspawn");
    factory(&entity.v);
    auto *world = static_cast<CWorld *>(entity.pvPrivateData);
    CHECK(world);
    constexpr int NumericTeam = 37;
    entity.v.team = NumericTeam;
    for (const char *value : {"saved-red;saved-blue", ""}) {
        KeyValueData key{const_cast<char *>("worldspawn"), const_cast<char *>("mapteams"),
                         const_cast<char *>(value), FALSE};
        world->KeyValue(&key);
        CHECK(std::strcmp(STRING(world->MapTeamList()), value) == 0);
        CHECK(entity.v.team == NumericTeam);
        SavedEntity saved(entity);
        CSave writer(&saved.data);
        CHECK(world->Save(writer));
        world->m_worldMapTeams = AllocateString("changed");
        world->m_worldMapTeamsKnown = false;
        saved.Rewind();
        CRestore reader(&saved.data);
        CHECK(world->Restore(reader));
        CHECK(world->m_worldMapTeamsKnown);
        recoveryCalls = 0;
        world->EnsureMapTeamList();
        CHECK(recoveryCalls == 0 && std::strcmp(STRING(world->MapTeamList()), value) == 0);
        CHECK(entity.v.team == NumericTeam);
    }
    // Simulate a legacy BASE group with neither optional world field; never decode the old raw team bits.
    SavedEntity legacy(entity);
    CSave writer(&legacy.data);
    CHECK(world->Save(writer));
    auto groups = legacy.Decode();
    for (auto &group : groups) {
        group.fields.erase(std::remove_if(group.fields.begin(), group.fields.end(), [&](const Field &field) {
            const char *name = legacy.tokens[field.token];
            return std::strcmp(name, "m_worldMapTeams") == 0 || std::strcmp(name, "m_worldMapTeamsKnown") == 0;
        }), group.fields.end());
    }
    legacy.Encode(groups);
    legacy.Rewind();
    CRestore reader(&legacy.data);
    CHECK(world->Restore(reader));
    CHECK(!world->m_worldMapTeamsKnown && world->MapTeamList() == 0);
    recoveryCalls = 0;
    world->EnsureMapTeamList();
    CHECK(recoveryCalls == 1 && world->m_worldMapTeamsKnown);
    CHECK(std::strcmp(STRING(world->MapTeamList()), "bsp-red;bsp-blue") == 0);
    world->EnsureMapTeamList();
    CHECK(recoveryCalls == 1 && entity.v.team == NumericTeam && errors == 0);
    std::free(entity.pvPrivateData);
    std::puts("Production world mapteams save, explicit empty and legacy recovery passed");
    return true;
}
} // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }
    HMODULE module = LoadLibraryA(argv[1]);
    if (!module) {
        return 2;
    }
    const bool passed = Probe(module);
    FreeLibrary(module);
    return passed ? 0 : 1;
}
