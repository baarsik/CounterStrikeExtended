#define main RetainedWindowRegressionMain
#include "window_tests.cpp"
#undef main
#include "sdl3_cursor.h"
#include "cvardef.h"
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
    float deltaX = 17.0f;
    float deltaY = 19.0f;
};
void BaseCaller(CallerState &state, bool wasVisible, bool visible) {
    const bool s_bCursorVisible = visible;
    bool &s_bUsedRawInputLast = state.lastRaw;
    float &m_deltax = state.deltaX;
    float &m_deltay = state.deltaY;
#include "base_caller.inc"
}
void WrapCaller(CallerState &state, bool wasVisible, bool visible) {
    const bool s_bCursorVisible = visible;
    bool &s_bUsedRawInputLast = state.lastRaw;
#include "wrap_caller.inc"
}
using Caller = void (*)(CallerState &, bool, bool);
#if UINTPTR_MAX > UINT32_MAX
Caller outerCall;
Caller innerCall;
CallerState innerState;
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

int main(int argc, char **argv) {
    CHECK(RetainedWindowRegressionMain() == 0);
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
        CHECK(scene.pumps == 0 && scene.drains == 0);
        CHECK(state.deltaX == 17.0f && state.deltaY == 19.0f);
        CHECK(active->NeedsRetry(ownerTag));
        scene.setterSuccess = true;
        call(state, false, false);
        CHECK(state.lastRaw && scene.pumps == 1 && scene.drains == 1);
        CHECK(!active->NeedsRetry(ownerTag));
        const int before = scene.calls;
        call(state, false, false);
        CHECK(scene.calls == before && scene.pumps == 1 && scene.drains == 1);
        // A nonpending unchanged second UI owner must not reclaim another owner's capture.
        CallerState inactive;
        inactive.lastRaw = true;
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
            CHECK(scene.pumps == 1 && scene.drains == stage);
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
