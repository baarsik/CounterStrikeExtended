#include "native_mouse_window.h"
#include "sdl3_native_mouse_bridge.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <initializer_list>
#include <type_traits>

namespace {
int checks = 0;
void Check(bool value, const char *text, int line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "native-window-check:%d:%s\n", line, text);
        std::exit(17);
    }
}
#define CHECK(value) Check((value), #value, __LINE__)

SDL_Window *const fixtureWindow = reinterpret_cast<SDL_Window *>(static_cast<std::uintptr_t>(0x1234567890ULL));
SDL_Window *const otherWindow = reinterpret_cast<SDL_Window *>(static_cast<std::uintptr_t>(0x2345678910ULL));
void *const nativeWindow = reinterpret_cast<void *>(static_cast<std::uintptr_t>(0x3456789010ULL));
struct Scene {
    bool mainThread = true;
    bool alive = true;
    bool owned = true;
    bool focused = true;
    bool setterSuccess = true;
    bool showSuccess = true;
    bool hideSuccess = true;
    bool publicRelative = false;
    bool applyMode = true;
    bool absoluteSuccess = true;
    bool shortcutSuccess = true;
    bool shortcutsEnabled = false;
    int shortcutWrites = 0;
    bool policySuccess = true;
    bool policyFailAfterEffect = false;
    bool policyHidden = false;
    int policyWrites = 0;
    SDL_WindowFlags windowFlags = 0;
    SDL_Window *keyboard = fixtureWindow;
    SDL_Window *mouse = fixtureWindow;
    void *native = nativeWindow;
    SDL_WindowID id = 19;
    SDL_PropertiesID properties = 23;
    std::uint32_t mode = CseSdlMouseModeAbsolute;
    float cachedX = 320.0f;
    float cachedY = 240.0f;
    float absoluteX = 360.0f;
    float absoluteY = 264.0f;
    int logicalWidth = 640;
    int logicalHeight = 480;
    int reads = 0;
    int absoluteReads = 0;
    int cachedReads = 0;
    int propertyCreates = 0;
    int forbidden = 0;
    int pumps = 0;
    int drains = 0;
    float relativeX = 0.0f;
    float relativeY = 0.0f;
    int grabs = 0;
    bool grabbed = false;
    bool grabSuccess = true;
    Uint64 ticks = 100000000;
    std::uint32_t interactionResult = 1;
    int interactionQueries = 0;
    void (*onInteraction)() = nullptr;
    void (*onFlush)(Uint32) = nullptr;
    int warps = 0;
    void (*onPump)() = nullptr;
    void (*onDrain)() = nullptr;
    void (*onMode)() = nullptr;
    void (*onGrab)() = nullptr;
    int calls = 0;
    char order[128]{};
    void (*reenter)() = nullptr;
};
Scene scene;
cse::NativeMouseWindow *active = nullptr;
cse::NativeCursorTransaction *nestedTicket = nullptr;
void Event(char event) {
    CHECK(scene.calls < static_cast<int>(sizeof(scene.order) - 1));
    scene.order[scene.calls++] = event;
}
void Reenter() {
    if (scene.reenter) {
        const auto callback = scene.reenter;
        scene.reenter = nullptr;
        callback();
    }
}
bool Owned(void *native) {
    CHECK(native == nativeWindow);
    ++scene.reads;
    Reenter();
    return scene.owned;
}
bool Focused(void *native, int width, int height) {
    CHECK(native == nativeWindow);
    ++scene.reads;
    Reenter();
    return scene.focused && width == scene.logicalWidth && height == scene.logicalHeight;
}
bool Absolute(void *native, int width, int height, float *x, float *y) {
    ++scene.absoluteReads;
    if (!Focused(native, width, height) || !scene.absoluteSuccess) {
        return false;
    }
    *x = scene.absoluteX;
    *y = scene.absoluteY;
    return true;
}
cse::NativeMousePlatform Services() {
    return {"fixture.native.window", Owned, Focused, Absolute, [](void *native, bool hidden) {
        CHECK(native == nativeWindow);
        ++scene.policyWrites;
        if (!scene.policySuccess) {
            return false;
        }
        scene.policyHidden = hidden;
        Reenter();
        return !scene.policyFailAfterEffect;
    }, [](void *native, bool enabled) {
        CHECK(native == nativeWindow);
        ++scene.shortcutWrites;
        if (!scene.shortcutSuccess) {
            return false;
        }
        scene.shortcutsEnabled = enabled;
        return true;
    }
#if defined(CSE_CAPTURE_CALLER_TEST)
    , [](void *) -> std::uint32_t {
        ++scene.interactionQueries;
        return scene.focused ? scene.interactionResult : 8;
    }
#endif
    };
}
void Reset() {
    scene = Scene{};
}
void NoSample(cse::NativeMouseWindow &state) {
    float x = 111.0f;
    float y = 222.0f;
    CHECK(!state.Sample(fixtureWindow, 640, 480, &x, &y));
    CHECK(x == 111.0f && y == 222.0f);
}
void Bind(cse::NativeMouseWindow &state) {
    CHECK(state.Bind(fixtureWindow));
    scene.calls = 0;
    scene.order[0] = 0;
}
void NestedDifferent() {
    CHECK(active && nestedTicket);
    CHECK(!active->Begin(fixtureWindow, cse::NativeCursorOwner::VGui2, true, true, *nestedTicket));
}
void Invalidate() {
    active->Invalidate(fixtureWindow);
}
void RejectReentrantBind() {
    CHECK(!active->Bind(fixtureWindow));
}
} // namespace

