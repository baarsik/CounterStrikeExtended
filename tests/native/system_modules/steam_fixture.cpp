#include "external/steamworks/steam_gameserver.h"

// No provider is loaded or initialized. Proxy shutdown sees the absent Steam service.
HSteamUser S_CALLTYPE SteamGameServer_GetHSteamUser() {
    return 0;
}
void *S_CALLTYPE SteamInternal_ContextInit(void *) {
    static void *absentInterface = nullptr;
    return &absentInterface;
}
void *S_CALLTYPE SteamInternal_FindOrCreateGameServerInterface(HSteamUser, const char *) {
    return nullptr;
}
bool S_CALLTYPE SteamInternal_GameServer_Init(uint32, uint16, uint16, uint16, EServerMode, const char *) {
    return false;
}
void S_CALLTYPE SteamGameServer_Shutdown() {}
void S_CALLTYPE SteamGameServer_RunCallbacks() {}
