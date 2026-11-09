#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include "fixture_sdl.h"

static int modeChecks = 0;
#define REQUIRE(expression) do { ++modeChecks; if (!(expression)) { \
    std::fprintf(stderr, "sdl-mode:%d:%s\n", __LINE__, #expression); std::exit(FixtureAssertionFailureExit); \
} } while (false)

static int modeRequests = 0;
static int showRequests = 0;
static int hideRequests = 0;
static bool CountedMode(SDL_Window *window, bool enabled) {
    ++modeRequests;
    return SDL_SetWindowRelativeMouseMode(window, enabled);
}
static bool CountedShow() {
    ++showRequests;
    return SDL_ShowCursor();
}
static bool CountedHide() {
    ++hideRequests;
    return SDL_HideCursor();
}
#pragma push_macro("SDL_SetWindowRelativeMouseMode")
#pragma push_macro("SDL_ShowCursor")
#pragma push_macro("SDL_HideCursor")
#undef SDL_SetWindowRelativeMouseMode
#undef SDL_ShowCursor
#undef SDL_HideCursor
#define SDL_SetWindowRelativeMouseMode CountedMode
#define SDL_ShowCursor CountedShow
#define SDL_HideCursor CountedHide
#include "sdl3_cursor.h"
#undef SDL_SetWindowRelativeMouseMode
#undef SDL_ShowCursor
#undef SDL_HideCursor
#pragma pop_macro("SDL_HideCursor")
#pragma pop_macro("SDL_ShowCursor")
#pragma pop_macro("SDL_SetWindowRelativeMouseMode")

static void Layout() {
    std::size_t mouseBytes = 0;
    std::size_t windowBytes = 0;
    std::size_t offsets[8]{};
    FixtureGetLayout(&mouseBytes, &windowBytes, offsets);
    REQUIRE(mouseBytes == sizeof(SDL_Mouse) && windowBytes == sizeof(SDL_Window));
    if (sizeof(void *) > sizeof(std::uint32_t)) {
        REQUIRE(reinterpret_cast<std::uintptr_t>(SDL_GetMouse()) > UINT32_MAX);
        REQUIRE(reinterpret_cast<std::uintptr_t>(FixtureWindow(0)) > UINT32_MAX);
    }
    const std::size_t expected[] = {offsetof(SDL_Mouse, relative_mode), offsetof(SDL_Mouse, warp_emulation_active),
        offsetof(SDL_Mouse, warp_emulation_hint), offsetof(SDL_Mouse, warp_emulation_prohibited),
        offsetof(SDL_Mouse, x), offsetof(SDL_Mouse, last_x), offsetof(SDL_Window, flags), offsetof(SDL_Window, id)};
    REQUIRE(std::memcmp(offsets, expected, sizeof(expected)) == 0);
    std::printf("compiled fixture declarations: pointer=%zu SDL_Mouse=%zu SDL_Window=%zu; not frozen-PDB layout proof\n",
        sizeof(void *), mouseBytes, windowBytes);
}

static bool FailedDisable() {
    FixtureReset();
    SDL_Mouse *mouse = SDL_GetMouse();
    mouse->relative_mode = true;
    mouse->warp_emulation_active = true;
    FixtureFailBackend(true);
    const bool success = SDL_SetWindowRelativeMouseMode(FixtureWindow(0), false);
    REQUIRE(success && !SDL_GetWindowRelativeMouseMode(FixtureWindow(0)));
    REQUIRE(mouse->relative_mode && !mouse->warp_emulation_active && mouse->warp_emulation_prohibited);
    REQUIRE(FixtureGetCounts().backendDisable == 1 && FixtureGetCounts().motionFlushes == 0);
    return !mouse->relative_mode;
}

