#include <cstdio>
#include <cstring>
#include "module.h"

namespace cse::extstats {

enginefuncs_t* HookedEngine = nullptr;
bool Publishing = false;
bool MessageOpen = false;
int HealthId = 0;
int ArmorId = 0;
int SpecHealthId = 0;
int SpecHealth2Id = 0;
int HltvId = 0;
int ResetHudId = 0;
int InitHudId = 0;
static int HealthExId = 0;
static int ArmorExId = 0;
static bool Active = false;
static bool ConfigPending = false;
static cvar_t AutoObserver = {const_cast<char*>("cse_extstats_auto"), const_cast<char*>("1"), 0, 1, nullptr};

Player ReadEntityPlayer(int index) {
    Player result;
    if (!gpGlobals || index < 1 || index > gpGlobals->maxClients || index > MaxPlayerIndex ||
        !MF_IsPlayerIngame(index)) {
        return result;
    }
    const edict_t* entity = MF_GetPlayerEdict(index);
    if (!entity || entity->free) {
        return result;
    }
    result.connected = true;
    result.bot = MF_IsPlayerBot(index) != 0;
    result.proxy = MF_IsPlayerHLTV(index) != 0;
    result.alive = entity->v.deadflag == DEAD_NO && entity->v.health > 0;
    result.health = entity->v.health;
    result.armor = entity->v.armorvalue;
    if (!result.alive && entity->v.iuser1 != 0) {
        result.target = entity->v.iuser2;
    }
    return result;
}

class EngineHost final : public ServerHost {
  public:
    int MaxPlayers() const override {
        return gpGlobals ? gpGlobals->maxClients : 0;
    }
    Player ReadPlayer(int index) const override {
        return ReadEntityPlayer(index);
    }
    void AssignHealth(int index, float amount) override {
        MF_GetPlayerEdict(index)->v.health = amount;
    }
    void AssignArmor(int index, float amount) override {
        MF_GetPlayerEdict(index)->v.armorvalue = amount;
    }
    void Kill(int index) override {
        gpGamedllFuncs->dllapi_table->pfnClientKill(MF_GetPlayerEdict(index));
    }
    void Query(int index, const char* name, int request) override {
        if (g_engfuncs.pfnQueryClientCvarValue2) {
            g_engfuncs.pfnQueryClientCvarValue2(MF_GetPlayerEdict(index), name, request);
        }
    }
    void Emit(int recipient, int subject, Stat stat, std::int32_t amount) override {
        const int message = stat == Stat::Health ? HealthExId : ArmorExId;
        if (!message || !HookedEngine) {
            return;
        }
        Publishing = true;
        HookedEngine->pfnMessageBegin(recipient ? MSG_ONE : MSG_SPEC, message, nullptr,
                                      recipient ? MF_GetPlayerEdict(recipient) : nullptr);
        HookedEngine->pfnWriteByte(subject);
        HookedEngine->pfnWriteLong(amount);
        HookedEngine->pfnMessageEnd();
        Publishing = false;
    }
    void EmitLegacy(int recipient, int subject, std::int32_t health, std::int32_t armor) override {
        if (!HookedEngine || MF_IsPlayerBot(recipient)) {
            return;
        }
        const int safeHealth = health > LegacyHealthMaximum ? LegacyHealthMaximum : health;
        const int safeArmor = armor > LegacyArmorMaximum ? LegacyArmorMaximum : armor;
        Publishing = true;
        edict_t* entity = MF_GetPlayerEdict(recipient);
        if (recipient == subject && HealthId && ArmorId) {
            HookedEngine->pfnMessageBegin(MSG_ONE, HealthId, nullptr, entity);
            HookedEngine->pfnWriteByte(safeHealth);
            HookedEngine->pfnMessageEnd();
            HookedEngine->pfnMessageBegin(MSG_ONE, ArmorId, nullptr, entity);
            HookedEngine->pfnWriteShort(safeArmor);
            HookedEngine->pfnMessageEnd();
        } else if (SpecHealth2Id) {
            HookedEngine->pfnMessageBegin(MSG_ONE, SpecHealth2Id, nullptr, entity);
            HookedEngine->pfnWriteByte(safeHealth);
            HookedEngine->pfnWriteByte(subject);
            HookedEngine->pfnMessageEnd();
        }
        Publishing = false;
    }
};

static EngineHost Host;
ServerState State(Host);

static void ResolveLegacyMessages() {
    HealthId = GET_USER_MSG_ID(PLID, "Health", nullptr);
    ArmorId = GET_USER_MSG_ID(PLID, "Battery", nullptr);
    SpecHealthId = GET_USER_MSG_ID(PLID, "SpecHealth", nullptr);
    SpecHealth2Id = GET_USER_MSG_ID(PLID, "SpecHealth2", nullptr);
    HltvId = GET_USER_MSG_ID(PLID, "HLTV", nullptr);
    ResetHudId = GET_USER_MSG_ID(PLID, "ResetHUD", nullptr);
    InitHudId = GET_USER_MSG_ID(PLID, "InitHUD", nullptr);
}

static bool RegisterMessages() {
    HealthExId = 0;
    ArmorExId = 0;
    HealthId = 0;
    ArmorId = 0;
    SpecHealthId = 0;
    SpecHealth2Id = 0;
    HltvId = 0;
    ResetHudId = 0;
    InitHudId = 0;
    if (!HookedEngine) {
        MF_Log("Extended stats disabled: Metamod hook tables are unavailable");
        return false;
    }
    const char* names[] = {HealthMessage, ArmorMessage};
    for (const char* name : names) {
        int size = 0;
        if (GET_USER_MSG_ID(PLID, name, &size) != 0 && size != MessageBytes) {
            MF_Log("Extended stats disabled: existing %s has incompatible size %d", name, size);
            return false;
        }
    }
    HealthExId = HookedEngine->pfnRegUserMsg(HealthMessage, MessageBytes);
    ArmorExId = HookedEngine->pfnRegUserMsg(ArmorMessage, MessageBytes);
    if (!HealthExId || !ArmorExId) {
        MF_Log("Extended stats disabled: user-message registration failed");
        return false;
    }
    return true;
}

} // namespace cse::extstats

