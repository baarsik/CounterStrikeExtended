// Complete actual event loops and owner/adapter; only SDL, game UI and engine destinations are controlled.
#define main RetainedWindowRegressionMain
#define NativeMouseDefaultPlatform RetainedNativeMouseDefaultPlatform
#include "../../native_mouse_window/window_tests.cpp"
#undef NativeMouseDefaultPlatform
#undef main
#include "sdl3_native_cocoa_keyboard.h"
#include "sdl3_input.h"
#include "keys.h"
#include "cvardef.h"
#include <cstring>
#include <vector>
#include <utility>
#if defined(CSE_SHORTCUT_LIFECYCLE_TEST)
#include "VGui1NativeRuntime.h"
#include <pthread.h>
#endif
namespace cse {
NativeMousePlatform NativeMouseDefaultPlatform();
}
keydest_t key_dest = key_game;
namespace {
bool shortcuts = false;
int shortcutWrites = 0;
bool shortcutSuccess = true;
std::uint32_t shortcutFailure = CseSdlCocoaMouseRejected;
void (*onShortcut)() = nullptr;
void (*afterDelivery)() = nullptr;
bool expectedAtDispatch = false;
int dispatchChecks = 0;
std::vector<std::pair<int, bool>> delivered;
std::vector<SDL_Event> queued;
std::size_t nextEvent = 0;
bool gameUi = false;
bool consoleUi = false;
int con_forcedup = 0;
int chat_bufferlen = 0;
char chat_buffer[KEY_CHAT_BUFFER_BYTES]{};
cvar_t cl_mousegrab{};
cvar_t m_rawinput{};
bool s_bCursorVisible = false;
unsigned mouseCode = 0;
cse::FractionalMotion s_wheelMotion;
constexpr int DLL_ACTIVE = 1;
constexpr int kEngineActive = 1;
int VGuiWrap2_IsGameUIVisible() {
    return gameUi;
}
int VGuiWrap2_IsConsoleVisible() {
    return consoleUi;
}
int BUsesSDLInput() {
    return 1;
}
int accumulations = 0;
bool host_initialized = true;
bool clientMouse = true;
int inputResets = 0;
int clientActivations = 0;
int clientDeactivations = 0;
void (*onInputReset)() = nullptr;
bool heldKeys[KEY_BINDING_COUNT]{};
void Key_Event(int key, int down) {
    heldKeys[key] = down != 0;
}
void Key_ClearStates() {}
void ClientDLL_ClearStates() {
    ++inputResets;
    if (onInputReset) {
        const auto callback = onInputReset;
        onInputReset = nullptr;
        callback();
    }
}
#include "clear_io.inc"
bool fClientLoaded = true;
struct ClientMouseFunctions {
    void (*pIN_ActivateMouse)() = []() {
        clientMouse = true;
        ++clientActivations;
    };
    void (*pIN_DeactivateMouse)() = []() {
        clientMouse = false;
        ++clientDeactivations;
    };
} cl_funcs;
#include "mouse_activation_state.inc"
#include "activate_mouse.inc"
#include "deactivate_mouse.inc"
#include "native_mouse_capture.inc"
void ClientDLL_IN_Accumulate() {
    ++accumulations;
}
int vguiDeliveries = 0;
void VGuiWrap2_CallEngineSurfaceAppHandler(SDL_Event *, void *) {
    ++vguiDeliveries;
}
#if defined(CSE_SHORTCUT_LIFECYCLE_TEST)
bool videoInitSuccess = true;
bool gameInitSuccess = true;
bool engineLoadSuccess = true;
bool fontRetirementSuccess = true;
int releases = 0;
int videoShutdowns = 0;
int engineUnloads = 0;
int gameFrames = 0;
int destroyedWindows = 0;
int sdkQuits = 0;
int delayCalls = 0;
void (*onCleanupStage)() = nullptr;
void CleanupStage() {
    if (onCleanupStage) {
        const auto callback = onCleanupStage;
        onCleanupStage = nullptr;
        callback();
    }
}
#endif
struct VideoMode {
    bool IsWindowedMode() {
        return true;
    }
    void UpdateWindowPosition() {}
#if defined(CSE_SHORTCUT_LIFECYCLE_TEST)
    bool Init(void *) {
        return videoInitSuccess;
    }
    void PlayStartupSequence() {}
    void ReleaseVideo() {
        ++releases;
        CleanupStage();
    }
    void Shutdown() {
        ++videoShutdowns;
        CleanupStage();
    }
#endif
} videoMode;
VideoMode *videomode = &videoMode;
struct Surface {
    bool IsCursorVisible() {
        return s_bCursorVisible;
    }
} g_BaseUISurface;
struct Engine {
    int GetState() {
        return 1;
    }
    int quitting = 0;
    void SetQuitting(int value) {
        quitting = value;
    }
    void TrapKey_Event(int key, bool down) {
        delivered.emplace_back(key, down);
        if (afterDelivery) {
            const auto callback = afterDelivery;
            afterDelivery = nullptr;
            callback();
        }
    }
    void TrapMouse_Event(unsigned, bool) {}
#if defined(CSE_SHORTCUT_LIFECYCLE_TEST)
    int GetQuitting() {
        return quitting;
    }
    bool Load(bool, char *, char *) {
        return engineLoadSuccess;
    }
    void Unload() {
        CHECK(!shortcuts);
        ++engineUnloads;
    }
    void Frame() {
        ++gameFrames;
    }
#endif
} engine;
Engine *eng = &engine;
class CGame {
    public:
    void SleepUntilInput(int time);
#if defined(CSE_SHORTCUT_LIFECYCLE_TEST)
    bool Init(void *) {
        return gameInitSuccess;
    }
    bool Shutdown();
    void *GetMainWindow() {
        return m_hSDLWindow;
    }
    SDL_Renderer *m_hSDLRenderer = nullptr;
    SDL_GLContext m_hSDLGLContext = nullptr;
    bool m_bCreatedRenderer = false;
#endif
    void AppActivate(bool active) {
        m_bActiveApp = active;
        expectedAtDispatch = active;
    }
    bool IsActiveApp() {
        return m_bActiveApp;
    }
    void SetWindowXY(int, int) {}
    void UpdateWindowMetrics() {}
    bool m_bActiveApp = true;
    bool m_bExpectSyntheticMouseMotion = false;
    bool m_bCursorVisible = false;
    int m_nMouseTargetX = 320;
    int m_nMouseTargetY = 240;
    int m_nWarpDelta = 213;
    SDL_Window *m_hSDLWindow = fixtureWindow;
};
CGame g_Game;
CGame *game = &g_Game;
#if !defined(CSE_TEST_SOFTWARE)
#include "key_mapping.inc"
#endif
#include CSE_LOOP_BODY
#if !defined(CSE_SHORTCUT_LIFECYCLE_TEST)
#include "ancillary_dispatch.inc"
#endif
void ExpectEnabledAtDispatch() {
    CHECK(shortcuts);
}
void ExpectDisabledAtDispatch() {
    CHECK(!shortcuts);
}
void RequestChat() {
    key_dest = key_message;
    expectedAtDispatch = false;
}
void ReplaceNativeProperty() {
    scene.native = reinterpret_cast<void *>(static_cast<std::uintptr_t>(0x4456789010ULL));
}
void NestedRetirement() {
    CHECK(!active->Invalidate(fixtureWindow));
}
void NestedUiRequest() {
    cse::NativeCursorTransaction nested;
    CHECK(!active->Begin(fixtureWindow, cse::NativeCursorOwner::VGui1, true, false, nested));
}
void Cursor(bool visible, bool gameplay = true, cse::NativeCursorOwner owner = cse::NativeCursorOwner::VGui2) {
    cse::NativeCursorTransaction transaction;
    CHECK(active->Begin(fixtureWindow, owner, visible, false, transaction, gameplay));
    CHECK(transaction.Commit());
}
void Run(bool enabled) {
    expectedAtDispatch = enabled;
    scene.onPump = enabled ? ExpectEnabledAtDispatch : ExpectDisabledAtDispatch;
    g_Game.SleepUntilInput(0);
}
void Key(SDL_Scancode code, bool down) {
    SDL_Event event{};
    event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.scancode = code;
    event.common.timestamp = scene.ticks + 1;
    queued.push_back(event);
}
} // namespace
#include "context.inc"
#include "capture_context.inc"
extern "C" std::uint32_t CSE_SDL_NATIVE_MOUSE_CALL CseSdl_CheckCocoaInteractionOwnerV1(std::uint32_t, void *) {
#if defined(CSE_CAPTURE_TEST)
    ++scene.interactionQueries;
    if (scene.onInteraction) {
        const auto callback = scene.onInteraction;
        scene.onInteraction = nullptr;
        callback();
    }
    return scene.focused ? scene.interactionResult : static_cast<std::uint32_t>(CseSdlCocoaMouseNotInteractionOwner);
#else
    return CseSdlCocoaMouseReady;
#endif
}
extern "C" std::uint32_t CSE_SDL_NATIVE_MOUSE_CALL CseSdl_CheckCocoaMouseOwnedV1(std::uint32_t version, void *window) {
    CHECK(version == CSE_SDL_COCOA_MOUSE_CONTRACT_V1);
    return Owned(window) ? CseSdlCocoaMouseReady : CseSdlCocoaMouseRejected;
}
extern "C" std::uint32_t CSE_SDL_NATIVE_MOUSE_CALL CseSdl_CheckCocoaRelativeFocusV1(std::uint32_t, void *window,
        int width, int height) {
    return Focused(window, width, height) ? CseSdlCocoaMouseReady : CseSdlCocoaMouseRejected;
}
extern "C" std::uint32_t CSE_SDL_NATIVE_MOUSE_CALL CseSdl_ReadCocoaMouseAbsoluteV1(std::uint32_t, void *window,
        int width, int height, float *x, float *y) {
    return Absolute(window, width, height, x, y) ? CseSdlCocoaMouseReady : CseSdlCocoaMouseRejected;
}
extern "C" std::uint32_t CSE_SDL_NATIVE_MOUSE_CALL CseSdl_SetCocoaGameplayCursorV1(std::uint32_t, void *window,
        std::uint32_t hidden) {
    return Services().gameplayCursor(window, hidden != 0) ? CseSdlCocoaMouseReady : CseSdlCocoaMouseRejected;
}
extern "C" std::uint32_t CSE_SDL_NATIVE_MOUSE_CALL CseSdl_SetCocoaGameplayShortcutsV1(std::uint32_t version,
        void *window, std::uint32_t enabled) {
    CHECK(version == CSE_SDL_COCOA_KEYBOARD_CONTRACT_V1 && window == nativeWindow && enabled <= 1);
    ++shortcutWrites;
    if (onShortcut) {
        const auto callback = onShortcut;
        onShortcut = nullptr;
        callback();
    }
    if (!shortcutSuccess) {
        return shortcutFailure;
    }
    shortcuts = enabled != 0;
    return CseSdlCocoaMouseReady;
}
extern "C" bool SDLCALL SDL_PollEvent(SDL_Event *event) {
    ++dispatchChecks;
#if !defined(CSE_CAPTURE_TEST)
    if (shortcuts != expectedAtDispatch) {
        std::fprintf(stderr, "dispatch mismatch next=%zu queued=%zu active=%d dest=%d capture=%d\n", nextEvent, queued.size(), g_Game.m_bActiveApp, key_dest, active->CaptureAllowed());
    }
    CHECK(shortcuts == expectedAtDispatch);
#endif
    if (nextEvent == queued.size()) {
        return false;
    }
    *event = queued[nextEvent++];
    return true;
}
extern "C" int SDLCALL SDL_PeepEvents(SDL_Event *event, int count, SDL_EventAction action, Uint32 first, Uint32 last) {
    CHECK(count == 1 && action == SDL_GETEVENT && first == SDL_EVENT_FIRST && last == SDL_EVENT_LAST);
    if (nextEvent == queued.size()) {
        return 0;
    }
    *event = queued[nextEvent++];
    return 1;
}
extern "C" bool SDLCALL SDL_WaitEventTimeout(SDL_Event *event, Sint32 time) {
    CHECK(time == 0);
    return SDL_PollEvent(event);
}
int EngineRegressionMain() {
    CHECK(con_forcedup == 0 && VGuiWrap2_IsGameUIVisible() == 0 && VGuiWrap2_IsConsoleVisible() == 0);
    CHECK(DLL_ACTIVE == kEngineActive);
    Reset();
    active = &cse::NativeMouseWindowState();
    Bind(*active);
    ClientDLL_DeactivateMouse();
    ClientDLL_ActivateMouse();
    cse::NativeCursorTransaction transaction;
    CHECK(active->Begin(fixtureWindow, cse::NativeCursorOwner::VGui2, false, true, transaction));
    CHECK(transaction.Commit());
    Key(SDL_SCANCODE_LCTRL, true);
    Key(SDL_SCANCODE_SPACE, true);
    Key(SDL_SCANCODE_SPACE, false);
    Key(SDL_SCANCODE_LCTRL, false);
    Run(true);
    CHECK(delivered.size() == 4);
    CHECK(delivered[0] == std::make_pair(static_cast<int>(K_CTRL), true));
    CHECK(delivered[1] == std::make_pair(static_cast<int>(K_SPACE), true));
    CHECK(delivered[2] == std::make_pair(static_cast<int>(K_SPACE), false));
    CHECK(delivered[3] == std::make_pair(static_cast<int>(K_CTRL), false));
    const int steadyWrites = shortcutWrites;
    for (int frame = 0; frame < 5; ++frame) {
        Run(true);
    }
    CHECK(shortcutWrites == steadyWrites);
    CHECK(active->SetGameplayShortcutsSuspended(false));
    CHECK(shortcuts && shortcutWrites == steadyWrites);
    // Actual context predicate excludes each keyboard/UI destination independently.
    key_dest = key_message;
    Run(false);
    key_dest = key_menu;
    Run(false);
    key_dest = key_game;
    con_forcedup = 1;
    Run(false);
    con_forcedup = 0;
    gameUi = true;
    Run(false);
    gameUi = false;
    consoleUi = true;
    Run(false);
    consoleUi = false;
    g_Game.m_bActiveApp = false;
    Run(false);
    g_Game.m_bActiveApp = true;
    scene.keyboard = otherWindow;
    Run(false);
    scene.keyboard = fixtureWindow;
    Run(true);
    // The next implicit native pump in PollEvent must see a destination changed by a delivered key.
    Key(SDL_SCANCODE_A, true);
    Key(SDL_SCANCODE_A, false);
    afterDelivery = RequestChat;
    Run(true);
    CHECK(key_dest == key_message && !shortcuts);
    key_dest = key_game;
    Run(true);
    for (const auto type : {SDL_EVENT_WINDOW_FOCUS_LOST, SDL_EVENT_WINDOW_FOCUS_GAINED,
                            SDL_EVENT_WINDOW_RESIZED, SDL_EVENT_WINDOW_ENTER_FULLSCREEN,
                            SDL_EVENT_WINDOW_LEAVE_FULLSCREEN}) {
        SDL_Event event{};
        event.type = type;
        queued.push_back(event);
        Run(g_Game.m_bActiveApp);
    }
    CHECK(shortcuts);
#if !defined(CSE_SHORTCUT_LIFECYCLE_TEST)
    const int beforeAudio = audioUpdates;
    S_ExtraUpdate();
    CHECK(audioUpdates == beforeAudio + 1);
    key_dest = key_message;
    expectedAtDispatch = false;
    S_ExtraUpdate();
    CHECK(!shortcuts && audioUpdates == beforeAudio + 2);
    WebmDispatch();
    key_dest = key_game;
    Run(true);
    shortcutSuccess = false;
    key_dest = key_message;
    const int beforeBlockedPump = scene.pumps;
    S_ExtraUpdate();
    WebmDispatch();
    CHECK(scene.pumps == beforeBlockedPump && audioUpdates == beforeAudio + 3);
    shortcutSuccess = true;
    Run(false);
    key_dest = key_game;
    Run(true);
#endif
    // Suspension never loses intent and never re-enables until a fresh context query.
    CHECK(active->SetGameplayShortcutsSuspended(true));
    CHECK(!shortcuts);
    Run(false);
    CHECK(active->SetGameplayShortcutsSuspended(false));
    CHECK(!shortcuts);
    Run(true);
    for (const auto owner : {cse::NativeCursorOwner::VGui1, cse::NativeCursorOwner::VGui2}) {
        Cursor(true, true, owner);
        Run(false);
        Cursor(false, true, owner);
        Run(true);
        Cursor(false, false, owner);
        Run(false); // Hidden custom UI cursor is not gameplay.
    }
    Cursor(false);
    Run(true);
    // Failed visible UI intent has immediate precedence over the old committed gameplay cursor.
    scene.setterSuccess = false;
    cse::NativeCursorTransaction failedUi;
    CHECK(!active->Begin(fixtureWindow, cse::NativeCursorOwner::VGui1, true, false, failedUi));
    CHECK(!shortcuts && active->NeedsRetry(cse::NativeCursorOwner::VGui1));
    Run(false);
    scene.setterSuccess = true;
    Cursor(true, true, cse::NativeCursorOwner::VGui1);
    Run(false);
    Cursor(false);
    Run(true);
    // A failed native disable gates dispatch and destruction, preserving the next retry.
    shortcutSuccess = false;
    key_dest = key_message;
    const int pumpsBeforeFailure = scene.pumps;
    const int dispatchBeforeFailure = dispatchChecks;
    g_Game.SleepUntilInput(0);
    CHECK(scene.pumps == pumpsBeforeFailure && dispatchChecks == dispatchBeforeFailure);
    CHECK(!active->Invalidate(fixtureWindow));
    CHECK(!active->Bind(fixtureWindow));
    shortcutSuccess = true;
    CHECK(active->Invalidate(fixtureWindow));
    CHECK(!shortcuts);
    Bind(*active);
    key_dest = key_game;
    Run(false); // New window has no committed gameplay intent.
    Cursor(false);
    for (const auto failure : {CseSdlCocoaMouseRejected, CseSdlCocoaMouseWrongThread,
                              CseSdlCocoaMouseNotReady}) {
        shortcutFailure = failure;
        shortcutSuccess = false;
        const int pumps = scene.pumps;
        g_Game.SleepUntilInput(0);
        CHECK(scene.pumps == pumps);
    }
    shortcutSuccess = true;
    Run(true);
    // In-flight retirement is retryable and cannot authorize stale outer admission.
    key_dest = key_message;
    onShortcut = NestedRetirement;
    CHECK(!cse::RefreshNativeGameplayShortcuts());
    CHECK(active->Invalidate(fixtureWindow));
    Bind(*active);
    key_dest = key_game;
    Cursor(false);
    onShortcut = NestedUiRequest;
    CHECK(!cse::RefreshNativeGameplayShortcuts());
    Run(false);
    CHECK(active->NeedsRetry(cse::NativeCursorOwner::VGui1));
    Cursor(true, true, cse::NativeCursorOwner::VGui1);
    Cursor(false);
    Run(true);
    scene.native = reinterpret_cast<void *>(static_cast<std::uintptr_t>(0x4456789010ULL));
    Run(false); // SDL property replacement cannot inherit enabled ownership.
    scene.native = nativeWindow;
    CHECK(active->Invalidate(fixtureWindow));
    Bind(*active);
    Run(false);
    Cursor(false);
    onShortcut = ReplaceNativeProperty;
    CHECK(!cse::RefreshNativeGameplayShortcuts());
    scene.native = nativeWindow;
    CHECK(active->Invalidate(nullptr));
    std::printf("actual-loop-shortcuts checks=%d writes=%d\n", checks, shortcutWrites);
    return 0;
}

#if !defined(CSE_SHORTCUT_LIFECYCLE_TEST) && !defined(CSE_CAPTURE_TEST)
int main() {
    return EngineRegressionMain();
}
#endif