static void InitialAndBranches() {
    for (bool raw : {false, true}) {
        for (bool visible : {false, true}) {
            FixtureReset();
            modeRequests = showRequests = hideRequests = 0;
            cse::ApplyCursorVisibility(FixtureWindow(0), visible, raw);
            SDL_Mouse *mouse = SDL_GetMouse();
            REQUIRE(modeRequests == 1);
            REQUIRE(mouse->warp_emulation_prohibited && !mouse->warp_emulation_active);
            REQUIRE(SDL_GetWindowRelativeMouseMode(FixtureWindow(0)) == (raw && !visible));
            REQUIRE(mouse->relative_mode == (raw && !visible));
            REQUIRE(mouse->cursor_visible == (raw || visible));
            REQUIRE(showRequests == (raw || visible ? 1 : 0));
            REQUIRE(hideRequests == (!raw && !visible ? 1 : 0));
            REQUIRE(FixtureGetCounts().backendEnable == (raw && !visible ? 1 : 0));
            REQUIRE(FixtureGetCounts().backendDisable == 0);
        }
    }
}

static void ResetPublicCounts() {
    FixtureClearCounts();
    modeRequests = showRequests = hideRequests = 0;
}

static void InitializationAndEmulation() {
    FixtureReset();
    SDL_Mouse *mouse = SDL_GetMouse();
    REQUIRE(!mouse->relative_mode && !mouse->warp_emulation_active && !mouse->warp_emulation_prohibited);
    REQUIRE(mouse->warp_emulation_hint && mouse->cursor_visible && mouse->relative_mode_hide_cursor);
    REQUIRE(FixtureGetCounts().hintCallbacks == 14 && FixtureGetCounts().hashCreates == 1);
    cse::ApplyCursorVisibility(FixtureWindow(0), false, false);
    REQUIRE(mouse->warp_emulation_prohibited && !mouse->cursor_visible);
    ResetPublicCounts();
    FixtureWarp(320.0f, 240.0f);
    FixtureClock(SDL_MS_TO_NS(110));
    FixtureWarp(320.0f, 240.0f);
    REQUIRE(!mouse->relative_mode && !mouse->warp_emulation_active);
    REQUIRE(FixtureGetCounts().backendEnable == 0 && FixtureGetCounts().backendWarps == 2);
    FixtureReset();
    mouse = SDL_GetMouse();
    FixtureInitialCursor(false);
    ResetPublicCounts();
    FixtureWarp(320.0f, 240.0f);
    FixtureClock(SDL_MS_TO_NS(110));
    FixtureWarp(320.0f, 240.0f);
    REQUIRE(mouse->relative_mode && mouse->warp_emulation_active && !mouse->warp_emulation_prohibited);
    REQUIRE(!SDL_GetWindowRelativeMouseMode(FixtureWindow(0)));
    REQUIRE(FixtureGetCounts().backendEnable == 1 && FixtureGetCounts().backendWarps == 1);
    FixtureReset();
    mouse = SDL_GetMouse();
    FixtureInitialCursor(false);
    FixtureFailBackend(true);
    FixtureWarp(320.0f, 240.0f);
    FixtureClock(SDL_MS_TO_NS(110));
    FixtureWarp(320.0f, 240.0f);
    REQUIRE(!mouse->relative_mode && !mouse->warp_emulation_active);
    REQUIRE(FixtureGetCounts().backendEnable == 1 && FixtureGetCounts().backendWarps == 2);
    FixtureFailBackend(false);
    FixtureClock(SDL_MS_TO_NS(120));
    FixtureWarp(320.0f, 240.0f);
    REQUIRE(mouse->relative_mode && mouse->warp_emulation_active);
    REQUIRE(FixtureGetCounts().backendEnable == 2);
}

static void ExplicitFailureRetry() {
    FixtureReset();
    SDL_Window *window = FixtureWindow(0);
    SDL_Mouse *mouse = SDL_GetMouse();
    ResetPublicCounts();
    FixtureFailBackend(true);
    REQUIRE(!SDL_SetWindowRelativeMouseMode(window, true));
    REQUIRE(!SDL_GetWindowRelativeMouseMode(window) && !mouse->relative_mode);
    REQUIRE(FixtureGetCounts().backendEnable == 1 && FixtureGetCounts().motionFlushes == 0);
    FixtureFailBackend(false);
    REQUIRE(SDL_SetWindowRelativeMouseMode(window, true));
    REQUIRE(SDL_GetWindowRelativeMouseMode(window) && mouse->relative_mode);
    const FixtureCounts enabled = FixtureGetCounts();
    REQUIRE(SDL_SetWindowRelativeMouseMode(window, true));
    REQUIRE(FixtureGetCounts().backendEnable == enabled.backendEnable);
    REQUIRE(FixtureGetCounts().motionFlushes == enabled.motionFlushes);
    FixtureFailBackend(true);
    REQUIRE(!SDL_SetWindowRelativeMouseMode(window, false));
    REQUIRE(SDL_GetWindowRelativeMouseMode(window) && mouse->relative_mode);
    FixtureFailBackend(false);
    REQUIRE(SDL_SetWindowRelativeMouseMode(window, false));
    REQUIRE(!SDL_GetWindowRelativeMouseMode(window) && !mouse->relative_mode);
    REQUIRE(FixtureGetCounts().backendDisable == 2);
    REQUIRE(FixtureGetCounts().motionFlushes == 2);
    std::printf("explicit failure/retry: enable=%d disable=%d flush=%d drain=%d\n",
        FixtureGetCounts().backendEnable, FixtureGetCounts().backendDisable,
        FixtureGetCounts().motionFlushes, FixtureGetCounts().relativeDrains);
}

