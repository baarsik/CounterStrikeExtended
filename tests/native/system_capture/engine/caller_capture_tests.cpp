#define CSE_CAPTURE_CALLER_TEST 1
#include "../../native_mouse_window/caller_tests.cpp"
#include "VGUI_Cursor.h"
#include "sdl3_native_cocoa_mouse.h"

// The VGUI1 cursor value is a named fixture seam; the complete production owner body is extracted unchanged.
vgui::Cursor::Cursor(DefaultCursor dc) : _hotspot{}, _bitmap(nullptr), _dc(dc) {}
vgui::Cursor::Cursor(vgui::Bitmap *, int, int) : Cursor(dc_user) {}
void vgui::Cursor::getHotspot(int &x, int &y) {
    x = _hotspot[0];
    y = _hotspot[1];
}
void vgui::Cursor::privateInit(vgui::Bitmap *, int, int) {}
vgui::Bitmap *vgui::Cursor::getBitmap() {
    return _bitmap;
}
vgui::Cursor::DefaultCursor vgui::Cursor::getDefaultCursor() {
    return _dc;
}

namespace {
struct MouseControl {
    bool VGUI2MouseControl() {
        return false;
    }
} mouseControl;
MouseControl *mousecontrol = &mouseControl;
int VGuiWrap2_IsGameUIVisible() {
    return 0;
}
struct CursorImage { void setImage(void *) {} } cursorImage;
class EngineSurfaceWrap {
    public:
    void setCursor(vgui::Cursor *cursor);
    bool _cursorLocked = false;
    vgui::Cursor *_currentCursor = nullptr;
    CursorImage *_emulatedCursor = &cursorImage;
    void *_mouseBitmap = nullptr;
};
#include "wrap_full.inc"
bool clientActive = true;
int clientActivations = 0;
bool fClientLoaded = true;
struct ClientMouseFunctions {
    void (*pIN_ActivateMouse)() = []() {
        clientActive = true;
        ++clientActivations;
    };
    void (*pIN_DeactivateMouse)() = []() {
        clientActive = false;
    };
} cl_funcs;
#include "mouse_activation_state.inc"
#include "activate_mouse.inc"
#include "deactivate_mouse.inc"
#include "native_mouse_capture.inc"
void Refresh(bool gameplayContext = true) {
    scene.ticks += UINT64_C(10000000);
    const cse::NativeMouseCaptureHooks hooks{[]() {}, [](bool enabled) {
        ClientDLL_SetNativeMouseCapture(enabled);
    }};
    active->RefreshInteraction(true, gameplayContext, hooks);
}
struct ActivationCursorOwner {
    explicit ActivationCursorOwner(bool useLegacy) : legacy(useLegacy) {
        s_bCursorVisible = true;
        s_bUsedRawInputLast = false;
        s_bUsedNativeCaptureLast = false;
    }
    void SetVisible(bool visible) {
        if (legacy) {
            wrap.setCursor(visible ? &arrow : &hidden);
        } else {
            surface.SetCursor(visible ? vgui2::dc_arrow : vgui2::dc_none);
        }
    }
    bool legacy;
    BaseUISurface surface;
    EngineSurfaceWrap wrap;
    vgui::Cursor arrow{vgui::Cursor::dc_arrow};
    vgui::Cursor hidden{vgui::Cursor::dc_none};
};
void Fresh() {
    scene.relativeX = 13.25f;
    scene.relativeY = -6.75f;
}
}
int main() {
    // The original engine activation request survives native fencing, even when
    // interaction returns while a menu is visible and its cursor later hides.
    for (bool legacy : {false, true}) {
        for (bool raw : {false, true}) {
            for (bool requestAgain : {false, true}) {
                Setup();
                ClientDLL_DeactivateMouse();
                ActivationCursorOwner owner(legacy);
                m_rawinput.value = raw ? 1.0f : 0.0f;
                owner.SetVisible(true);
                ClientDLL_ActivateMouse();
                CHECK(clientActive);
                scene.interactionResult = CseSdlCocoaMouseRejected;
                Refresh(false);
                CHECK(!clientActive && !scene.publicRelative);
                // A fresh activation request during foreign interaction must also
                // remain pending, without activating while capture is fenced.
                if (requestAgain) {
                    ClientDLL_ActivateMouse();
                }
                CHECK(!clientActive);
                scene.interactionResult = CseSdlCocoaMouseReady;
                Refresh(false);
                owner.SetVisible(false);
                Refresh();
                CHECK(scene.publicRelative && clientActive);
                const int activations = clientActivations;
                Refresh();
                Refresh();
                CHECK(clientActivations == activations);
            }
        }
    }
    // A blocked request before the first current ownership query is also retained.
    // Neither no request nor an explicit cancellation may start the client later.
    for (bool legacy : {false, true}) {
        for (bool requested : {false, true}) {
            for (bool cancel : {false, true}) {
                Setup();
                ClientDLL_DeactivateMouse();
                ActivationCursorOwner owner(legacy);
                m_rawinput.value = 0.0f;
                owner.SetVisible(true);
                active->InvalidateInteraction();
                if (requested) {
                    ClientDLL_ActivateMouse();
                }
                CHECK(!clientActive);
                if (cancel) {
                    ClientDLL_DeactivateMouse();
                }
                Refresh(false);
                CHECK(!clientActive);
                owner.SetVisible(false);
                Refresh();
                CHECK(scene.publicRelative);
                CHECK(clientActive == (requested && !cancel));
            }
        }
    }
    // Explicit deactivation during a native suspension overrides the older active
    // request, even after the original UI owner reacquires gameplay capture.
    for (bool legacy : {false, true}) {
        Setup();
        ClientDLL_DeactivateMouse();
        ActivationCursorOwner owner(legacy);
        owner.SetVisible(false);
        ClientDLL_ActivateMouse();
        CHECK(clientActive);
        scene.focused = false;
        Refresh();
        CHECK(!clientActive);
        ClientDLL_DeactivateMouse();
        scene.focused = true;
        Refresh();
        CHECK(scene.publicRelative && !clientActive);
    }
    for (auto caller : {BaseCaller, WrapCaller}) {
        for (bool raw : {false, true}) {
            for (bool grab : {false, true}) {
                Setup();
                CallerState local;
                m_rawinput.value = raw ? 1.0f : 0.0f;
                cl_mousegrab.value = grab ? 1.0f : 0.0f;
                scene.grabbed = grab; // VGUI1 inherits the current logical grab without a setter.
                const int grabs = scene.grabs;
                caller(local, true, false);
                CHECK(scene.publicRelative);
                if (caller == WrapCaller) {
                    CHECK(scene.grabs == grabs);
                }
                scene.focused = false;
                Refresh();
                CHECK(!scene.publicRelative && !scene.grabbed);
                // Raw choice changes while selection owns input; local lastRaw stays old.
                m_rawinput.value = raw ? 0.0f : 1.0f;
                caller(local, false, false);
                CHECK(!scene.publicRelative);
                scene.focused = true;
                scene.onMode = Fresh;
                Refresh();
                CHECK(scene.publicRelative && scene.relativeX == 13.25f && scene.relativeY == -6.75f);
                const int setters = scene.calls;
                const int drains = scene.drains;
                const int pumps = scene.pumps;
                const int policies = scene.policyWrites;
                caller(local, false, false);
                CHECK(local.lastRaw != raw);
                CHECK(scene.calls == setters && scene.drains == drains && scene.pumps == pumps);
                CHECK(scene.policyWrites == policies && scene.relativeX == 13.25f);
            }
        }
    }
    // Ordinary visible/custom UI retains keyboard eligibility outside the content rectangle.
    for (bool custom : {false, true}) {
        Setup();
        BaseUISurface surface;
        s_bCursorVisible = true;
        surface.SetCursor(custom ? vgui2::dc_user : vgui2::dc_arrow);
        scene.focused = false;
        Refresh();
        SDL_Event key{};
        key.type = SDL_EVENT_KEY_DOWN;
        key.common.timestamp = scene.ticks + 1;
        CHECK(active->InputAllowed(key));
        CHECK(!active->CaptureAllowed());
        // New explicit UI intent is recorded while capture itself remains unauthorized.
        surface.SetCursor(custom ? vgui2::dc_arrow : vgui2::dc_user);
        CHECK(active->InputAllowed(key) && !scene.publicRelative);
        Setup();
        EngineSurfaceWrap wrap;
        vgui::Cursor cursor(custom ? vgui::Cursor::dc_user : vgui::Cursor::dc_arrow);
        s_bCursorVisible = true;
        wrap.setCursor(&cursor);
        scene.focused = false;
        Refresh();
        key.common.timestamp = scene.ticks + 1;
        CHECK(active->InputAllowed(key) && !active->CaptureAllowed());
    }
    // A true gameplay suspension cannot be waived by the same visible/custom UI requests.
    for (bool custom : {false, true}) {
        Setup();
        BaseUISurface surface;
        s_bCursorVisible = true;
        surface.SetCursor(vgui2::dc_none);
        scene.focused = false;
        Refresh();
        surface.SetCursor(custom ? vgui2::dc_user : vgui2::dc_arrow);
        SDL_Event key{};
        key.type = SDL_EVENT_KEY_DOWN;
        key.common.timestamp = scene.ticks + 1;
        CHECK(!active->InputAllowed(key) && !active->CaptureAllowed());
        scene.focused = true;
        Refresh();
        key.common.timestamp = scene.ticks + 1;
        CHECK(active->InputAllowed(key) && !scene.publicRelative);
    }
    // A grab-only change has no visibility/raw cache difference, but is still current UI intent.
    for (bool grab : {false, true}) {
        Setup();
        CallerState local;
        cl_mousegrab.value = grab ? 1.0f : 0.0f;
        BaseCaller(local, true, false);
        scene.focused = false;
        Refresh();
        cl_mousegrab.value = grab ? 0.0f : 1.0f;
        const int setters = scene.calls;
        const int drains = scene.drains;
        const int pumps = scene.pumps;
        const int queries = scene.interactionQueries;
        CHECK(!active->NeedsRetry(cse::NativeCursorOwner::VGui1));
        for (int repeat = 0; repeat < 20; ++repeat) {
            BaseCaller(local, false, false);
        }
        CHECK(scene.calls == setters && scene.drains == drains && scene.pumps == pumps);
        CHECK(scene.interactionQueries == queries);
        scene.focused = true;
        scene.onMode = Fresh;
        Refresh();
        CHECK(scene.grabbed != grab && scene.relativeX == 13.25f);
        BaseCaller(local, false, false);
        CHECK(scene.relativeX == 13.25f);
        Setup();
        CallerState legacy;
        scene.grabbed = grab;
        WrapCaller(legacy, true, false);
        scene.focused = false;
        Refresh();
        cl_mousegrab.value = grab ? 0.0f : 1.0f;
        CHECK(!active->NeedsRetry(cse::NativeCursorOwner::VGui2));
        WrapCaller(legacy, false, false);
        scene.focused = true;
        Refresh();
        CHECK(scene.grabbed == grab); // Preserve never adopts the forced-false effective grab or VGUI2's cvar.
    }
    // A no-change restoration acknowledgement still loses to a nested newer intent.
    Setup();
    CallerState acknowledged;
    BaseCaller(acknowledged, true, false);
    scene.focused = false;
    Refresh();
    scene.focused = true;
    Refresh();
    cse::NativeCursorTransaction acknowledgement;
    CHECK(active->Begin(fixtureWindow, cse::NativeCursorOwner::VGui2, false, true, acknowledgement, true,
                        cse::NativeMouseGrab::Enabled));
    CHECK(!acknowledgement.Changed());
    cse::NativeCursorTransaction newer;
    CHECK(!active->Begin(fixtureWindow, cse::NativeCursorOwner::VGui1, true, false, newer));
    CHECK(!acknowledgement.Commit());
    CHECK(active->NeedsRetry(cse::NativeCursorOwner::VGui1));
    Setup();
    EngineSurfaceWrap wrap;
    vgui::Cursor hidden(vgui::Cursor::dc_none);
    vgui::Cursor arrow(vgui::Cursor::dc_arrow);
    s_bCursorVisible = true;
    wrap.setCursor(&hidden);
    scene.focused = false;
    Refresh();
    const int wrapImages = cursorImages;
    wrap.setCursor(&arrow);
    CHECK(cursorImages == wrapImages && !scene.publicRelative);
    scene.focused = true;
    Refresh();
    CHECK(!scene.publicRelative);
    wrap.setCursor(&arrow);
    // The complete BaseUISurface image-selection body cannot replace the system cursor.
    Setup();
    BaseUISurface surface;
    s_bCursorVisible = true;
    surface.SetCursor(vgui2::dc_none);
    scene.focused = false;
    Refresh();
    const int images = cursorImages;
    surface.SetCursor(vgui2::dc_arrow);
    surface.SetCursor(vgui2::dc_user);
    CHECK(cursorImages == images && !scene.publicRelative);
    scene.focused = true;
    Refresh();
    CHECK(!scene.publicRelative && scene.policyHidden && !clientActive);
    const int drains = scene.drains;
    surface.SetCursor(vgui2::dc_user);
    CHECK(scene.drains == drains);
    std::printf("actual UI capture acknowledgement checks=%d\n", checks);
    return 0;
}
