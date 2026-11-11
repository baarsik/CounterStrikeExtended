// Appended to the verbatim production host.cpp prefix and Host_Shutdown by run_tests.py.
#include <cassert>
#include <vector>
#include <string>
client_static_t cls{};
client_state_t cl{};
server_t sv{};
server_static_t svs{};
char *wadpath;
static std::vector<std::string> order;
void Host_ShutdownServer(qboolean) {
    order.push_back("drain");
    Host_Shutdown();
}
void Steam_Shutdown() {
    order.push_back("steam");
}
void NET_Shutdown() {
    order.push_back("network");
}
void ReleaseEntityDlls() {
    order.push_back("dll");
}
void Host_WriteConfiguration() {}
void SV_ServerShutdown() {}
void Voice_Deinit() {}
void CDAudio_Shutdown() {}
void VGui_Shutdown() {}
void ClientDLL_Shutdown() {}
void Cmd_RemoveGameCmds() {}
void Cmd_Shutdown() {}
void Cvar_ResetList() {}
void HPAK_FlushHostQueue() {}
void SV_DeallocateDynamicData() {}
void SV_ClearFrames(client_frame_t **) {}
void SV_Shutdown() {}
void SystemWrapper_ShutDown() {}
void S_Shutdown() {}
void Con_Shutdown() {}
void CL_ShutDownClientStatic() {}
void CM_FreePAS() {}
void Mem_Free(void *) {}
void Draw_Shutdown() {}
void Draw_DecalShutdown() {}
void W_Shutdown() {}
void Log_Printf(const char *, ...) {}
void Log_Close() {}
void COM_Shutdown() {}
void CL_Shutdown() {}
void DELTA_Shutdown() {}
void Key_Shutdown() {}
int main() {
    cls.state = ca_dedicated;
    host_initialized = true;
    Host_Shutdown();
    const std::vector<std::string> expected{"drain", "steam", "network", "dll"};
    assert(order == expected);
    Host_Shutdown();
    assert(order == expected);
    assert(!host_initialized);
}
extern "C" void CseSteamModuleInitialize() {}
