#include "check.h"
#include "native_gameui_retirement.h"
#include "vgui_style_service.h"
#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <map>
#include <new>
#include <string>
#include <vector>

namespace {
std::vector<std::string> events;
void Event(const char *name) {
    events.emplace_back(name);
}
int Count(const char *name) {
    return static_cast<int>(std::count(events.begin(), events.end(), name));
}

void CSE_ABI_CALL CSE_VGui1BindStyle(const cse::ui::StyleApi* style) {
    Require(style == nullptr, "VGUI1 retirement must clear its style service");
    Event("style.unbound");
}
} // namespace

// Apple wrapper controls are compiled only by the actual Apple toolchain.
#if defined(__APPLE__)
namespace cse_vgui1_native {
Result detachResult = Result::Busy;
Result DetachEmbedded(EngineSurfaceWrap *) noexcept {
    Event("detach");
    return detachResult;
}
} // namespace cse_vgui1_native
#endif

class EngineSurfaceWrap {
public:
    ~EngineSurfaceWrap() {
        Event("wrapper1.deleted");
    }
};
void *staticPanel = nullptr;
EngineSurfaceWrap *staticEngineSurface = nullptr;
struct UI {
    void Shutdown() {
        Event("wrapper2.shutdown");
    }
};
UI ui;
UI *staticUIFuncs = &ui;
class EngineSurface {
public:
    static void freeEngineSurface() {
        Event("surface.free");
    }
};

using qboolean = int;
qboolean hostShutdownStarted = 0;
qboolean host_initialized = 1;
constexpr int ca_dedicated = 9;
constexpr int DLL_INACTIVE = 0;
bool g_bIsDedicatedServer = false;
struct ClientStatic {
    int state = 0;
} cls;
struct Frames {};
struct Client {
    Frames frames;
};
struct ServerStatic {
    int maxclientslimit = 1;
    Client clients[1];
} svs;
struct TimeState {
    double time = 1;
};
TimeState sv;
TimeState cl;
double realtime = 1;
void *wadpath = nullptr;

namespace cse_diagnostics {
struct Timings {
    void Flush() {
        Event("timings.flush");
    }
};
Timings &NativeFrameTimings() {
    static Timings timings;
    return timings;
}
} // namespace cse_diagnostics