namespace cse {
NativeMousePlatform NativeMouseDefaultPlatform() {
    return Services();
}
} // namespace cse
extern "C" bool SDLCALL SDL_IsMainThread() {
    return scene.mainThread;
}
extern "C" SDL_WindowID SDLCALL SDL_GetWindowID(SDL_Window *value) {
    CHECK(value == fixtureWindow);
    return scene.id;
}
extern "C" SDL_Window *SDLCALL SDL_GetWindowFromID(SDL_WindowID id) {
    return scene.alive && id == scene.id ? fixtureWindow : nullptr;
}
extern "C" SDL_PropertiesID SDLCALL SDL_GetWindowProperties(SDL_Window *value) {
    CHECK(value == fixtureWindow);
    ++scene.propertyCreates;
    return scene.properties;
}
extern "C" void *SDLCALL SDL_GetPointerProperty(SDL_PropertiesID id, const char *, void *) {
    return id == scene.properties ? scene.native : nullptr;
}
extern "C" SDL_Window *SDLCALL SDL_GetKeyboardFocus() {
    return scene.keyboard;
}
extern "C" SDL_Window *SDLCALL SDL_GetMouseFocus() {
    return scene.mouse;
}
extern "C" bool SDLCALL SDL_GetWindowRelativeMouseMode(SDL_Window *) {
    return scene.publicRelative;
}
extern "C" bool SDLCALL SDL_SetWindowRelativeMouseMode(SDL_Window *value, bool enabled) {
    CHECK(value == fixtureWindow);
    Event(enabled ? 'R' : 'A');
    Reenter();
    if (!scene.setterSuccess) {
        return false;
    }
    scene.publicRelative = enabled;
    if (scene.applyMode) {
        scene.mode = enabled && scene.keyboard == fixtureWindow ? CseSdlMouseModeRelative : CseSdlMouseModeAbsolute;
    }
    const auto callback = scene.onMode;
    scene.onMode = nullptr;
    if (callback) {
        callback();
    }
    return true;
}
extern "C" bool SDLCALL SDL_ShowCursor() {
    Event('S');
    Reenter();
    return scene.showSuccess;
}
extern "C" bool SDLCALL SDL_HideCursor() {
    Event('H');
    Reenter();
    return scene.hideSuccess;
}
extern "C" SDL_MouseButtonFlags SDLCALL SDL_GetMouseState(float *x, float *y) {
    ++scene.cachedReads;
    *x = scene.cachedX;
    *y = scene.cachedY;
    return 0;
}
extern "C" std::uint32_t CSE_SDL_NATIVE_MOUSE_CALL CseSdl_QueryMouseModeV1(std::uint32_t version) {
    CHECK(version == CSE_SDL_MOUSE_MODE_CONTRACT_V1);
    return scene.mainThread ? scene.mode : static_cast<std::uint32_t>(CseSdlMouseModeWrongThread);
}
extern "C" void SDLCALL SDL_PumpEvents() {
    ++scene.forbidden;
    ++scene.pumps;
    const auto callback = scene.onPump;
    scene.onPump = nullptr;
    if (callback) {
        callback();
    }
}
extern "C" SDL_MouseButtonFlags SDLCALL SDL_GetRelativeMouseState(float *x, float *y) {
    if (x) {
        *x = scene.relativeX;
    }
    if (y) {
        *y = scene.relativeY;
    }
    scene.relativeX = 0.0f;
    scene.relativeY = 0.0f;
    ++scene.forbidden;
    ++scene.drains;
    const auto callback = scene.onDrain;
    scene.onDrain = nullptr;
    if (callback) {
        callback();
    }
    return 0;
}
extern "C" void SDLCALL SDL_WarpMouseInWindow(SDL_Window *, float, float) {
    ++scene.forbidden;
    ++scene.warps;
}
extern "C" bool SDLCALL SDL_SetWindowMouseGrab(SDL_Window *value, bool grabbed) {
    CHECK(value == fixtureWindow);
    ++scene.grabs;
    if (!scene.grabSuccess) {
        return false;
    }
    scene.grabbed = grabbed;
    const auto callback = scene.onGrab;
    scene.onGrab = nullptr;
    if (callback) {
        callback();
    }
    return true;
}
extern "C" bool SDLCALL SDL_GetWindowSize(SDL_Window *value, int *width, int *height) {
    CHECK(value == fixtureWindow);
    *width = scene.logicalWidth;
    *height = scene.logicalHeight;
    return true;
}

