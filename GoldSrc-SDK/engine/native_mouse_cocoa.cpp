#include "native_mouse_window.h"
#include "../common/sdl3_native_cocoa_mouse.h"
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
} // namespace

NativeMousePlatform NativeMouseDefaultPlatform() {
    return {SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, DefaultOwned, DefaultFocused, DefaultAbsolute};
}
} // namespace cse
