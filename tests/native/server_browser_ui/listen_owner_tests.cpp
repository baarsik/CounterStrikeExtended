#if defined(__APPLE__)
#include "engine/native_socket_lifetime.h"
#include <pthread.h>
#endif
#include "check.h"
#include "native_gameui_retirement.h"
#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using CreateInterfaceFn = void *(*)(const char *, int *);
namespace {
std::vector<std::string> events;
std::string testCase;
int pendingInner = 2;
int pendingWindow = 2;
bool retirementSafe = false;
bool filesystemLive = false;
bool steamLive = false;
void Event(const char *name) {
    events.emplace_back(name);
}
int Count(const char *name) {
    return static_cast<int>(std::count(events.begin(), events.end(), name));
}
}
char *g_pPostRestartCmdLineArgs = nullptr;
bool g_bIsDedicatedServer = false;
void Q_strncpy_s(char *output, const char *input, size_t size) {
    std::strncpy(output, input, size);
    output[size - 1] = 0;
}
void TraceInit(const char *, const char *, int) {}
void TraceShutdown(const char *, int) {}
void Sys_InitArgv(char *) {
    Event("argv");
}
void Steam_InitClient() {
    steamLive = true;
    Event("steam.init");
}
void Steam_ShutdownClient() {
    steamLive = false;
    Event("steam.shutdown");
}
int FileSystem_Init(char *, CreateInterfaceFn) {
    filesystemLive = true;
    Event("filesystem.init");
    return testCase != "filesystem-failure";
}
void FileSystem_Shutdown() {
    Require(filesystemLive, "filesystem cleanup repeated");
    filesystemLive = false;
    Event("filesystem.shutdown");
}
struct Registry {
    void Init() {
        Event("registry.init");
    }
    void WriteInt(const char *, int) {}
    void Shutdown() {
        Event("registry.shutdown");
    }
} registryOwner;
Registry *registry = &registryOwner;
struct Game {
    bool window = false;
    bool Init(void *) {
        Event("game.init");
        return testCase != "game-failure";
    }
    void *GetMainWindow() {
        return window ? this : nullptr;
    }
    void SleepUntilInput(int) {}
    bool Shutdown() {
        Require(filesystemLive && !steamLive, "window retirement ran outside retained dependency order");
        Event("game.shutdown.attempt");
        if (pendingWindow > 0) {
            --pendingWindow;
            return false;
        }
        window = false;
        Event("game.shutdown");
        return true;
    }
} gameOwner;
Game *game = &gameOwner;
struct Video {
    bool Init(void *) {
        Event("video.init");
        game->window = testCase != "video-failure";
        return game->window;
    }
    void PlayStartupSequence() {
        Event("startup");
    }
    void ReleaseVideo() {
        Event("video.release");
    }
    void Shutdown() {
        Event("video.shutdown");
    }
} videoOwner;
Video *videomode = &videoOwner;
void VideoMode_Create() {
    Event("video.create");
}
struct Engine {
    int quitting = 0;
    void SetQuitting(int value) {
        quitting = value;
    }
    int GetQuitting() {
        return quitting;
    }
    bool Load(bool dedicated, char *, char *) {
        Require(!dedicated, "listen fixture entered dedicated path");
        Event("engine.load");
        return testCase != "load-failure";
    }
    void Frame() {
        quitting = 1;
        Event("engine.frame");
    }
} engineOwner;
Engine *eng = &engineOwner;
void SDL_Delay(unsigned int milliseconds) {
    Require(milliseconds == 1 && filesystemLive, "retirement did not retain filesystem across bounded yield");
    Event("yield");
}
namespace cse {
NativeGameInitState nativeGameInit;
void ResetNativeHostShutdown() {
    Event("host.reset");
}
bool EnterNativeGameUIRetirement() {
    Require(!retirementSafe, "recursive retirement admission");
    retirementSafe = true;
    return true;
}
void LeaveNativeGameUIRetirement() {
    retirementSafe = false;
}
bool TryShutdownNativeGame() {
    Require(retirementSafe, "inner retirement outside safe point");
    Event("inner.attempt");
    if (pendingInner > 0) {
        Require(filesystemLive && steamLive, "inner refusal lost active dependencies");
        --pendingInner;
        return false;
    }
    if (!nativeGameInit.complete) {
        Steam_ShutdownClient();
        nativeGameInit.complete = true;
    }
    return true;
}
bool TryUnloadNativeEngine() {
    return TryShutdownNativeGame();
}
#if defined(__APPLE__)
struct MouseState {
    bool Invalidate(void *) {
        return true;
    }
};
MouseState &NativeMouseWindowState() {
    static MouseState state;
    return state;
}
bool BeginNativeListenShutdown(bool, bool, bool);
bool FinishNativeListenShutdown();
#endif
}
#if defined(__APPLE__)
#include "listen_state.inc"
#include "listen_retirement.inc"
#endif
#if CSE_TEST_HARDWARE
#include "listen_hw.inc"
#else
#include "listen_sw.inc"
#endif
#if defined(__APPLE__)
extern "C" __attribute__((visibility("default"))) int CseTestListenRun(int argc, char **argv) {
#else
int main(int argc, char **argv) {
#endif
    if (argc != 2) {
        return 2;
    }
    testCase = argv[1];
    return RunCase([&] {
        const int sessions = testCase == "restart" ? 2 : 1;
        for (int session = 0; session < sessions; ++session) {
            events.clear();
            pendingInner = 2;
            pendingWindow = 2;
            char directory[] = ".";
            char command[] = "";
            char restart[] = "";
            RunListenServer(nullptr, directory, command, restart, nullptr, nullptr);
            Require(!filesystemLive && !steamLive && Count("filesystem.shutdown") == 1 &&
                        Count("registry.shutdown") == 1 && Count("steam.shutdown") == 1,
                    "outer return did not balance exactly acquired filesystem/registry/Steam owners");
            if (testCase == "filesystem-failure") {
                Require(Count("video.create") == 0 && Count("engine.load") == 0 && Count("inner.attempt") == 0,
                        "filesystem failure cleaned up never-created engine/video owners");
            } else {
                Require(Count("yield") >= 2 && Count("inner.attempt") >= 3,
                        "Run returned through caller unload before inner refusal resolved");
                if (testCase == "video-failure") {
                    Require(Count("game.init") == 0 && Count("engine.load") == 0,
                            "video failure entered absent game/engine owners");
                } else if (testCase == "game-failure") {
                    Require(Count("engine.load") == 0, "game failure entered absent engine owner");
                } else {
                    Require(Count("engine.load") == 1, "attempted engine Load omitted");
                }
#if defined(__APPLE__)
                if (testCase != "video-failure") {
                    Require(Count("game.shutdown.attempt") == 3 && Count("game.shutdown") == 1 &&
                                Count("video.shutdown") == 1,
                            "outer typed refusal repeated completed phases or skipped window");
                }
#endif
            }
        }
    });
}