static void FocusAndOtherWindow() {
    FixtureReset();
    SDL_Window *first = FixtureWindow(0);
    SDL_Window *second = FixtureWindow(1);
    SDL_Mouse *mouse = SDL_GetMouse();
    FixtureSetFocus(nullptr);
    REQUIRE(SDL_SetWindowRelativeMouseMode(first, true));
    REQUIRE(SDL_GetWindowRelativeMouseMode(first) && !mouse->relative_mode);
    REQUIRE(FixtureGetCounts().backendEnable == 0);
    FixtureFailBackend(true);
    FixtureSetFocus(first);
    REQUIRE(!SDL_UpdateRelativeMouseMode());
    REQUIRE(!mouse->relative_mode && SDL_GetWindowRelativeMouseMode(first));
    const int attempts = FixtureGetCounts().backendEnable;
    REQUIRE(SDL_SetWindowRelativeMouseMode(first, true));
    REQUIRE(FixtureGetCounts().backendEnable == attempts && !mouse->relative_mode);
    FixtureFailBackend(false);
    REQUIRE(SDL_UpdateRelativeMouseMode());
    REQUIRE(mouse->relative_mode);
    FixtureSetFocus(nullptr);
    FixtureFailBackend(true);
    REQUIRE(!SDL_UpdateRelativeMouseMode() && mouse->relative_mode);
    FixtureFailBackend(false);
    REQUIRE(SDL_UpdateRelativeMouseMode() && !mouse->relative_mode);
    FixtureSetFocus(second);
    REQUIRE(SDL_SetWindowRelativeMouseMode(second, true) && mouse->relative_mode);
    REQUIRE(SDL_SetWindowRelativeMouseMode(first, false));
    REQUIRE(mouse->relative_mode && !SDL_GetWindowRelativeMouseMode(first));
    REQUIRE(SDL_GetWindowRelativeMouseMode(second));
    FixtureWindowAlive(0, false, first->id);
    REQUIRE(!SDL_SetWindowRelativeMouseMode(first, false));
    FixtureWindowAlive(0, true, FixtureReplacementWindowId);
    REQUIRE(first == FixtureWindow(0) && first->id == FixtureReplacementWindowId);
    REQUIRE(SDL_SetWindowRelativeMouseMode(first, false) && mouse->relative_mode);
    REQUIRE(mouse->warp_emulation_prohibited);
    FixtureReset();
    REQUIRE(!SDL_GetMouse()->warp_emulation_prohibited && !SDL_GetMouse()->relative_mode);
    REQUIRE(SDL_GetMouse()->cursor_visible);
}

static void FailedShowAndHint() {
    for (bool viaHint : {false, true}) {
        FixtureReset();
        SDL_Mouse *mouse = SDL_GetMouse();
        mouse->relative_mode = true;
        mouse->warp_emulation_active = true;
        mouse->cursor_visible = false;
        FixtureFailBackend(true);
        if (viaHint) {
            FixtureHint("0");
            REQUIRE(!mouse->warp_emulation_hint);
        } else {
            REQUIRE(SDL_ShowCursor());
            REQUIRE(mouse->cursor_visible);
        }
        REQUIRE(mouse->relative_mode && !mouse->warp_emulation_active);
        REQUIRE(!SDL_GetWindowRelativeMouseMode(FixtureWindow(0)));
        REQUIRE(SDL_SetWindowRelativeMouseMode(FixtureWindow(0), false));
        REQUIRE(mouse->relative_mode && mouse->warp_emulation_prohibited);
        REQUIRE(FixtureGetCounts().backendDisable == 1);
    }
}

