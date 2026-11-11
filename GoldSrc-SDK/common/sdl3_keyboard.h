#pragma once

#include <SDL3/SDL.h>

namespace cse {
inline bool ConfigureMacKeyRepeat() {
#if defined(__APPLE__)
    // SDL 3.4 defaults to the macOS accent picker. Games need held keys to
    // repeat in every UI state; set this before Cocoa first registers defaults.
    return SDL_SetHintWithPriority(SDL_HINT_MAC_PRESS_AND_HOLD, "0", SDL_HINT_OVERRIDE);
#else
    return true;
#endif
}

// Match the pinned Windows baseline: French numeric rows and Latin fallback for bindings.
// Text input continues to use SDL's independent UTF-8 text events.
inline bool ConfigureKeyboardMapping() {
    if (!ConfigureMacKeyRepeat()) {
        return false;
    }
    constexpr const char *bindingOptions = "french_numbers,latin_letters";
    return SDL_SetHintWithPriority(SDL_HINT_KEYCODE_OPTIONS, bindingOptions, SDL_HINT_OVERRIDE);
}

inline SDL_Keycode KeycodeForBindingLabel(SDL_Scancode scancode) {
    // Labels must use the same policy as key events, not the raw modifier-sensitive layout.
    return SDL_GetKeyFromScancode(scancode, SDL_KMOD_NONE, true);
}
} // namespace cse
