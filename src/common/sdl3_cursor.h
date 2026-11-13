#pragma once

#include <SDL3/SDL.h>
#include "sdl3_policy.h"

namespace cse {
inline void ApplyCursorVisibility(SDL_Window *window, bool visible, bool rawInput) {
    if (rawInput) {
        // Relative mode hides the cursor itself. Keep the explicit visibility flag set so
        // another UI owner can reveal it by ending relative mode. Show first to end any
        // legacy warp emulation before enabling explicit relative mode.
        SDL_ShowCursor();
        SDL_SetWindowRelativeMouseMode(window, !visible);
    } else {
        SDL_SetWindowRelativeMouseMode(window, false);
        if (visible) {
            SDL_ShowCursor();
        } else {
            SDL_HideCursor();
        }
    }
}
} // namespace cse