int main() {
    static_assert(std::is_trivially_destructible<cse::NativeMouseWindow>::value, "No static cleanup");
#if defined(__APPLE__)
    // Native camera capture is independent of the saved raw-input preference.
    for (bool rawInput : {false, true}) {
        for (auto owner : {cse::NativeCursorOwner::VGui1, cse::NativeCursorOwner::VGui2}) {
            Reset();
            cse::NativeMouseWindow capture(Services());
            Bind(capture);
            for (bool visible : {false, true, false}) {
                cse::NativeCursorTransaction ticket;
                CHECK(capture.Begin(fixtureWindow, owner, visible, rawInput, ticket));
                CHECK(scene.publicRelative == !visible);
                CHECK(scene.mode == (visible ? CseSdlMouseModeAbsolute : CseSdlMouseModeRelative));
                CHECK(ticket.Commit());
                CHECK(scene.policyHidden == !visible);
            }
            // SDL's focus boundary releases effective capture while retaining
            // this window's requested mode; returning focus reapplies that mode.
            scene.keyboard = otherWindow;
            scene.mouse = otherWindow;
            scene.mode = CseSdlMouseModeAbsolute;
            NoSample(capture);
            scene.keyboard = fixtureWindow;
            scene.mouse = fixtureWindow;
            scene.mode = CseSdlMouseModeRelative;
            float focusX = 0.0f;
            float focusY = 0.0f;
            CHECK(capture.Sample(fixtureWindow, 640, 480, &focusX, &focusY));
            const int calls = scene.calls;
            CHECK(capture.RefreshCursorPolicy());
            CHECK(scene.calls == calls && scene.publicRelative && scene.policyHidden);
            CHECK(capture.Invalidate(fixtureWindow));
        }
    }
#endif
    Reset();
    cse::NativeMouseWindow state(Services());
    active = &state;
    NoSample(state);
    Bind(state);
    const int creates = scene.propertyCreates;
    float x = 0.0f;
    float y = 0.0f;
    CHECK(state.Sample(fixtureWindow, 640, 480, &x, &y));
    CHECK(x == 360.0f && y == 264.0f && scene.cachedReads == 0 && scene.absoluteReads == 1);
    scene.absoluteX = -400.0f;
    scene.absoluteY = 980.5f;
    scene.mouse = otherWindow;
    CHECK(state.Sample(fixtureWindow, 640, 480, &x, &y));
    CHECK(x == -400.0f && y == 980.5f); // Outside the window is valid in absolute mode.
    scene.mode = CseSdlMouseModeRelative;
    NoSample(state);
    scene.mouse = fixtureWindow;
    CHECK(state.Sample(fixtureWindow, 640, 480, &x, &y));
    CHECK(x == 320.0f && y == 240.0f && scene.cachedReads == 1);
    CHECK(scene.propertyCreates == creates && scene.forbidden == 0);
    for (std::uint32_t mode : {0U, 3U, 4U, 5U, 6U, 0xffffffffU}) {
        scene.mode = mode;
        NoSample(state);
    }
    scene.mode = CseSdlMouseModeAbsolute;
    scene.keyboard = otherWindow;
    NoSample(state);
    scene.keyboard = fixtureWindow;
    scene.focused = false;
    NoSample(state);
    scene.focused = true;
    scene.absoluteSuccess = false;
    NoSample(state);
    scene.absoluteSuccess = true;
    scene.absoluteX = (std::numeric_limits<float>::infinity)();
    NoSample(state);
    scene.absoluteX = 1.0f;
    scene.absoluteY = (std::numeric_limits<float>::quiet_NaN)();
    NoSample(state);
    scene.absoluteY = 2.0f;
    scene.native = nullptr;
    NoSample(state);
    scene.native = nativeWindow;
    scene.alive = false;
    NoSample(state);
    scene.alive = true;
    scene.id = 20;
    NoSample(state);
    scene.id = 19;
    CHECK(!state.Sample(fixtureWindow, 639, 480, &x, &y));
    CHECK(!state.Sample(fixtureWindow, 0, 480, &x, &y));
    CHECK(!state.Sample(fixtureWindow, 640, 480, nullptr, &y));
    CHECK(!state.Sample(otherWindow, 640, 480, &x, &y));
    scene.reenter = Invalidate;
    NoSample(state);
    CHECK(!state.NeedsRetry(cse::NativeCursorOwner::VGui1));
    Bind(state);

    // Failure leaves the correct owner pending and does not claim a transaction.
    scene.setterSuccess = false;
    cse::NativeCursorTransaction failed;
    CHECK(!state.Begin(fixtureWindow, cse::NativeCursorOwner::VGui1, false, true, failed));
    CHECK(!failed.Current() && state.NeedsRetry(cse::NativeCursorOwner::VGui1));
    CHECK(!state.NeedsRetry(cse::NativeCursorOwner::VGui2));
    NoSample(state);
    scene.setterSuccess = true;
    {
        cse::NativeCursorTransaction retry;
        scene.calls = 0;
        CHECK(state.Begin(fixtureWindow, cse::NativeCursorOwner::VGui1, false, true, retry));
        CHECK(scene.calls == 2 && scene.order[0] == 'S' && scene.order[1] == 'R');
        CHECK(retry.Current());
        NoSample(state); // Suppress reentrant queries through the entire caller handoff.
        CHECK(retry.Commit());
        CHECK(scene.policyHidden);
    }
    CHECK(!state.NeedsRetry(cse::NativeCursorOwner::VGui1));
    for (bool visible : {false, true}) {
        cse::NativeCursorTransaction transaction;
        scene.calls = 0;
        CHECK(state.Begin(fixtureWindow, cse::NativeCursorOwner::VGui1, visible, false, transaction));
#if defined(__APPLE__)
        CHECK(scene.calls == 2 && scene.order[0] == (visible ? 'A' : 'S') &&
              scene.order[1] == (visible ? 'S' : 'R'));
#else
        CHECK(scene.calls == 2 && scene.order[0] == 'A' && scene.order[1] == (visible ? 'S' : 'H'));
#endif
        CHECK(transaction.Commit());
    }
    {
        cse::NativeCursorTransaction abandoned;
        CHECK(state.Begin(fixtureWindow, cse::NativeCursorOwner::VGui1, false, false, abandoned));
    }
    CHECK(state.NeedsRetry(cse::NativeCursorOwner::VGui1));
    CHECK(!scene.policyHidden); // No policy acquire for an abandoned transaction.
    // A nested newer request during setter, pump, drain or game handoff supersedes the outer ticket.
    for (int stage = 0; stage < 4; ++stage) {
        cse::NativeCursorTransaction inner;
        nestedTicket = &inner;
        cse::NativeCursorTransaction outer;
        if (stage == 0) {
            scene.reenter = NestedDifferent;
            CHECK(!state.Begin(fixtureWindow, cse::NativeCursorOwner::VGui1, false, false, outer));
        } else {
            CHECK(state.Begin(fixtureWindow, cse::NativeCursorOwner::VGui1, false, false, outer));
            NestedDifferent();
            CHECK(!outer.Current());
            CHECK(!outer.Commit());
        }
        CHECK(state.NeedsRetry(cse::NativeCursorOwner::VGui2));
        CHECK(!state.NeedsRetry(cse::NativeCursorOwner::VGui1));
        cse::NativeCursorTransaction latest;
        CHECK(state.Begin(fixtureWindow, cse::NativeCursorOwner::VGui2, true, true, latest));
        CHECK(latest.Commit());
    }
    // The same nested intent may coalesce; a ticket still cannot survive teardown/rebind.
    {
        cse::NativeCursorTransaction outer;
        CHECK(state.Begin(fixtureWindow, cse::NativeCursorOwner::VGui1, false, false, outer));
        cse::NativeCursorTransaction same;
        CHECK(!state.Begin(fixtureWindow, cse::NativeCursorOwner::VGui1, false, false, same));
        CHECK(outer.Current());
        state.Invalidate(fixtureWindow);
        CHECK(!outer.Current() && !outer.Commit());
    }
    Bind(state);
    {
        cse::NativeCursorTransaction transaction;
        CHECK(state.Begin(fixtureWindow, cse::NativeCursorOwner::VGui1, false, false, transaction));
        state.Invalidate(otherWindow);
        CHECK(transaction.Current() && transaction.Commit());
    }
    scene.setterSuccess = false;
    state.Invalidate(fixtureWindow);
    CHECK(!state.Bind(fixtureWindow));
    NoSample(state);
    scene.setterSuccess = true;
    scene.applyMode = false;
    scene.mode = CseSdlMouseModeRelative;
    CHECK(!state.Bind(fixtureWindow)); // Returned true/public false/global true is not admitted.
    scene.applyMode = true;
    scene.mode = CseSdlMouseModeAbsolute;
    Bind(state);
    scene.mainThread = false;
    NoSample(state);
    scene.mainThread = true;
    CHECK(scene.forbidden == scene.drains);
    {
        scene.policySuccess = false;
        cse::NativeCursorTransaction rejectedPolicy;
        CHECK(state.Begin(fixtureWindow, cse::NativeCursorOwner::VGui1, false, false, rejectedPolicy));
        CHECK(!rejectedPolicy.Commit());
        CHECK(state.NeedsRetry(cse::NativeCursorOwner::VGui1));
        CHECK(!scene.policyHidden);
        scene.policySuccess = true;
        cse::NativeCursorTransaction retriedPolicy;
        CHECK(state.Begin(fixtureWindow, cse::NativeCursorOwner::VGui2, false, false, retriedPolicy));
        CHECK(retriedPolicy.Commit() && scene.policyHidden);
        scene.policySuccess = false;
        CHECK(!state.Invalidate(fixtureWindow));
        CHECK(scene.policyHidden); // Keep ownership available for a later release attempt.
        scene.policySuccess = true;
        CHECK(state.Invalidate(fixtureWindow));
        CHECK(!scene.policyHidden);
    }
    Bind(state);
    {
        cse::NativeCursorTransaction reentrantPolicy;
        CHECK(state.Begin(fixtureWindow, cse::NativeCursorOwner::VGui1, false, false, reentrantPolicy));
        scene.reenter = Invalidate;
        CHECK(!reentrantPolicy.Commit());
        CHECK(!scene.policyHidden);
        CHECK(state.Invalidate(nullptr));
        CHECK(!scene.policyHidden);
    }
    for (bool refresh : {false, true}) {
        Bind(state);
        cse::NativeCursorTransaction hiddenPolicy;
        CHECK(state.Begin(fixtureWindow, cse::NativeCursorOwner::VGui1, false, false, hiddenPolicy));
        CHECK(hiddenPolicy.Commit());
        cse::NativeCursorTransaction nestedPolicy;
        nestedTicket = &nestedPolicy;
        scene.reenter = NestedDifferent;
        CHECK(!(refresh ? state.RefreshCursorPolicy() : state.Invalidate(fixtureWindow)));
        CHECK(state.NeedsRetry(cse::NativeCursorOwner::VGui2));
        CHECK(!nestedPolicy.Current());
        cse::NativeCursorTransaction newestPolicy;
        CHECK(state.Begin(fixtureWindow, cse::NativeCursorOwner::VGui2, true, true, newestPolicy));
        CHECK(newestPolicy.Commit() && !scene.policyHidden);
        scene.reenter = RejectReentrantBind;
        CHECK(state.Invalidate(fixtureWindow));
        CHECK(!scene.policyHidden);
    }
    Bind(state);
    {
        scene.policyFailAfterEffect = true;
        cse::NativeCursorTransaction partiallyApplied;
        CHECK(state.Begin(fixtureWindow, cse::NativeCursorOwner::VGui1, false, false, partiallyApplied));
        CHECK(!partiallyApplied.Commit() && scene.policyHidden);
        scene.policyFailAfterEffect = false;
        CHECK(state.Invalidate(fixtureWindow));
        CHECK(!scene.policyHidden);
    }
    std::printf("native-window checks=%d\n", checks);
    return 0;
}

extern "C" bool SDLCALL SDL_GetWindowMouseGrab(SDL_Window *) {
    return scene.grabbed;
}
extern "C" Uint64 SDLCALL SDL_GetTicksNS() {
    return scene.ticks;
}
extern "C" void SDLCALL SDL_FlushEvent(Uint32 type) {
    if (scene.onFlush) {
        scene.onFlush(type);
    }
}

extern "C" SDL_WindowFlags SDLCALL SDL_GetWindowFlags(SDL_Window *window) {
    CHECK(window == fixtureWindow);
    return scene.windowFlags;
}
