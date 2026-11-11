#include <ctime>
#include <cstring>
struct quakeparms_t {
    int argc = 0;
    char **argv = nullptr;
    void *membase = nullptr;
    int memsize = 0;
    char *basedir = nullptr;
};
quakeparms_t host_parms;
int com_argc = 0;
char **com_argv = nullptr;
int console = 0;
int suitvolume = 0;
int g_module = 0;
int g_clmove = 0;
int host_hunklevel = 0;
int giActive = 0;
int scr_skipupdate = 0;
bool bLowViolenceBuild = false;
bool hostVideoReady = true;
unsigned short *host_basepal = nullptr;
const char *gpszVersionString = "fixture";
constexpr int PROTOCOL_VERSION = 48;
char com_empty_string[] = "";
char com_gamedir[] = "cstrike";
#define INIT_LEAF(name)                                                                                                \
    template <class... Arguments> void name(Arguments &&...) {                                                         \
        Event(#name);                                                                                                  \
    }
INIT_LEAF(Memory_Init)
INIT_LEAF(Voice_RegisterCvars)
INIT_LEAF(Cvar_RegisterVariable)
INIT_LEAF(Cvar_DirectSet)
INIT_LEAF(Host_InitLocal)
INIT_LEAF(Cvar_SetValue)
INIT_LEAF(Cbuf_Init)
INIT_LEAF(Cmd_Init)
INIT_LEAF(Cvar_Init)
INIT_LEAF(Cvar_CmdInit)
INIT_LEAF(V_Init)
INIT_LEAF(Chase_Init)
INIT_LEAF(COM_Init)
INIT_LEAF(Host_ClearSaveDirectory)
INIT_LEAF(HPAK_Init)
INIT_LEAF(W_LoadWadFile)
INIT_LEAF(Key_Init)
INIT_LEAF(Con_Init)
INIT_LEAF(Decal_Init)
INIT_LEAF(Mod_Init)
INIT_LEAF(NET_Init)
INIT_LEAF(Netchan_Init)
INIT_LEAF(DELTA_Init)
INIT_LEAF(SV_Init)
INIT_LEAF(SystemWrapper_Init)
INIT_LEAF(Host_Version)
INIT_LEAF(Cvar_Set)
INIT_LEAF(Con_DPrintf)
INIT_LEAF(R_InitTextures)
INIT_LEAF(HPAK_CheckIntegrity)
INIT_LEAF(GL_Init)
INIT_LEAF(PM_Init)
INIT_LEAF(CL_InitEventSystem)
INIT_LEAF(ClientDLL_Init)
INIT_LEAF(VGui_Startup)
INIT_LEAF(Draw_Init)
INIT_LEAF(SCR_Init)
INIT_LEAF(R_Init)
INIT_LEAF(S_Init)
INIT_LEAF(CDAudio_Init)
INIT_LEAF(Voice_Init)
INIT_LEAF(DemoPlayer_Init)
INIT_LEAF(CL_Init)
INIT_LEAF(Cbuf_InsertText)
INIT_LEAF(GL_Config)
INIT_LEAF(Sys_GetRegKeyValue)
INIT_LEAF(Steam_ProcessInsecureLaunchParameters)
#undef INIT_LEAF
int COM_CheckParm(const char *) {
    return 0;
}
int build_number() {
    return 1;
}
#define _snprintf std::snprintf
#define Q_memset std::memset
#define Q_strcpy std::strcpy
#define Q_strlen std::strlen
char *COM_LoadHunkFile(char *) {
    constexpr size_t PaletteBytes = 256 * 3;
    static char bytes[PaletteBytes]{};
    return bytes;
}
void *Hunk_AllocName(size_t, const char *) {
    constexpr size_t PaletteComponents = 256 * 4;
    static unsigned short entries[PaletteComponents]{};
    return entries;
}
int Hunk_LowMark() {
    return 0;
}
void Sys_Error(const char *) {
    throw std::runtime_error("unexpected fatal host fixture initialization");
}
int VID_Init(unsigned short *) {
    Event("VID_Init");
    return hostVideoReady;
}
#include "host_init.inc"
#undef _snprintf
#undef Q_memset
#undef Q_strcpy
#undef Q_strlen
