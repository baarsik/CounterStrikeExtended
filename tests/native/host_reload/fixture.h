#pragma once
// Reuse only the labelled in-memory filesystem and independent archive byte builders.
#define MAX_PATH 1024
#define ARCHIVE_OLD
#include "../save_archive/fixture.h"
#undef ARCHIVE_OLD
#include <array>

#define Q_memcpy std::memcpy
#define Q_memset std::memset
#define Q_strcpy std::strcpy
using qboolean = int;
using byte = unsigned char;
constexpr int src_command = 0;
constexpr int CAREER_NONE = 0;
constexpr int ca_active = 1;
constexpr int ca_connected = 2;
constexpr int ca_uninitialized = 3;
constexpr int ca_connecting = 4;
constexpr int ca_disconnected = 5;
constexpr int NA_UNUSED = 0;
constexpr int clc_stringcmd = 4;
constexpr int MAX_EFRAGS = 2;
constexpr int CommandBytes = 4096;
constexpr int EdictSlots = 8;

struct sizebuf_t {
    byte *data;
    int cursize;
    int maxsize;
};
struct Channel {
    struct Address {
        int type;
    } remote_address;
    int sock;
    sizebuf_t message;
};
struct edict_t {
    bool free;
    double freetime;
};
struct client_t {
    void *frames;
    int customdata;
};
struct Server {
    int active;
    int num_edicts;
    int max_edicts;
    edict_t *edicts;
    double time;
} inline sv{};
inline client_t client{};
struct ServerStatic {
    bool dll_initialized = true;
    int maxclients = 1;
    int maxclientslimit = 1;
    client_t *clients = &client;
} inline svs;
struct ClientStatic {
    int demonum;
    int demoplayback;
    int demorecording;
    int state;
    int connect_retry;
    int signon;
    int passive;
    int connect_stream;
    int game_stream;
    double connect_time;
    int timedemo;
    Channel netchan;
} inline cls{};
struct Efrag {
    Efrag *entnext;
};
struct ClientState {
    Efrag *free_efrags;
} inline cl{};
struct Cvar {
    float value;
    char *string;
};
inline char mapName[] = "fallback";
inline Cvar gHostMap{0, mapName};
inline Cvar scr_downloading{};
inline Cvar sys_timescale{};
inline Cvar sv_allow_autoaim{};
inline Cvar sv_allow_shaders{};
inline Efrag cl_efrags[MAX_EFRAGS]{};
inline int cl_dlights[1]{};
inline int cl_elights[1]{};
inline int cl_lightstyle[1]{};
inline int g_GameServerAddress = 0;
inline int g_careerState = 0;
inline float g_LastScreenUpdateTime = 0;
inline int host_hunklevel = 1;
inline int giStateInfo = 0;
inline int cmd_source = src_command;
inline bool cmd_wait = false;
inline char com_gamedir[] = "valve";
inline std::array<byte, CommandBytes> commands{};
inline std::array<byte, CommandBytes> untrustedCommands{};
inline sizebuf_t cmd_text{commands.data(), 0, CommandBytes};
inline sizebuf_t cmd_text_untrusted{untrustedCommands.data(), 0, CommandBytes};
inline std::unique_ptr<edict_t[]> allocation;
inline std::vector<std::string> events;
inline std::string recentSave = "slot";
inline bool headerValid = true;
inline bool spawnSucceeds = true;
inline int allocations = 0;
inline int retirements = 0;
inline int entityCallbacks = 0;
inline int activations = 0;
inline int mapped = 0;
inline int markers = 0;
inline int headerReads = 0;
struct GAME_HEADER {
    int mapCount = 1;
    char mapName[32] = "saved";
};

void Host_Reload_f();
int __cdecl Host_Load(const char *);
void Cbuf_AddText(const char *);
void Cbuf_Execute();
void CL_Disconnect();
void CL_Disconnect_f();
void CL_ClearState(qboolean);
void Host_ClearMemory(qboolean);
qboolean Host_IsServerActive();
edict_t *ED_Alloc();
void Cmd_ExecuteStringWithPrivilegeCheck(const char *, int, qboolean);