void StatsAttach() {
    using namespace cse::extstats;
    MF_AddNatives(Natives);
    g_engfuncs.pfnCVarRegister(&AutoObserver);
    GET_HOOK_TABLES(PLID, &HookedEngine, nullptr, nullptr);
}

int StatsCheckGame(const char* game) {
    return game && (std::strcmp(game, "cstrike") == 0 || std::strcmp(game, "czero") == 0) ? AMXX_GAME_OK
                                                                                          : AMXX_GAME_BAD;
}

void StatsServerActivate(edict_t*, int, int) {
    using namespace cse::extstats;
    State.Reset();
    ResetLegacyMessage();
    // Register before AMXX's post-activation plugin_init callbacks can look up the IDs.
    RegisterMessages();
    // Identity queries remain available even when a conflicting Ex message disables publication.
    Active = true;
    ConfigPending = true;
    RETURN_META(MRES_IGNORED);
}

void StatsServerActivated(edict_t*, int, int) {
    cse::extstats::ResolveLegacyMessages();
    RETURN_META(MRES_IGNORED);
}

void StatsServerDeactivate() {
    using namespace cse::extstats;
    Active = false;
    State.Reset();
    ResetLegacyMessage();
    RETURN_META(MRES_IGNORED);
}

void StatsStartFrame() {
    using namespace cse::extstats;
    if (Active && ConfigPending) {
        ConfigPending = false;
        const char* directory = MF_GetLocalInfo("amxx_configsdir", "addons/amxmodx/configs");
        char command[512];
        // Config directories are server-controlled, but never permit command injection through a path.
        if (!std::strpbrk(directory, "\"\r\n;")) {
            std::snprintf(command, sizeof(command), "exec \"%s/cse_extstats.cfg\"\n", directory);
            g_engfuncs.pfnServerCommand(command);
            g_engfuncs.pfnServerExecute();
        }
    }
    if (Active) {
        State.Frame(gpGlobals->time, AutoObserver.value != 0, MessageOpen);
    }
    RETURN_META(MRES_IGNORED);
}

void StatsClientEnter(edict_t* entity) {
    if (cse::extstats::Active) {
        cse::extstats::State.Connect(ENTINDEX(entity), gpGlobals->time);
    }
    RETURN_META(MRES_IGNORED);
}

void StatsClientLeave(edict_t* entity) {
    cse::extstats::State.Disconnect(ENTINDEX(entity));
    RETURN_META(MRES_IGNORED);
}

void StatsCvarReply(const edict_t* entity, int request, const char* name, const char* value) {
    cse::extstats::State.Reply(ENTINDEX(entity), request, name, value, gpGlobals->time);
    RETURN_META(MRES_IGNORED);
}
