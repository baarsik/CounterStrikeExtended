// Native SDL3 regression for shared VGUI2/VGUI1 cursor ownership.
#define SDL_MAIN_HANDLED
#include <windows.h>
#include <SDL3/SDL_main.h>
#include "../../GoldSrc-SDK/common/sdl3_cursor.h"
#include <cstdio>
#include <cstring>

namespace {
struct CursorOwner {
    bool visible;
    bool rawInput;
};

bool before = false;
int failures = 0;

void BaseUIRequest(SDL_Window *window, CursorOwner &owner, bool visible, bool rawInput, bool mouseGrab) {
    bool changed = cse::CursorRequestChanged(owner.visible, visible, owner.rawInput, rawInput);
    if (before) {
        changed = changed || SDL_GetWindowRelativeMouseMode(window) != (!visible && rawInput);
    }
    if (changed) {
        if (before) {
            SDL_SetWindowRelativeMouseMode(window, !visible && rawInput);
            if (visible) {
                SDL_ShowCursor();
            } else {
                SDL_HideCursor();
            }
        } else {
            cse::ApplyCursorVisibility(window, visible, rawInput);
        }
        SDL_SetWindowMouseGrab(window, !visible && mouseGrab);
    }
    owner.visible = visible;
    owner.rawInput = rawInput;
}

void VGUI1Request(SDL_Window *window, CursorOwner &owner, bool visible, bool rawInput) {
    if (before) {
        if (owner.visible != visible) {
            if (rawInput) {
                SDL_SetWindowRelativeMouseMode(window, !visible);
            } else if (visible) {
                SDL_ShowCursor();
            } else {
                SDL_HideCursor();
            }
        }
    } else if (cse::CursorRequestChanged(owner.visible, visible, owner.rawInput, rawInput)) {
        cse::ApplyCursorVisibility(window, visible, rawInput);
    }
    owner.visible = visible;
    owner.rawInput = rawInput;
}

void Check(SDL_Window *window, const char *stage, bool relative, bool visibleFlag, bool mouseGrab) {
    SDL_PumpEvents();
    const bool actualRelative = SDL_GetWindowRelativeMouseMode(window);
    const bool actualVisible = SDL_CursorVisible();
    const bool actualGrab = SDL_GetWindowMouseGrab(window);
    const bool passed = actualRelative == relative && actualVisible == visibleFlag && actualGrab == mouseGrab;
    std::printf("%s: relative=%d explicit_visible=%d manual_grab=%d %s\n", stage,
                static_cast<int>(actualRelative), static_cast<int>(actualVisible), static_cast<int>(actualGrab),
                passed ? "PASS" : "FAIL");
    if (!passed) {
        ++failures;
    }
}
} // namespace

int main(int argc, char **argv) {
    before = argc > 1 && std::strcmp(argv[1], "--before") == 0;
    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    constexpr int windowWidth = 320;
    constexpr int windowHeight = 200;
    SDL_Window *window = SDL_CreateWindow("SDL cursor ownership regression", windowWidth, windowHeight, 0);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    HWND nativeWindow = static_cast<HWND>(
        SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    SDL_RaiseWindow(window);
    SetForegroundWindow(nativeWindow);
    SetFocus(nativeWindow);
    SDL_SyncWindow(window);
    SDL_WarpMouseInWindow(window, static_cast<float>(windowWidth / 2), static_cast<float>(windowHeight / 2));
    SDL_PumpEvents();
    if (SDL_GetKeyboardFocus() != window || SDL_GetMouseFocus() != window) {
        std::fprintf(stderr, "Fixture requires an interactive foreground window with mouse focus\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    CursorOwner baseUI = {true, false};
    CursorOwner vgui1 = {false, false};
    cse::ApplyCursorVisibility(window, true, false);
    SDL_SetWindowMouseGrab(window, false);
    Check(window, "initial visible", false, true, false);

    BaseUIRequest(window, baseUI, false, true, true);
    Check(window, "VGUI2 raw hidden", true, true, true);
    VGUI1Request(window, vgui1, true, true);
    Check(window, "VGUI1 raw visible handoff", false, true, true);
    BaseUIRequest(window, baseUI, false, true, true);
    Check(window, "unchanged VGUI2 request preserves VGUI1", false, true, true);
    BaseUIRequest(window, baseUI, true, true, true);
    Check(window, "VGUI2 releases manual grab", false, true, false);

    VGUI1Request(window, vgui1, false, true);
    Check(window, "VGUI1 raw hidden", true, true, false);
    VGUI1Request(window, vgui1, false, false);
    Check(window, "VGUI1 hidden raw to legacy", false, false, false);
    VGUI1Request(window, vgui1, false, true);
    Check(window, "VGUI1 hidden legacy to raw", true, true, false);
    VGUI1Request(window, vgui1, true, true);
    Check(window, "VGUI1 visible after toggles", false, true, false);

    BaseUIRequest(window, baseUI, false, true, false);
    Check(window, "VGUI2 raw hidden without manual grab", true, true, false);
    BaseUIRequest(window, baseUI, false, false, false);
    Check(window, "VGUI2 hidden raw to legacy", false, false, false);
    BaseUIRequest(window, baseUI, false, true, false);
    Check(window, "VGUI2 hidden legacy to raw", true, true, false);
    BaseUIRequest(window, baseUI, true, true, false);
    Check(window, "VGUI2 visible after toggles", false, true, false);

    VGUI1Request(window, vgui1, false, false);
    Check(window, "VGUI1 legacy hidden", false, false, false);
    VGUI1Request(window, vgui1, true, false);
    Check(window, "VGUI1 legacy visible", false, true, false);

    cse::ApplyCursorVisibility(window, true, false);
    SDL_SetWindowMouseGrab(window, false);
    SDL_DestroyWindow(window);
    SDL_Quit();
    std::printf("cursor ownership failures: %d\n", failures);
    return failures == 0 ? 0 : 1;
}
