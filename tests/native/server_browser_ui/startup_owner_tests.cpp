#include "check.h"
#include "native_gameui_retirement.h"
#include <cstring>
#include <map>
#include <string>

#include <clocale>
#include <cstdio>
#include <cstdint>

#if !defined(_WIN32) && CSE_TEST_HARDWARE
constexpr size_t kLocaleMessageBytes = 1024;
int Q_strcasecmp(const char *left, const char *right) {
    return std::strcmp(left, right);
}
// Device UI is a controlled leaf, while the real native locale branch remains selected.
int SDL_ShowSimpleMessageBox(unsigned int, const char *, const char *, void *) {
    return 0;
}
#endif

namespace {
std::map<std::string, int> calls;
int fixtureHostInitResult = 1;
bool hostFlag = true;
int modeResult = 1;
void Call(const char *name) {
    ++calls[name];
}
} // namespace

struct SDL_Window {};
using CseGameWindowHandle = SDL_Window *;
SDL_Window **pmainwindow = nullptr;
bool host_initialized = false;
int g_bIsDedicatedServer = 0;
struct Cvar {
    float value = 0;
} developer;
struct ModInfo {
    int value = 0;
} gmodinfo;
struct HostParms {
    void *membase = nullptr;
} host_parms;
cse::NativeGameInitState cse::nativeGameInit;
void *maindc = nullptr;
void *baseRC = nullptr;
bool gD3DMode = false;
struct VideoMode {
    void UpdateWindowPosition() {
        Call("window-position");
    }
};
VideoMode video;
VideoMode *videomode = &video;
int COM_CheckParm(const char *) {
    return 0;
}
void SV_ResetModInfo() {}
void TraceInit(const char *, const char *, int) {}
void Sys_Init() {
    Call("sys");
}
void FS_LogLevelLoadStarted(const char *) {}
void SeedRandomNumberGenerator() {}
void Sys_InitMemory() {
    Call("memory");
}
void Sys_InitLauncherInterface() {
    Call("launcher");
}
int GL_SetMode(CseGameWindowHandle, void **, void **, bool, const char *, char *) {
    Call("mode");
    return modeResult;
}
int Host_Init(HostParms *) {
    Call("host");
    host_initialized = hostFlag;
    return fixtureHostInitResult;
}
void Sys_InitAuthentication() {
    Call("authentication");
}
void Host_InitializeGameDLL() {
    Call("game-dll");
}
void NET_Config(bool) {
    Call("network");
}
void ClientDLL_ActivateMouse() {
    Call("mouse");
}

#if CSE_TEST_HARDWARE
#include "startup_hw.inc"
#else
#include "startup_sw.inc"
#endif

int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }
    const std::string name = argv[1];
    return RunCase([&] {
        SDL_Window window;
        SDL_Window *windowPointer = &window;
        char arguments[] = "";
        char directory[] = ".";
        if (name == "host-return") {
            fixtureHostInitResult = 0;
            hostFlag = true;
            Require(Sys_InitGame(arguments, directory, &windowPointer, 0) == 0,
                    "Sys_InitGame ignored Host_Init's explicit failure result");
            Require(calls["authentication"] == 0 && calls["mouse"] == 0,
                    "failed Host_Init continued into authentication/input startup");
        } else if (name == "host-flag") {
            fixtureHostInitResult = 1;
            hostFlag = false;
            Require(Sys_InitGame(arguments, directory, &windowPointer, 0) == 0, "unset host_initialized was ignored");
            Require(calls["authentication"] == 0, "partial Host startup enabled authentication");
        } else if (name == "success") {
            Require(Sys_InitGame(arguments, directory, &windowPointer, 0) == 1, "successful startup was rejected");
            Require(calls["authentication"] == 1 && calls["mouse"] == 1,
                    "successful startup lost retained authentication/input sequence");
        } else if (name == "mode-failure") {
#if CSE_TEST_HARDWARE
            modeResult = 0;
            Require(Sys_InitGame(arguments, directory, &windowPointer, 0) == 0, "failed GL mode was accepted");
            Require(calls["host"] == 0 && calls["authentication"] == 0,
                    "failed GL mode continued into absent Host owners");
#else
            throw std::runtime_error("mode failure is hardware-only");
#endif
        } else {
            throw std::runtime_error("unknown startup case");
        }
    });
}
