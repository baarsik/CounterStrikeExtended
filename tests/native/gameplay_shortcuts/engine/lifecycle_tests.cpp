#include "engine_tests.cpp"
#include <thread>
#include "../../../../src/launcher/native/VGui1HostImageOwner.h"
extern "C" int CseNativeRuntimeCloseV1() noexcept;
namespace cse_mac_runtime {
struct ControlledImageOwner {
    int leaves = 0;
    int closes = 0;
    cse_vgui1_host::Result LeaveRoot() {
        ++leaves;
        return cse_vgui1_host::Result::Ready;
    }
    cse_vgui1_host::Result Close() {
        ++closes;
        return cse_vgui1_host::Result::Closed;
    }
};
class Session {
    public:
    bool Close() noexcept;
    bool entered_ = true;
    bool active_ = true;
    int (*close_)() = CseNativeRuntimeCloseV1;
    ControlledImageOwner owner_;
};
#include "launcher_close.inc"
} // namespace cse_mac_runtime
namespace {
using CreateInterfaceFn = void *(*)(const char *, int *);
int filesystemShutdowns = 0;
int registryShutdowns = 0;
int argumentInits = 0;
int rootCloses = 0;
bool g_bIsDedicatedServer = false;
char *g_pPostRestartCmdLineArgs = nullptr;
void Q_strncpy_s(char *destination, const char *source, std::size_t size) {
    std::snprintf(destination, size, "%s", source);
}
void TraceInit(const char *, const char *, int) {}
void TraceShutdown(const char *, int) {}
void Sys_InitArgv(char *) {
    ++argumentInits;
}
void Steam_InitClient() {}
int FileSystem_Init(char *, CreateInterfaceFn) {
    return 1;
}
void VideoMode_Create() {}
struct Registry {
    void Init() {}
    void WriteInt(const char *, int) {}
    void Shutdown() {
        ++registryShutdowns;
        CleanupStage();
    }
} registryStorage;
Registry *registry = &registryStorage;
#include CSE_SHUTDOWN_BODY
#include CSE_LISTEN_BODY
} // namespace
void FileSystem_Shutdown() {
    ++filesystemShutdowns;
    CleanupStage();
}
#include "shutdown_phases.inc"
namespace cse_vgui1_native {
Result ConfigureEmbedded(const BindingV2 &) noexcept {
    return Result::Ready;
}
Result CloseEmbedded() noexcept {
    ++rootCloses;
    return Result::Closed;
}
Result RendererWillDestroy(bool) noexcept {
    CleanupStage();
    return fontRetirementSuccess ? Result::Ready : Result::Busy;
}
} // namespace cse_vgui1_native
extern "C" void SDLCALL SDL_DestroyRenderer(SDL_Renderer *) {}
extern "C" bool SDLCALL SDL_GL_DestroyContext(SDL_GLContext) {
    return true;
}
extern "C" void SDLCALL SDL_DestroyWindow(SDL_Window *) {
    CHECK(!shortcuts);
    ++destroyedWindows;
}
extern "C" void SDLCALL SDL_Quit() {
    CHECK(!shortcuts);
    ++sdkQuits;
}
extern "C" void SDLCALL SDL_Delay(Uint32 milliseconds) {
    CHECK(milliseconds == 1 && gameFrames == 0 && engineUnloads == 0);
    ++delayCalls;
    shortcutSuccess = true;
    expectedAtDispatch = false;
}
namespace {
void ReenterShutdown() {
    CHECK(!cse::FinishNativeListenShutdown());
    CHECK(!cse::BeginNativeListenShutdown(true, true, true));
    CHECK(CseNativeRuntimeCloseV1() == static_cast<int>(cse_vgui1_native::Result::Busy));
}
void ResetLifecycle() {
    CHECK(cse::FinishNativeListenShutdown());
    CHECK(active->Invalidate(nullptr));
    Reset();
    shortcutSuccess = true;
    shortcuts = false;
    shortcutWrites = 0;
    videoInitSuccess = true;
    gameInitSuccess = true;
    engineLoadSuccess = true;
    fontRetirementSuccess = true;
    releases = 0;
    videoShutdowns = 0;
    engineUnloads = 0;
    gameFrames = 0;
    destroyedWindows = 0;
    sdkQuits = 0;
    delayCalls = 0;
    filesystemShutdowns = 0;
    registryShutdowns = 0;
    queued.clear();
    nextEvent = 0;
    g_Game = CGame{};
    key_dest = key_game;
    Bind(*active);
    Cursor(false);
    CHECK(cse::RefreshNativeGameplayShortcuts());
    expectedAtDispatch = true;
    SDL_Event quit{};
    quit.type = SDL_EVENT_QUIT;
    queued.push_back(quit);
}
int Listen() {
    char directory[] = ".";
    char command[] = "hl";
    char restart[8]{};
    return RunListenServer(nullptr, directory, command, restart, nullptr, nullptr);
}
void FailRemovalAfterQuit() {
    // The queued Quit is delivered normally; native disable at the outer boundary must retry.
    shortcutSuccess = false;
}
} // namespace
int main() {
    active = &cse::NativeMouseWindowState();
    // Actual bootstrap is called before any fixture window/video setup. Worker
    // rejection comes from pthread_main_np, independently of SDL's default true.
    CHECK(CseNativeRuntimeCloseV1() == cse::native_runtime::Closed);
    const int closes = rootCloses;
    int workerResult = 0;
    std::thread worker([&]() {
        workerResult = CseNativeRuntimeCloseV1();
    });
    worker.join();
    CHECK(workerResult == static_cast<int>(cse_vgui1_native::Result::Busy));
    CHECK(rootCloses == closes);
    // Windowless/dedicated closure remains a no-op for the listen/window owners.
    CHECK(CseNativeRuntimeCloseV1() == cse::native_runtime::Closed);

    ResetLifecycle();
    CHECK(Listen() == 0);
    CHECK(engineUnloads == 1 && destroyedWindows == 1 && sdkQuits == 1);
    CHECK(videoShutdowns == 1 && filesystemShutdowns == 1 && registryShutdowns == 1);
    CHECK(CseNativeRuntimeCloseV1() == cse::native_runtime::Closed);
    CHECK(filesystemShutdowns == 1 && registryShutdowns == 1);

    // A later renderer retirement failure must stop dependent cleanup, and the
    // actual root close retries Game then completes each remaining stage once.
    ResetLifecycle();
    fontRetirementSuccess = false;
    CHECK(Listen() == 0);
    CHECK(engineUnloads == 1 && destroyedWindows == 0 && sdkQuits == 0);
    CHECK(videoShutdowns == 0 && filesystemShutdowns == 0 && registryShutdowns == 0);
    const int arguments = argumentInits;
    CHECK(Listen() == 2);
    CHECK(argumentInits == arguments); // Replacement cannot overwrite pending cleanup.
    CHECK(CseNativeRuntimeCloseV1() == static_cast<int>(cse_vgui1_native::Result::Busy));
    cse_mac_runtime::Session session;
    CHECK(!session.Close());
    CHECK(session.owner_.leaves == 0 && session.owner_.closes == 0 && session.entered_);
    fontRetirementSuccess = true;
    onCleanupStage = ReenterShutdown;
    CHECK(CseNativeRuntimeCloseV1() == cse::native_runtime::Closed);
    CHECK(destroyedWindows == 1 && sdkQuits == 1 && videoShutdowns == 1);
    CHECK(session.Close());
    CHECK(session.owner_.leaves == 1 && session.owner_.closes == 1 && !session.entered_);
    CHECK(filesystemShutdowns == 1 && registryShutdowns == 1);
    CHECK(CseNativeRuntimeCloseV1() == cse::native_runtime::Closed);
    CHECK(filesystemShutdowns == 1 && registryShutdowns == 1);

    for (int failure = 0; failure < 3; ++failure) {
        ResetLifecycle();
        videoInitSuccess = failure != 0;
        gameInitSuccess = failure != 1;
        engineLoadSuccess = failure != 2;
        fontRetirementSuccess = false;
        Listen();
        CHECK(engineUnloads == 0 && destroyedWindows == 0 && sdkQuits == 0);
        CHECK(filesystemShutdowns == 0 && registryShutdowns == 0);
        CHECK(CseNativeRuntimeCloseV1() == static_cast<int>(cse_vgui1_native::Result::Busy));
        fontRetirementSuccess = true;
        CHECK(CseNativeRuntimeCloseV1() == cse::native_runtime::Closed);
        CHECK(destroyedWindows == 1 && sdkQuits == 1);
        CHECK(filesystemShutdowns == 1 && registryShutdowns == 1);
    }
    // Native failure before eng->Unload returns to the existing outer loop,
    // admitting no gameplay frame and no unload until the next safe retry.
    ResetLifecycle();
    scene.onPump = FailRemovalAfterQuit;
    CHECK(Listen() == 0);
    CHECK(delayCalls == 1 && gameFrames == 0 && engineUnloads == 1);
    CHECK(CseNativeRuntimeCloseV1() == cse::native_runtime::Closed);
    // Recursive calls at every external cleanup stage cannot skip or duplicate work.
    for (int stage = 0; stage < 4; ++stage) {
        ResetLifecycle();
        onCleanupStage = ReenterShutdown;
        // Each prior stage is selected away to put the callback at the desired boundary.
        CHECK(cse::BeginNativeListenShutdown(stage == 0, stage <= 1, stage <= 2));
        CHECK(cse::FinishNativeListenShutdown());
    }
    CHECK(!g_bIsDedicatedServer);
    std::printf("actual-lifecycle-shortcuts checks=%d\n", checks);
    return 0;
}