static void QualifiedAbsoluteHistory() {
    FixtureReset();
    SDL_Window *window = FixtureWindow(0);
    SDL_Mouse *mouse = SDL_GetMouse();
    cse::ApplyCursorVisibility(window, true, false);
    REQUIRE(mouse->warp_emulation_prohibited && !mouse->relative_mode);
    constexpr std::uint32_t sequenceSeed = 0x13579bdfu;
    constexpr std::uint32_t sequenceMultiplier = 1664525u;
    constexpr std::uint32_t sequenceIncrement = 1013904223u;
    constexpr int operationCount = 8;
    constexpr int historySteps = 4096;
    std::uint32_t state = sequenceSeed;
    for (int step = 0; step < historySteps; ++step) {
        state = state * sequenceMultiplier + sequenceIncrement;
        FixtureFailBackend((state & 0x100u) != 0);
        switch ((state >> 16) % operationCount) {
        case 0:
            SDL_SetWindowRelativeMouseMode(window, (state & 1u) != 0);
            break;
        case 1:
            FixtureSetFocus((state & 1u) ? window : nullptr);
            SDL_UpdateRelativeMouseMode();
            break;
        case 2:
            SDL_ShowCursor();
            break;
        case 3:
            SDL_HideCursor();
            break;
        case 4:
            FixtureWarp(static_cast<float>(FixtureCenterX), static_cast<float>(FixtureCenterY));
            break;
        case 5:
            FixtureHint((state & 1u) ? "1" : "0");
            break;
        case 6:
            cse::ApplyCursorVisibility(window, (state & 1u) != 0, (state & 2u) != 0);
            break;
        default:
            SDL_UpdateRelativeMouseMode();
            break;
        }
        REQUIRE(mouse->warp_emulation_prohibited && !mouse->warp_emulation_active);
        if (!SDL_GetWindowRelativeMouseMode(window)) {
            REQUIRE(!mouse->relative_mode);
        }
    }
    std::printf("qualified fresh sole-window history: %d seeded actual-SDK operations preserve publicfalse=>globalfalse; "
        "not an arbitrary-lifetime guarantee\n", historySteps);
    FixtureReset();
    window = FixtureWindow(0);
    mouse = SDL_GetMouse();
    REQUIRE(SDL_SetWindowRelativeMouseMode(window, true));
    FixtureFailBackend(true);
    FixtureSetFocus(nullptr);
    REQUIRE(!SDL_UpdateRelativeMouseMode() && mouse->relative_mode);
    FixtureWindowAlive(0, false, window->id);
    std::memset(window, 0, sizeof(*window));
    window->w = FixtureWindowWidth;
    window->h = FixtureWindowHeight;
    FixtureWindowAlive(0, true, FixtureReplacementWindowId);
    FixtureSetFocus(window);
    REQUIRE(SDL_SetWindowRelativeMouseMode(window, false));
    REQUIRE(!SDL_GetWindowRelativeMouseMode(window) && mouse->relative_mode);
    std::puts("controlled window-storage reuse after failed disable invalidates prior-generation authority");
}

void RunCameraCases();
void RunCallerCases();

int main(int argc, char **argv) {
    Layout();
    const bool absolute = FailedDisable();
    if (argc == 2 && std::strcmp(argv[1], "--naive-authority-hypothesis") == 0) {
        REQUIRE(absolute);
    }
    REQUIRE(!absolute);
    InitialAndBranches();
    InitializationAndEmulation();
    ExplicitFailureRetry();
    FocusAndOtherWindow();
    FailedShowAndHint();
    QualifiedAbsoluteHistory();
    REQUIRE(FixtureGetCounts().relativeDrains == 0);
    RunCallerCases();
    RunCameraCases();
    REQUIRE(FixtureUnexpectedCount() == 0);
    std::printf("SDL mode characterization checks=%d; production readiness=false\n", modeChecks);
    return 0;
}