inline void Con_Printf(const char *, ...) {}
inline void Con_DPrintf(const char *, ...) {}
inline void Cvar_SetValue(const char *, int) {}
inline void Cvar_DirectSet(Cvar *value, const char *text) {
    value->value = static_cast<float>(std::atoi(text));
}
inline void SZ_Write(sizebuf_t *buffer, const void *data, int bytes) {
    check(bytes >= 0 && buffer->cursize + bytes <= buffer->maxsize, "command overflow");
    std::memcpy(buffer->data + buffer->cursize, data, bytes);
    buffer->cursize += bytes;
}
inline void SZ_Clear(sizebuf_t *buffer) {
    buffer->cursize = 0;
}
inline void Hunk_FreeToLowMark(int mark) {
    check(mark == host_hunklevel, "incorrect hunk mark");
    if (allocation) {
        ++retirements;
        events.emplace_back("retire edicts");
        allocation.reset();
    }
}
inline void ED_ClearEdict(edict_t *entity) {
    check(allocation && sv.edicts == allocation.get(), "allocation has expired");
    entity->free = false;
}
[[noreturn]] inline void Sys_Error(const char *message) {
    events.emplace_back(message);
    throw std::runtime_error(message);
}
inline int SaveReadHeader(FileHandle_t file, GAME_HEADER *header, int readGlobals) {
    check(readGlobals == 1, "global restore mode changed");
    ++headerReads;
    *header = {};
    if (!headerValid) {
        FS_Close(file); // Same failure ownership as production SaveReadHeader.
        return 0;
    }
    // Envelope parsing is outside this lifecycle fixture. Input starts at the archive.
    return 1;
}
inline const char *Host_FindRecentSave(char *, int) {
    return recentSave.empty() ? nullptr : recentSave.c_str();
}
inline void SV_ServerShutdown() {
    events.emplace_back("deactivate old server");
}
inline int SV_SpawnServer(qboolean demo, char *map, char *start) {
    check(!demo && map == gHostMap.string && !start, "fallback map arguments changed");
    events.emplace_back("spawn fallback");
    Host_ClearMemory(FALSE);
    if (!spawnSucceeds) {
        return 0;
    }
    allocation = std::make_unique<edict_t[]>(EdictSlots);
    ++allocations;
    sv.edicts = allocation.get();
    sv.max_edicts = EdictSlots;
    sv.num_edicts = svs.maxclients + 1;
    sv.active = false;
    events.emplace_back("allocate fallback edicts while inactive");
    return 1;
}
inline void SV_LoadEntities() {
    ++entityCallbacks;
    events.emplace_back("enter world game-rules callback");
    // Controlled world boundary reproduces InstallGameRules' queue/drain, then soundent allocation.
    Cbuf_AddText("exec game.cfg\n");
    Cbuf_Execute();
    events.emplace_back("allocate soundent");
    check(ED_Alloc() != nullptr, "soundent allocation failed");
}
inline void SV_ActivateServer(int runPhysics) {
    check(runPhysics == 1 && sv.max_edicts > 0 && sv.edicts == allocation.get(), "invalid activation");
    ++activations;
    sv.active = true;
}
inline void Host_Map(qboolean demo, char *, char *map, qboolean loadGame) {
    check(!demo && loadGame && std::strcmp(map, "saved") == 0, "successful load map changed");
    ++mapped;
    events.emplace_back("load saved map");
}
inline void Host_ShutdownServer(qboolean) {
    // Outside the reproducer: the fallback has sv.active=false, so this leaf is never reached there.
    events.emplace_back("shutdown active server");
    sv.active = false;
}
inline void Cmd_ExecuteStringWithPrivilegeCheck(const char *line, int source, qboolean privileged) {
    check(source == src_command && privileged, "command privilege changed");
    const std::string command(line);
    if (command == "reload") {
        Host_Reload_f();
    } else if (command == "disconnect") {
        events.emplace_back("execute disconnect");
        CL_Disconnect_f();
    } else if (command == "marker") {
        ++markers;
        events.emplace_back("unrelated marker");
    } else if (command == "exec game.cfg") {
        events.emplace_back("execute game.cfg");
    } else {
        check(command.empty(), "unexpected command");
    }
}

// Unrelated external/subsystem leaves. No platform, renderer, audio or game code is launched.
inline void Host_InitializeGameDLL() {}
inline void Host_ClearGameState() {}
inline void SV_InactivateClients() {}
inline void SCR_BeginLoadingPlaque(int) {}
inline void SetCareerAudioState(qboolean) {}
inline void SPR_Shutdown_NoModelFree() {}
inline void S_StopAllSounds(qboolean) {}
inline void CL_StopPlayback() {}
inline void CL_Stop_f() {}
inline void FS_LogLevelLoadStarted(const char *) {}
inline void NET_LeaveGroup(int, int) {}
inline void Netchan_Transmit(Channel *, int, byte *) {}
inline void Steam_GSTerminateGameConnection() {}
inline void CloseSecurityModule() {}
inline void NET_CheckCleanupFakeIPConnection(int, Channel::Address *) {}
inline void Netchan_Clear(Channel *) {}
inline void CL_DeallocateDynamicData() {}
inline void VGuiWrap2_NotifyOfServerDisconnect() {}
inline void StopLoadingProgressBar() {}
inline void CL_ClearClientState() {
    cl = {};
}
inline void CL_TempEntInit() {}
inline void CM_FreePAS() {}
inline void SV_ClearEntities() {}
inline void D_FlushCaches() {}
inline void Mod_ClearAll() {}
inline void SV_ClearFrames(void **) {}
inline void SV_ClearCaches() {}
inline void COM_ClearCustomizationList(int *, qboolean) {}
inline void SV_ClearResourceLists(client_t *) {}

#include "save_archive_impl.h"
#include "owners.h"