#define OBSERVE_VOID(name)                                                                                             \
    void name() {                                                                                                      \
        Event(#name);                                                                                                  \
    }
OBSERVE_VOID(Host_ResetNativeVideoRestart)
OBSERVE_VOID(Host_WriteConfiguration)
OBSERVE_VOID(Steam_Shutdown)
OBSERVE_VOID(SV_ServerShutdown)
OBSERVE_VOID(Voice_Deinit)
OBSERVE_VOID(CDAudio_Shutdown)
OBSERVE_VOID(ClientDLL_Shutdown)
OBSERVE_VOID(Cmd_RemoveGameCmds)
OBSERVE_VOID(Cmd_Shutdown)
OBSERVE_VOID(Cvar_ResetList)
OBSERVE_VOID(HPAK_FlushHostQueue)
OBSERVE_VOID(SV_DeallocateDynamicData)
OBSERVE_VOID(SV_Shutdown)
OBSERVE_VOID(SystemWrapper_ShutDown)
OBSERVE_VOID(NET_Shutdown)
OBSERVE_VOID(S_Shutdown)
OBSERVE_VOID(Con_Shutdown)
OBSERVE_VOID(ReleaseEntityDlls)
OBSERVE_VOID(CL_ShutDownClientStatic)
OBSERVE_VOID(CM_FreePAS)
OBSERVE_VOID(Draw_Shutdown)
OBSERVE_VOID(Draw_DecalShutdown)
OBSERVE_VOID(W_Shutdown)
OBSERVE_VOID(Log_Close)
OBSERVE_VOID(COM_Shutdown)
OBSERVE_VOID(CL_Shutdown)
OBSERVE_VOID(DELTA_Shutdown)
OBSERVE_VOID(Key_Shutdown)
OBSERVE_VOID(ClientDLL_DeactivateMouse)
OBSERVE_VOID(Sys_ShutdownMemory)
OBSERVE_VOID(Sys_Shutdown)
OBSERVE_VOID(Steam_ShutdownClient)
#undef OBSERVE_VOID

void Host_ShutdownServer(bool) {
    Event("Host_ShutdownServer");
}
void SV_ClearFrames(Frames *) {
    Event("SV_ClearFrames");
}
void Mem_Free(void *pointer) {
    std::free(pointer);
}
void Log_Printf(const char *) {}
void TraceShutdown(const char *, int) {}
void NET_Config(bool) {
    Event("NET_Config");
}

void Sys_ShutdownGame();

class CEngine {
public:
    void Unload();
    bool TryUnload();
    int m_nDLLState = 1;
};

cse::NativeGameInitState cse::nativeGameInit;
bool safePoint = true;
#if defined(__APPLE__)
bool browserReady = true;
#else
bool browserReady = false;
#endif
bool cse::NativeGameUIIsRetirementSafePoint() {
    return safePoint;
}
bool cse::TryRetireNativeGameUI() {
    Event("browser.retire");
    return browserReady;
}
bool cse::TryShutdownNativeBaseUI() {
    staticUIFuncs->Shutdown();
    return true;
}
#include "retirement_state.inc"
#include "retirement.inc"
#include "host_init_fixture.h"

namespace {
struct Allocations {
    Allocations() {
        staticPanel = ::operator new(1);
        staticEngineSurface = new EngineSurfaceWrap;
    }
    ~Allocations() {
        // Test-only cleanup after observing the actual owner's behavior, even on an expected RED.
        ::operator delete(staticPanel);
        staticPanel = nullptr;
        delete staticEngineSurface;
        staticEngineSurface = nullptr;
    }
};

void RequireRetainedVGui() {
    Require(Count("style.unbound") == 0, "refused detach cleared the active VGUI1 style service");
    Require(staticPanel != nullptr, "refused detach lost wrapper1 root");
    Require(staticEngineSurface != nullptr, "refused detach lost wrapper1 surface");
    Require(Count("wrapper2.shutdown") == 0, "VGUI2 retired after VGUI1 refusal");
    Require(Count("surface.free") == 0, "EngineSurface freed after VGUI1 refusal");
    Require(staticUIFuncs == &ui, "BaseUI pointer cleared after VGUI1 refusal");
}

void RequireRetainedHost() {
    RequireRetainedVGui();
    Require(Count("ClientDLL_Shutdown") == 0, "client image retired after VGUI refusal");
    Require(Count("Cmd_Shutdown") == 0, "command owner retired after VGUI refusal");
    Require(Count("Cvar_ResetList") == 0, "cvars retired after VGUI refusal");
    Require(Count("NET_Shutdown") == 0, "network owner retired after VGUI refusal");
}
} // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }
    const std::string name = argv[1];
    return RunCase([&] {
        Allocations allocations;
        nativeVGuiShutdownPhase = NativeVGuiShutdownPhase::Browser;
        cse::ResetNativeHostShutdown();
        cse::SetNativeHostInitPhase(cse::NativeHostInitPhase::Complete);
        cse::nativeGameInit.sysInitialized = true;
        cse::nativeGameInit.memoryAllocated = true;
        cse::nativeGameInit.hostAttempted = true;
        cse::nativeGameInit.clientMouseActivated = true;
        if (name == "vid-failure" || name == "dedicated-init") {
            nativeHostShutdownPhase = NativeHostShutdownPhase::Complete;
            cse::ResetNativeHostShutdown();
            host_initialized = false;
            const bool dedicated = name == "dedicated-init";
            g_bIsDedicatedServer = dedicated;
            cls.state = dedicated ? ca_dedicated : 0;
            hostVideoReady = false;
            quakeparms_t parameters{};
            const int result = Host_Init(&parameters);
            if (dedicated) {
                Require(result == 1 && Count("VGui_Startup") == 0 && Count("ClientDLL_Init") == 0 &&
                            nativeHostInitPhase == cse::NativeHostInitPhase::NotStarted,
                        "dedicated Host initialization selected native listen/VGUI ownership");
            } else {
                Require(result == 0 && nativeHostInitPhase == cse::NativeHostInitPhase::VGuiStarted &&
                            Count("VGui_Startup") == 1 && Count("Draw_Init") == 0,
                        "VID failure lost the actual partial VGUI checkpoint");
                Require(!cse::TryShutdownNativeHost(), "partial VGUI refusal authorized Host destruction");
                RequireRetainedHost();
#if defined(__APPLE__)
                cse_vgui1_native::detachResult = cse_vgui1_native::Result::Ready;
#else
                browserReady = true;
#endif
                Require(cse::TryShutdownNativeHost(), "partial Host failed to resume after actual VGUI retirement");
                Require(Count("Voice_Deinit") == 0 && Count("CDAudio_Shutdown") == 0 && Count("Draw_Shutdown") == 0 &&
                            Count("S_Shutdown") == 0 && Count("ClientDLL_Shutdown") == 1,
                        "partial VID failure retired never-created media/draw owners or lost entered client");
            }
        } else if (name == "vgui-refusal") {
            VGui_Shutdown();
            RequireRetainedVGui();
        } else if (name == "host-refusal" || name == "release-failed") {
            if (name == "release-failed") {
#if defined(__APPLE__)
                cse_vgui1_native::detachResult = cse_vgui1_native::Result::ReleaseFailed;
#else
                throw std::runtime_error("Apple wrapper failure is not a Windows branch control");
#endif
            }
            Host_Shutdown();
            RequireRetainedHost();
        } else if (name == "sys-refusal") {
            Sys_ShutdownGame();
            RequireRetainedHost();
            Require(Count("Sys_ShutdownMemory") == 0, "memory retired after Host refusal");
            Require(Count("Sys_Shutdown") == 0, "Steam client retired after Host refusal");
        } else if (name == "engine-refusal") {
            CEngine engine;
            engine.Unload();
            Require(engine.m_nDLLState != DLL_INACTIVE, "engine published inactive after refused retirement");
            RequireRetainedHost();
        } else if (name == "retry-once") {
            Host_Shutdown();
            RequireRetainedHost();
#if defined(__APPLE__)
            cse_vgui1_native::detachResult = cse_vgui1_native::Result::Ready;
#else
            browserReady = true;
#endif
            Host_Shutdown();
#if defined(__APPLE__)
            Require(Count("Host_ShutdownServer") == 1, "retry repeated server teardown");
#else
            Require(Count("Host_ShutdownServer") == 0, "retry invoked Apple-only server teardown");
#endif
            Require(Count("SV_ServerShutdown") == 1, "retry did not retire shared server exactly once");
            Require(Count("Voice_Deinit") == 1, "retry repeated voice teardown");
            Require(Count("CDAudio_Shutdown") == 1, "retry repeated CD teardown");
            Require(Count("wrapper1.deleted") == 1, "retry did not retire wrapper1 exactly once");
            Require(Count("style.unbound") == 1, "retry did not release the VGUI1 style service exactly once");
            Require(Count("ClientDLL_Shutdown") == 1, "retry did not resume later Host cleanup");
        } else if (name == "ready") {
#if defined(__APPLE__)
            cse_vgui1_native::detachResult = cse_vgui1_native::Result::Ready;
#else
            browserReady = true;
#endif
            CEngine engine;
            engine.Unload();
            Require(engine.m_nDLLState == DLL_INACTIVE, "ready shutdown failed to publish inactive");
            Require(Count("wrapper1.deleted") == 1 && Count("wrapper2.shutdown") == 1,
                    "ready shutdown did not retire both wrappers");
            Require(Count("surface.free") == 1 && Count("Sys_Shutdown") == 1,
                    "ready shutdown failed to retire later owners");
        } else {
            throw std::runtime_error("unknown retirement case");
        }
    });
}
