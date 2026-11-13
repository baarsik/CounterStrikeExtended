// Complete production HW/SW event loops + owner/adapter; named SDL/engine destinations only.
#define CSE_CAPTURE_TEST 1
#include "../../gameplay_shortcuts/engine/engine_tests.cpp"
#include <algorithm>

namespace {
constexpr Uint64 pollInterval = UINT64_C(10000000);
void Tick() {
    scene.ticks += pollInterval;
    g_Game.SleepUntilInput(0);
}
void Flush(Uint32 type) {
    queued.erase(std::remove_if(queued.begin() + nextEvent, queued.end(),
        [=](const SDL_Event &event) { return event.type == type; }), queued.end());
}
void Setup(bool pointerInside = true) {
    Reset();
    active = &cse::NativeMouseWindowState();
    CHECK(active->Invalidate(nullptr));
    queued.clear();
    delivered.clear();
    nextEvent = 0;
    g_Game = CGame{};
    gameUi = false;
    consoleUi = false;
    key_dest = key_game;
    con_forcedup = 0;
    shortcuts = false;
    shortcutSuccess = true;
    onShortcut = nullptr;
    clientMouse = true;
    clientActivations = 0;
    clientDeactivations = 0;
    inputResets = 0;
    onInputReset = nullptr;
    vguiDeliveries = 0;
    mouseCode = 0;
    scene.onFlush = Flush;
    scene.focused = pointerInside;
    chat_bufferlen = 0;
    chat_buffer[0] = 0;
    Bind(*active);
    ClientDLL_DeactivateMouse();
    ClientDLL_ActivateMouse();
    clientActivations = 0;
    clientDeactivations = 0;
}
void Request(cse::NativeCursorOwner owner, bool visible, bool raw, bool grab, bool gameplay = true) {
    cse::NativeCursorTransaction transaction;
    if (active->Begin(fixtureWindow, owner, visible, raw, transaction, gameplay,
                      grab ? cse::NativeMouseGrab::Enabled : cse::NativeMouseGrab::Disabled)) {
        CHECK(transaction.Commit());
    }
}
void Queue(Uint32 type, Uint64 time) {
    SDL_Event event{};
    event.type = type;
    event.common.timestamp = time;
    if (type == SDL_EVENT_KEY_DOWN || type == SDL_EVENT_KEY_UP) {
        event.key.scancode = SDL_SCANCODE_A;
    }
    if (type == SDL_EVENT_MOUSE_MOTION) {
        event.motion.x = 1000.0f;
    }
    queued.push_back(event);
}
void QueueUiNavigation() {
    Key(SDL_SCANCODE_LEFT, true);
    Key(SDL_SCANCODE_RETURN, true);
    Key(SDL_SCANCODE_ESCAPE, true);
    SDL_Event event{};
    event.type = SDL_EVENT_TEXT_INPUT;
    event.common.timestamp = scene.ticks + 1;
    event.text.text = "outside";
    queued.push_back(event);
}
void CheckUiNavigation() {
    CHECK(delivered.size() == 3);
    CHECK(delivered[0] == std::make_pair(static_cast<int>(K_LEFTARROW), true));
    CHECK(delivered[1] == std::make_pair(static_cast<int>(K_ENTER), true));
    CHECK(delivered[2] == std::make_pair(static_cast<int>(K_ESCAPE), true));
    CHECK(std::strcmp(chat_buffer, "outside") == 0 && vguiDeliveries == 4);
    CHECK(!active->CaptureAllowed() && !scene.publicRelative);
}
void InitialUiOutside() {
    Setup(false); // No cursor/gameplay intent has ever existed on this binding.
    gameUi = true;
    key_dest = key_message;
    QueueUiNavigation();
    Tick();
    CheckUiNavigation();
    CHECK(inputResets == 0 && clientActivations == 0 && clientDeactivations == 0);
}
void ExistingUiOutside() {
    for (bool custom : {false, true}) {
        for (bool previousGameplay : {false, true}) {
            Setup();
            if (previousGameplay) {
                Request(cse::NativeCursorOwner::VGui2, false, true, false);
            }
            Request(cse::NativeCursorOwner::VGui2, !custom, false, false, !custom);
            gameUi = true;
            key_dest = key_message;
            scene.focused = false;
            QueueUiNavigation();
            Tick();
            CheckUiNavigation();
            CHECK(inputResets == 0 && clientActivations == 0 && clientDeactivations == 0);
        }
    }
}
void UiEligibilityControls() {
    Setup(false);
    SDL_Event key{};
    key.type = SDL_EVENT_KEY_DOWN;
    key.common.timestamp = scene.ticks + 1;
    CHECK(active->InputAllowed(key));
    scene.keyboard = otherWindow;
    CHECK(!active->InputAllowed(key));
    scene.keyboard = fixtureWindow;
    scene.windowFlags = SDL_WINDOW_MINIMIZED;
    CHECK(!active->InputAllowed(key));
    scene.windowFlags = 0;
    // A blocked hidden request is latest intent, but is not historical native capture.
    Request(cse::NativeCursorOwner::VGui2, false, true, true);
    CHECK(!active->InputAllowed(key));
    CHECK(!scene.publicRelative);
    for (bool custom : {false, true}) {
        Request(cse::NativeCursorOwner::VGui2, !custom, false, false, !custom);
        CHECK(active->InputAllowed(key) && !active->CaptureAllowed());
    }
    // Actual partial native capture must retain a fence after explicit UI intent changes.
    Setup();
    scene.applyMode = false; // SDL request succeeds, mode confirmation fails after the setter.
    Request(cse::NativeCursorOwner::VGui2, false, true, true);
    CHECK(scene.publicRelative);
    scene.focused = false;
    Tick();
    Request(cse::NativeCursorOwner::VGui2, true, false, false);
    key.common.timestamp = scene.ticks + 1;
    CHECK(!active->InputAllowed(key));
    scene.applyMode = true;
    scene.focused = true;
    Tick();
    CHECK(!scene.publicRelative);
    key.common.timestamp = scene.ticks + 1;
    CHECK(active->InputAllowed(key));
    key.common.timestamp = 0;
    CHECK(!active->InputAllowed(key));
    key.common.timestamp = scene.ticks;
    CHECK(!active->InputAllowed(key));
    // Query errors are never UI authority. Recovery to result 8 is eligible only after safe release.
    for (std::uint32_t failure : {CseSdlCocoaMouseRejected, CseSdlCocoaMouseForeignDispatch,
                                 CseSdlCocoaMouseWrongThread, CseSdlCocoaMouseNotReady}) {
        Setup();
        Request(cse::NativeCursorOwner::VGui2, true, false, false);
        scene.interactionResult = failure;
        Tick();
        key.common.timestamp = scene.ticks + 1;
        CHECK(!active->InputAllowed(key) && !active->CaptureAllowed());
        scene.interactionResult = CseSdlCocoaMouseReady;
        scene.focused = false;
        Tick();
        CHECK(active->InputAllowed(key) && !active->CaptureAllowed());
    }
    Setup();
    Request(cse::NativeCursorOwner::VGui2, true, false, false);
    scene.interactionResult = CseSdlCocoaMouseRejected;
    scene.grabSuccess = false;
    Tick();
    scene.interactionResult = CseSdlCocoaMouseReady;
    scene.focused = false;
    Tick();
    key.common.timestamp = scene.ticks + 1;
    CHECK(!active->InputAllowed(key)); // Error-only release is not yet Complete.
    scene.grabSuccess = true;
    Tick();
    CHECK(active->InputAllowed(key) && !active->CaptureAllowed());
    // Key destination alone cannot waive capture: chat still owns hidden gameplay capture.
    Setup();
    Request(cse::NativeCursorOwner::VGui2, false, true, false);
    key_dest = key_message;
    scene.focused = false;
    Tick();
    key.common.timestamp = scene.ticks + 1;
    CHECK(!active->InputAllowed(key));
    Request(cse::NativeCursorOwner::VGui2, true, false, false);
    CHECK(!active->InputAllowed(key));
    QueueUiNavigation();
    Tick();
    CHECK(delivered.empty() && chat_bufferlen == 0);
    Setup(false);
    key_dest = key_message;
    Queue(SDL_EVENT_WINDOW_FOCUS_LOST, 0);
    QueueUiNavigation();
    g_Game.SleepUntilInput(0);
    CHECK(delivered.empty() && chat_bufferlen == 0);
    Queue(SDL_EVENT_WINDOW_FOCUS_GAINED, 0);
    QueueUiNavigation();
    g_Game.SleepUntilInput(0);
    CHECK(delivered.size() == 3 && std::strcmp(chat_buffer, "outside") == 0);
    // UI pointer exit does not add release setters, drains or unbounded native queries.
    Setup(false);
    const int queries = scene.interactionQueries;
    const int setters = scene.calls;
    const int drains = scene.drains;
    for (int i = 0; i < 1000; ++i) {
        scene.ticks += 100000;
        g_Game.SleepUntilInput(0);
    }
    CHECK(scene.interactionQueries == queries + 10 && scene.calls == setters && scene.drains == drains);
    CHECK(inputResets == 0 && !active->CaptureAllowed());
}
void FreshMotion() {
    scene.relativeX = 17.75f;
    scene.relativeY = -4.75f;
}
void Retire() {
    CHECK(active->Invalidate(fixtureWindow));
}
void NestedMenu() {
    Request(cse::NativeCursorOwner::VGui1, true, false, false);
}
void VerifyResetBeforeDeactivate() {
    CHECK(clientMouse);
}
}

