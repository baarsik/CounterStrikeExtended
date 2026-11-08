#ifndef CSE_STEAM_SERVICE_H
#define CSE_STEAM_SERVICE_H

#include "../external/steamworks/steam_gameserver.h"

S_API bool S_CALLTYPE SteamAPI_Init();
S_API void S_CALLTYPE SteamAPI_Shutdown();
S_API void S_CALLTYPE SteamAPI_SetBreakpadAppID(uint32 application);
// Signatures pinned to Valve source-sdk-2013, including bool and the typed callback.
S_API void S_CALLTYPE SteamAPI_UseBreakpadCrashHandler(const char* version, const char* date, const char* time,
    bool fullMemoryDumps, void* context, PFNPreMinidumpCallback preMinidumpCallback);
S_API void S_CALLTYPE SteamAPI_SetMiniDumpComment(const char* comment);
S_API void S_CALLTYPE SteamAPI_WriteMiniDump(uint32 exceptionCode, void* exceptionInfo, uint32 build);

#endif
