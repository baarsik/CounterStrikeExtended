#define main RetainedWindowRegressionMain
#include "window_tests.cpp"
#undef main
#include "sdl3_cursor.h"
#include "cvardef.h"
#include "../engine/vgui/Cursor.h"
#include "cse_vgui.h"
#include <cstring>

namespace {
struct ControlledGame {
    void *GetMainWindow() {
        return fixtureWindow;
    }
    void SetCursorVisible(bool visible) {
        lastVisible = visible;
        ++visibilityUpdates;
    }
    bool lastVisible = true;
    int visibilityUpdates = 0;
};
ControlledGame gameStorage;
ControlledGame *game = &gameStorage;
cvar_t m_rawinput{};
cvar_t cl_mousegrab{};
int usesSdlInput = 1;
int BUsesSDLInput() {
    return usesSdlInput;
}
struct CallerState {
    bool lastRaw = false;
    bool lastCapture = false;
    float deltaX = 17.0f;
    float deltaY = 19.0f;
};
void BaseCaller(CallerState &state, bool wasVisible, bool visible) {
    const bool s_bCursorVisible = visible;
    bool &s_bUsedRawInputLast = state.lastRaw;
#if defined(__APPLE__)
    const auto _currentCursor = visible ? vgui2::dc_arrow : vgui2::dc_none;
    bool &s_bUsedNativeCaptureLast = state.lastCapture;
    (void)_currentCursor;
    (void)s_bUsedNativeCaptureLast;
#endif
    float &m_deltax = state.deltaX;
    float &m_deltay = state.deltaY;
#include "base_caller.inc"
}
void WrapCaller(CallerState &state, bool wasVisible, bool visible) {
    const bool s_bCursorVisible = visible;
    bool &s_bUsedRawInputLast = state.lastRaw;
#include "wrap_caller.inc"
}
// Complete actual BaseUISurface cursor consumer, with only SDL/game/cvar seams.
bool s_bCursorVisible = true;
bool s_bUsedRawInputLast = false;
bool s_bUsedNativeCaptureLast = false;
SDL_Cursor *staticCurrentCursor = nullptr;
SDL_Cursor *staticDefaultCursor[vgui2::dc_last]{};
class BaseUISurface {
    public:
    void SetCursor(vgui2::HCursor cursor);
    void LockCursor() {
        _cursorLocked = true;
    }
    void CalculateNoPopup() {
#include "base_no_popup.inc"
    }
    vgui2::HCursor _currentCursor = vgui2::dc_arrow;
    bool _cursorLocked = false;
    float m_deltax = 0.0f;
    float m_deltay = 0.0f;
};
#include "base_full.inc"
using Caller = void (*)(CallerState &, bool, bool);
#if UINTPTR_MAX > UINT32_MAX
Caller outerCall;
Caller innerCall;
CallerState innerState;
void PumpAcceptedMotion() {
    scene.relativeX += 17.75f;
    scene.relativeY -= 4.75f;
}
void NestedCaller() {
    m_rawinput.value = 0.0f;
    innerCall(innerState, false, true);
}
#endif
void Setup() {
    Reset();
    active = &cse::NativeMouseWindowState();
    active->Invalidate(nullptr);
    Bind(*active);
    m_rawinput.value = 1.0f;
    cl_mousegrab.value = 1.0f;
    gameStorage = ControlledGame{};
    usesSdlInput = 1;
}
} // namespace

namespace cse {
bool RefreshNativeGameplayShortcuts() {
    return NativeMouseWindowState().RefreshGameplayShortcuts(true);
}
} // namespace cse

int cursorImages = 0;
extern "C" bool SDLCALL SDL_SetCursor(SDL_Cursor *) {
    ++cursorImages;
    return true;
}

