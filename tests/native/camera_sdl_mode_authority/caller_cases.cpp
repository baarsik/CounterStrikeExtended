#include <cstdio>
#include <cstdlib>
#include "fixture_sdl.h"
#include "cvardef.h"

static int modeRequests = 0;
static bool CallerMode(SDL_Window *window, bool enabled) {
    ++modeRequests;
    return SDL_SetWindowRelativeMouseMode(window, enabled);
}
#pragma push_macro("SDL_SetWindowRelativeMouseMode")
#undef SDL_SetWindowRelativeMouseMode
#define SDL_SetWindowRelativeMouseMode CallerMode
#define cse caller_cse
#include "sdl3_cursor.h"
#undef SDL_SetWindowRelativeMouseMode
#pragma pop_macro("SDL_SetWindowRelativeMouseMode")

static void Require(bool value, const char *expression, int line) {
    if (!value) {
        std::fprintf(stderr, "caller:%d:%s\n", line, expression);
        std::exit(FixtureAssertionFailureExit);
    }
}
#define REQUIRE(expression) Require((expression), #expression, __LINE__)

struct ControlledGame {
    void *GetMainWindow() {
        return FixtureWindow(0);
    }
    void SetCursorVisible(bool visible) {
        lastVisible = visible;
    }
    bool lastVisible = true;
};
static ControlledGame gameStorage;
static ControlledGame *game = &gameStorage;
static cvar_t m_rawinput{};
static cvar_t cl_mousegrab{};
static int usesSdlInput = 1;
static int BUsesSDLInput() {
    return usesSdlInput;
}

struct CallerState {
    bool lastRaw = false;
    float deltaX = 17.0f;
    float deltaY = 19.0f;
};
#pragma push_macro("SDL_PumpEvents")
#pragma push_macro("SDL_GetRelativeMouseState")
#undef SDL_PumpEvents
#undef SDL_GetRelativeMouseState
#define SDL_PumpEvents FixturePump
#define SDL_GetRelativeMouseState FixtureDrain
static void BaseCaller(CallerState &state, bool wasVisible, bool visible) {
    const bool s_bCursorVisible = visible;
    bool &s_bUsedRawInputLast = state.lastRaw;
    float &m_deltax = state.deltaX;
    float &m_deltay = state.deltaY;
#include "base_caller.inc"
}
static void WrapCaller(CallerState &state, bool wasVisible, bool visible) {
    const bool s_bCursorVisible = visible;
    bool &s_bUsedRawInputLast = state.lastRaw;
#include "wrap_caller.inc"
}
#undef SDL_PumpEvents
#undef SDL_GetRelativeMouseState
#pragma pop_macro("SDL_GetRelativeMouseState")
#pragma pop_macro("SDL_PumpEvents")
#undef cse

void RunCallerCases() {
    for (int owner = 0; owner < 2; ++owner) {
        FixtureReset();
        FixtureClearCounts();
        CallerState state;
        modeRequests = 0;
        m_rawinput.value = 1.0f;
        cl_mousegrab.value = 1.0f;
        FixtureFailBackend(true);
        const auto call = owner == 0 ? BaseCaller : WrapCaller;
        call(state, true, false);
        REQUIRE(modeRequests == 1 && state.lastRaw);
        REQUIRE(!SDL_GetMouse()->relative_mode && !SDL_GetWindowRelativeMouseMode(FixtureWindow(0)));
        REQUIRE(FixtureGetCounts().backendEnable == 1);
        REQUIRE(FixtureGetCounts().pumps == 1 && FixtureGetCounts().relativeDrains == 1);
        FixtureFailBackend(false);
        call(state, false, false);
        REQUIRE(modeRequests == 1 && state.lastRaw);
        REQUIRE(FixtureGetCounts().backendEnable == 1);
        REQUIRE(!SDL_GetMouse()->relative_mode);
        std::printf("actual caller=%s failure commits raw-cache: requests=%d backendenable=%d pump=%d drain=%d "
            "unchanged-request-retry=false\n", owner == 0 ? "BaseUISurface" : "EngineSurfaceWrap", modeRequests,
            FixtureGetCounts().backendEnable, FixtureGetCounts().pumps, FixtureGetCounts().relativeDrains);
    }
}
