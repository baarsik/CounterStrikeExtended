#include "module.h"

namespace cse::extstats {

static cell AMX_NATIVE_CALL SetHealth(AMX* amx, cell* params) {
    if (params[0] < static_cast<cell>(2 * sizeof(cell)) || !State.SetHealth(params[1], params[2], MessageOpen)) {
        MF_LogError(amx, AMX_ERR_NATIVE, "Invalid player or health setter arguments");
        return 0;
    }
    return 1;
}

static cell AMX_NATIVE_CALL SetArmor(AMX* amx, cell* params) {
    if (params[0] < static_cast<cell>(2 * sizeof(cell)) || !State.SetArmor(params[1], params[2], MessageOpen)) {
        MF_LogError(amx, AMX_ERR_NATIVE, "Invalid player or negative armor amount");
        return 0;
    }
    return 1;
}

static cell AMX_NATIVE_CALL SyncStats(AMX* amx, cell* params) {
    if (params[0] < static_cast<cell>(sizeof(cell)) || !State.PublishStats(params[1], true)) {
        MF_LogError(amx, AMX_ERR_NATIVE, "Invalid player for stat synchronization");
        return 0;
    }
    return 1;
}

static cell AMX_NATIVE_CALL IsSupported(AMX*, cell* params) {
    return params[0] >= static_cast<cell>(sizeof(cell)) && State.IsCseClient(params[1]);
}

AMX_NATIVE_INFO Natives[] = {{"cse_set_user_health", SetHealth},
                             {"cse_set_user_armor", SetArmor},
                             {"cse_sync_user_stats", SyncStats},
                             {"cse_is_supported", IsSupported},
                             {nullptr, nullptr}};

} // namespace cse::extstats