int CallerRegressionMain(int argc, char **argv) {
    CHECK(RetainedWindowRegressionMain() == 0);
#if defined(__APPLE__)
    for (auto call : {BaseCaller, WrapCaller}) {
        for (int stage = 0; stage < 3; ++stage) {
            // VGUI1 has no explicit grab setter; its native mode owns capture.
            if (call == WrapCaller && stage == 1) {
                continue;
            }
            Setup();
            CallerState handoff;
            scene.relativeX = 60.0f; // Old-owner menu movement must be discarded.
            if (stage == 0) {
                scene.onMode = PumpAcceptedMotion;
            } else if (stage == 1) {
                scene.onGrab = PumpAcceptedMotion;
            } else {
                scene.onPump = PumpAcceptedMotion;
            }
            call(handoff, true, false);
            CHECK(scene.relativeX == 17.75f && scene.relativeY == -4.75f);
            CHECK(scene.drains == 1);
        }
    }
#endif
#if defined(__APPLE__)
    for (int raw = 0; raw < 2; ++raw) {
        Setup();
        m_rawinput.value = static_cast<float>(raw);
        s_bCursorVisible = true;
        s_bUsedRawInputLast = false;
        s_bUsedNativeCaptureLast = false;
        BaseUISurface surface;
        surface.SetCursor(vgui2::dc_none);
        CHECK(scene.publicRelative && scene.policyHidden);
        CHECK(cse::RefreshNativeGameplayShortcuts());
        CHECK(scene.shortcutsEnabled);
        const int pumps = scene.pumps;
        const int drains = scene.drains;
        surface.CalculateNoPopup();
        CHECK(scene.pumps == pumps && scene.drains == drains && scene.warps == 0);
        surface._cursorLocked = false;
        surface.SetCursor(vgui2::dc_user);
        CHECK(!scene.publicRelative && scene.policyHidden);
        CHECK(cse::RefreshNativeGameplayShortcuts());
        CHECK(!scene.shortcutsEnabled);
        surface.SetCursor(vgui2::dc_none);
        CHECK(scene.publicRelative && scene.policyHidden);
        for (const char *ui : {"menu", "console", "MOTD"}) {
            surface.SetCursor(vgui2::dc_arrow);
            CHECK(!scene.publicRelative && !scene.policyHidden);
            surface.SetCursor(vgui2::dc_none);
            CHECK(scene.publicRelative && scene.policyHidden);
            std::printf("actual BaseUISurface raw=%d %s release/reacquire\n", raw, ui);
        }
    }
    Setup();
    CallerState rebound;
    rebound.lastRaw = true;
    rebound.lastCapture = true;
    BaseCaller(rebound, false, false);
    CHECK(scene.publicRelative); // Unchanged intent must establish a fresh binding.
    CHECK(active->Invalidate(fixtureWindow));
    Bind(*active);
    BaseCaller(rebound, false, false);
    CHECK(scene.publicRelative);
    const int reboundCalls = scene.calls;
    CallerState inactiveRebound;
    inactiveRebound.lastRaw = true;
    inactiveRebound.lastCapture = true;
    WrapCaller(inactiveRebound, false, false);
    CHECK(scene.calls == reboundCalls);
#endif
    for (int owner = 0; owner < 2; ++owner) {
        if (argc == 2 &&
            ((owner == 0 && std::strcmp(argv[1], "base") != 0) || (owner == 1 && std::strcmp(argv[1], "wrap") != 0))) {
            continue;
        }
        Setup();
        const Caller call = owner == 0 ? BaseCaller : WrapCaller;
        const auto ownerTag = owner == 0 ? cse::NativeCursorOwner::VGui2 : cse::NativeCursorOwner::VGui1;
        CallerState state;
        scene.setterSuccess = false;
        call(state, true, false);
#if UINTPTR_MAX > UINT32_MAX
        CHECK(!state.lastRaw);
#if defined(__APPLE__)
        CHECK(scene.pumps == 0 && scene.drains == 1);
#else
        CHECK(scene.pumps == 0 && scene.drains == 0);
#endif
        CHECK(state.deltaX == 17.0f && state.deltaY == 19.0f);
        CHECK(active->NeedsRetry(ownerTag));
        scene.setterSuccess = true;
        call(state, false, false);
#if defined(__APPLE__)
        constexpr int expectedRetryDrains = 2;
#else
        constexpr int expectedRetryDrains = 1;
#endif
        CHECK(state.lastRaw && scene.pumps == 1 && scene.drains == expectedRetryDrains);
        CHECK(!active->NeedsRetry(ownerTag));
        CHECK(cse::RefreshNativeGameplayShortcuts());
        CHECK(scene.shortcutsEnabled);
        const int before = scene.calls;
        call(state, false, false);
        CHECK(scene.calls == before && scene.pumps == 1);
        // A nonpending unchanged second UI owner must not reclaim another owner's capture.
        CallerState inactive;
        inactive.lastRaw = true;
        inactive.lastCapture = true;
        const Caller other = owner == 0 ? WrapCaller : BaseCaller;
        other(inactive, false, false);
        CHECK(scene.calls == before);
        for (int stage = 0; stage < 2; ++stage) {
            Setup();
            CallerState outer;
            innerState = CallerState{};
            outerCall = call;
            innerCall = owner == 0 ? WrapCaller : BaseCaller;
            if (stage == 0) {
                scene.onPump = NestedCaller;
            } else {
                scene.onDrain = NestedCaller;
            }
            outerCall(outer, true, false);
            CHECK(!outer.lastRaw && !innerState.lastRaw);
            const auto innerTag = owner == 0 ? cse::NativeCursorOwner::VGui1 : cse::NativeCursorOwner::VGui2;
            CHECK(active->NeedsRetry(innerTag));
            CHECK(!active->NeedsRetry(ownerTag));
#if defined(__APPLE__)
            CHECK(scene.pumps == (stage == 0 ? 1 : 0) && scene.drains == 1);
#else
            CHECK(scene.pumps == 1 && scene.drains == stage);
#endif
            CHECK(gameStorage.visibilityUpdates == 0);
            innerCall(innerState, true, true);
            CHECK(!active->NeedsRetry(innerTag));
            CHECK(scene.mode == CseSdlMouseModeAbsolute);
        }
#else
        // Exact retained Win32 behavior is a reference, not a selected native repair.
        CHECK(state.lastRaw && scene.pumps == 1 && scene.drains == 1);
        CHECK(!active->NeedsRetry(ownerTag));
#endif
    }
    std::printf("actual-caller checks=%d\n", checks);
    return 0;
}

#if !defined(CSE_CAPTURE_CALLER_TEST)
int main(int argc, char **argv) { return CallerRegressionMain(argc, argv); }
#endif
