#include <cstdio>
#include <cstring>

#include "feature_hooks.h"

#ifdef snprintf
#undef snprintf
#endif

namespace {

constexpr int ConfigCommandBytes = 512;
bool ConfigPending = false;

bool QueueConfig() {
    const char* directory = MF_GetLocalInfo("amxx_configsdir", "addons/amxmodx/configs");
    if (std::strpbrk(directory, "\"\r\n;")) {
        return false;
    }

    char command[ConfigCommandBytes];
    const int count = std::snprintf(command, sizeof(command), "exec \"%s/csextended.cfg\"\n", directory);
    if (count <= 0 || static_cast<std::size_t>(count) >= sizeof(command)) {
        return false;
    }
    g_engfuncs.pfnServerCommand(command);
    return true;
}

} // namespace

void CseAttach() {
    StatsAttach();
    FormsAttach();
}

int CseCheckGame(const char* game) {
    if (game && (std::strcmp(game, "cstrike") == 0 || std::strcmp(game, "czero") == 0)) {
        return AMXX_GAME_OK;
    }
    return AMXX_GAME_BAD;
}

void CseServerActivate(edict_t*, int, int) {
    StatsServerActivate();
    FormsServerActivate();
    RETURN_META(MRES_IGNORED);
}

void CseServerActivated(edict_t*, int, int) {
    StatsServerActivated();
    ConfigPending = QueueConfig();
    RETURN_META(MRES_IGNORED);
}

void CseServerDeactivate() {
    ConfigPending = false;
    StatsServerDeactivate();
    FormsServerDeactivate();
    RETURN_META(MRES_IGNORED);
}

void CseStartFrame() {
    if (ConfigPending) {
        ConfigPending = false;
        g_engfuncs.pfnServerExecute();
    }
    StatsStartFrame();
    FormsStartFrame();
    RETURN_META(MRES_IGNORED);
}

void CseClientEnter(edict_t* entity) {
    StatsClientEnter(entity);
    FormsClientEnter(entity);
    RETURN_META(MRES_IGNORED);
}

void CseClientLeave(edict_t* entity) {
    StatsClientLeave(entity);
    FormsClientLeave(entity);
    RETURN_META(MRES_IGNORED);
}