int main(int argc, char **argv) {
    if (argc == 2) {
        if (std::strcmp(argv[1], "initial-ui-outside") == 0) {
            InitialUiOutside();
            return 0;
        }
        if (std::strcmp(argv[1], "existing-ui-outside") == 0) {
            ExistingUiOutside();
            return 0;
        }
        return 2;
    }
    InitialUiOutside();
    ExistingUiOutside();
    UiEligibilityControls();
    for (auto owner : {cse::NativeCursorOwner::VGui1, cse::NativeCursorOwner::VGui2}) {
        for (bool raw : {false, true}) {
            for (bool grab : {false, true}) {
                Setup();
                Request(owner, false, raw, grab);
                CHECK(scene.publicRelative && scene.policyHidden);
                mouseCode = 7;
                heldKeys[K_CTRL] = true;
                heldKeys[K_MOUSE1] = true;
                onInputReset = VerifyResetBeforeDeactivate;
                scene.focused = false; // Active/key remain true; foreign frontmost recipient.
                Tick();
                CHECK(!scene.publicRelative && !scene.grabbed && !scene.policyHidden && !clientMouse);
                CHECK(mouseCode == 0 && inputResets == 1 && clientDeactivations == 1);
                CHECK(!heldKeys[K_CTRL] && !heldKeys[K_MOUSE1]);
                const int setters = scene.calls;
                const int drains = scene.drains;
                const int grabs = scene.grabs;
                const int policies = scene.policyWrites;
                for (int iteration = 0; iteration < 20; ++iteration) {
                    Tick();
                }
                CHECK(scene.calls == setters && scene.grabs == grabs && scene.policyWrites == policies);
                CHECK(scene.drains == drains && scene.warps == 0 && inputResets == 1);
                for (auto type : {SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_UP, SDL_EVENT_TEXT_EDITING,
                                  SDL_EVENT_TEXT_INPUT, SDL_EVENT_TEXT_EDITING_CANDIDATES,
                                  SDL_EVENT_MOUSE_MOTION, SDL_EVENT_MOUSE_BUTTON_DOWN,
                                  SDL_EVENT_MOUSE_BUTTON_UP, SDL_EVENT_MOUSE_WHEEL}) {
                    Queue(type, scene.ticks);
                }
                g_Game.SleepUntilInput(0);
                CHECK(delivered.empty() && vguiDeliveries == 0 && scene.warps == 0);
                Queue(SDL_EVENT_QUIT, 0);
                g_Game.SleepUntilInput(0);
                CHECK(engine.quitting == 1 && vguiDeliveries == 1);
                engine.quitting = 0;
                // Cancel/complete share the native ownership return. Old queued input is flushed.
                Queue(SDL_EVENT_KEY_DOWN, scene.ticks);
                Queue(SDL_EVENT_MOUSE_MOTION, 0);
                Queue(SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED, scene.ticks);
                scene.focused = true;
                scene.onMode = FreshMotion;
                Tick();
                CHECK(scene.publicRelative && scene.grabbed == grab && scene.policyHidden && clientMouse);
                CHECK(scene.relativeX == 17.75f && scene.relativeY == -4.75f);
                CHECK(clientActivations == 1 && delivered.empty());
                g_Game.SleepUntilInput(0); // Refresh cache invalidated by retained resize lifecycle.
                SDL_Event unknown{};
                unknown.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
                CHECK(!active->InputAllowed(unknown));
                unknown.common.timestamp = scene.ticks;
                CHECK(!active->InputAllowed(unknown));
                unknown.common.timestamp = scene.ticks + 1;
                CHECK(active->InputAllowed(unknown));
                unknown.type = SDL_EVENT_CLIPBOARD_UPDATE;
                unknown.common.timestamp = 0;
                CHECK(active->InputAllowed(unknown));
            }
        }
    }
    // Latest menu/custom cursor intent supersedes the pre-overlay gameplay request.
    for (bool custom : {false, true}) {
        Setup();
        Request(cse::NativeCursorOwner::VGui2, false, true, true);
        scene.focused = false;
        Tick();
        Request(cse::NativeCursorOwner::VGui1, !custom, false, false, !custom);
        scene.focused = true;
        Tick();
        CHECK(!scene.publicRelative && !scene.grabbed && !clientMouse);
        CHECK(active->CaptureAllowed());
    }
    // Callback UI changes invalidate the outer transaction and retain the newest intent.
    Setup();
    Request(cse::NativeCursorOwner::VGui2, false, false, true);
    scene.focused = false;
    onInputReset = NestedMenu;
    Tick();
    CHECK(!active->CaptureAllowed() && !clientMouse);
    scene.focused = true;
    Tick();
    CHECK(!scene.publicRelative && !clientMouse);
    // Retirement during the native query cannot authorize stale capture.
    Setup();
    Request(cse::NativeCursorOwner::VGui2, false, false, true);
    scene.onInteraction = Retire;
    Tick();
    CHECK(!active->CaptureAllowed());
    // Query failures fail closed, remain retryable, and do not starve quit.
    for (std::uint32_t failure : {CseSdlCocoaMouseRejected, CseSdlCocoaMouseWrongThread,
                                 CseSdlCocoaMouseContractMismatch, CseSdlCocoaMouseForeignDispatch,
                                 CseSdlCocoaMouseInvalidArgument, CseSdlCocoaMouseNotReady}) {
        Setup();
        Request(cse::NativeCursorOwner::VGui2, false, true, false);
        scene.interactionResult = failure;
        Queue(SDL_EVENT_QUIT, 0);
        Tick();
        CHECK(!clientMouse && !scene.publicRelative && engine.quitting == 1);
        scene.interactionResult = CseSdlCocoaMouseReady;
        Tick();
        CHECK(clientMouse && scene.publicRelative);
    }
    // Retry incomplete release stages only, including setter failures after a successful predecessor.
    for (int stage = 0; stage < 4; ++stage) {
        Setup();
        Request(cse::NativeCursorOwner::VGui2, false, true, true);
        scene.focused = false;
        scene.setterSuccess = stage != 0;
        scene.grabSuccess = stage != 1;
        scene.policySuccess = stage != 2;
        scene.showSuccess = stage != 3;
        Tick();
        CHECK(!clientMouse && !active->CaptureAllowed());
        const int grabs = scene.grabs;
        const int policies = scene.policyWrites;
        Tick();
        if (stage > 1) {
            CHECK(scene.grabs == grabs);
        }
        if (stage > 2) {
            CHECK(scene.policyWrites == policies);
        }
        CHECK(inputResets == 1 && clientDeactivations == 1);
        scene.setterSuccess = true;
        scene.grabSuccess = true;
        scene.policySuccess = true;
        scene.showSuccess = true;
        Tick();
        CHECK(!scene.publicRelative && !scene.grabbed && !scene.policyHidden);
        scene.focused = true;
        Tick();
        CHECK(scene.publicRelative && clientMouse);
    }
    // Failed restoration is never acknowledged and retries release before reacquiring.
    Setup();
    Request(cse::NativeCursorOwner::VGui2, false, false, true);
    scene.focused = false;
    Tick();
    scene.focused = true;
    scene.setterSuccess = false;
    Tick();
    CHECK(!clientMouse && !active->CaptureAllowed());
    scene.setterSuccess = true;
    Tick();
    CHECK(clientMouse && scene.publicRelative);
    // Reentrant retirement at each external reacquisition boundary prevents a stale commit.
    for (int boundary = 0; boundary < 4; ++boundary) {
        Setup();
        Request(cse::NativeCursorOwner::VGui2, false, true, true);
        scene.focused = false;
        Tick();
        scene.focused = true;
        if (boundary == 0) {
            onInputReset = Retire;
        }
        if (boundary == 1) {
            scene.onPump = Retire;
        }
        if (boundary == 2) {
            scene.onMode = Retire;
        }
        if (boundary == 3) {
            scene.onGrab = Retire;
        }
        Tick();
        CHECK(!clientMouse && !active->CaptureAllowed());
    }
    // Shortcut failure cannot pump native input, but already queued lifecycle/quit must progress.
    Setup();
    Request(cse::NativeCursorOwner::VGui2, false, true, true);
    CHECK(cse::RefreshNativeGameplayShortcuts());
    shortcutSuccess = false;
    key_dest = key_message;
    for (int retry = 0; retry < 3; ++retry) {
        Queue(SDL_EVENT_KEY_DOWN, scene.ticks + 1);
        Queue(SDL_EVENT_WINDOW_RESIZED, 0);
        Queue(SDL_EVENT_QUIT, 0);
        const int pumps = scene.pumps;
        g_Game.SleepUntilInput(0);
        CHECK(scene.pumps == pumps && delivered.empty() && engine.quitting == 1);
        CHECK(!active->Invalidate(fixtureWindow));
    }
    shortcutSuccess = true;
    CHECK(active->Invalidate(fixtureWindow));
    // Fractional wheel residue is reset, while fresh supported wheel pulses keep their normal delivery.
    Setup();
    Request(cse::NativeCursorOwner::VGui2, false, false, false);
    CHECK(cse::WheelSteps(s_wheelMotion, 0.75f, SDL_MOUSEWHEEL_NORMAL) == 0);
    scene.focused = false;
    Tick();
    scene.focused = true;
    Tick();
    SDL_Event wheel{};
    wheel.type = SDL_EVENT_MOUSE_WHEEL;
    wheel.common.timestamp = scene.ticks + 1;
    wheel.wheel.y = 0.5f;
    queued.push_back(wheel);
    g_Game.SleepUntilInput(0);
    CHECK(delivered.empty());
    wheel.wheel.y = 0.6f;
    queued.push_back(wheel);
    g_Game.SleepUntilInput(0);
    CHECK(delivered.size() == 2 && delivered[0] == std::make_pair(static_cast<int>(K_MWHEELUP), true));
    CHECK(delivered[1] == std::make_pair(static_cast<int>(K_MWHEELUP), false));
    Setup();
    Request(cse::NativeCursorOwner::VGui2, false, true, false);
    scene.mainThread = false;
    const int wrongThreadQueries = scene.interactionQueries;
    const int wrongThreadWrites = scene.calls;
    const cse::NativeMouseCaptureHooks hooks{[]() {}, [](bool) {
        CHECK(false);
    }};
    CHECK(!active->RefreshInteraction(true, true, hooks));
    CHECK(scene.interactionQueries == wrongThreadQueries && scene.calls == wrongThreadWrites);
    scene.mainThread = true;
    // An SDL native property replacement cannot authorize a setter for the retired handle.
    scene.native = reinterpret_cast<void *>(static_cast<std::uintptr_t>(0x4456789010ULL));
    Tick();
    CHECK(!active->CaptureAllowed() && scene.calls == wrongThreadWrites);
    scene.native = nativeWindow;
    // Native reads are bounded, and ordinary gameplay does not reapply policy or drain motion.
    Setup();
    Request(cse::NativeCursorOwner::VGui2, false, true, true);
    const int queries = scene.interactionQueries;
    const int writes = scene.calls;
    const int policies = scene.policyWrites;
    const int drains = scene.drains;
    for (int i = 0; i < 1000; ++i) {
        scene.ticks += 100000;
        g_Game.SleepUntilInput(0);
    }
    CHECK(scene.interactionQueries == queries + 10);
    CHECK(scene.calls == writes && scene.policyWrites == policies && scene.drains == drains);
    std::printf("actual capture loops passed checks=%d\n", checks);
    return 0;
}
