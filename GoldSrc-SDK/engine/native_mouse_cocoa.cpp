#include "native_mouse_window.h"
#include "../common/sdl3_native_cocoa_mouse.h"
#include "../common/sdl3_native_cocoa_keyboard.h"
#include <limits>

namespace cse {
namespace {
static_assert(std::numeric_limits<int>::digits == 31, "Cocoa mouse C ABI requires signed32 extents");

bool DefaultOwned(void *window) {
    return CseSdl_CheckCocoaMouseOwnedV1(CSE_SDL_COCOA_MOUSE_CONTRACT_V1, window) == CseSdlCocoaMouseReady;
}

bool DefaultFocused(void *window, int width, int height) {
    return CseSdl_CheckCocoaRelativeFocusV1(CSE_SDL_COCOA_MOUSE_CONTRACT_V1, window, width, height) ==
           CseSdlCocoaMouseReady;
}

bool DefaultAbsolute(void *window, int width, int height, float *x, float *y) {
    return CseSdl_ReadCocoaMouseAbsoluteV1(CSE_SDL_COCOA_MOUSE_CONTRACT_V1, window, width, height, x, y) ==
           CseSdlCocoaMouseReady;
}

bool DefaultGameplayCursor(void *window, bool hidden) {
    return CseSdl_SetCocoaGameplayCursorV1(CSE_SDL_COCOA_MOUSE_CONTRACT_V1, window, hidden ? 1U : 0U) ==
           CseSdlCocoaMouseReady;
}
std::uint32_t DefaultInteraction(void *window) {
    return CseSdl_CheckCocoaInteractionOwnerV1(CSE_SDL_COCOA_MOUSE_CONTRACT_V1, window);
}

bool DefaultGameplayShortcuts(void *window, bool enabled) {
    return CseSdl_SetCocoaGameplayShortcutsV1(CSE_SDL_COCOA_KEYBOARD_CONTRACT_V1, window, enabled ? 1U : 0U) ==
           CseSdlCocoaMouseReady;
}
} // namespace

NativeMousePlatform NativeMouseDefaultPlatform() {
    return {SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, DefaultOwned, DefaultFocused, DefaultAbsolute,
            DefaultGameplayCursor, DefaultGameplayShortcuts, DefaultInteraction};
}
} // namespace cse
