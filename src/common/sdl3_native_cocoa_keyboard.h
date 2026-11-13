#ifndef CSE_SDL3_NATIVE_COCOA_KEYBOARD_H
#define CSE_SDL3_NATIVE_COCOA_KEYBOARD_H

#include "sdl3_native_cocoa_mouse.h"

#define CSE_SDL_COCOA_KEYBOARD_CONTRACT_V1 UINT32_C(1)

#ifdef __cplusplus
extern "C" {
#endif

// App-local Control-Space policy; enabled is exactly 0 or 1. Only Ready succeeds.
// Borrow the genuine current SDL Cocoa window property for the synchronous call.
// Serialize calls, event dispatch and window/SDK teardown on the SDL video/main
// thread. No native window or engine callback is retained. One window owns the
// provider's monitor token at a time; replacement requires successful release.
// Disable BEFORE window destruction or SDL/provider unload, and require Ready.
// A disable inside a live callback invalidates filtering but returns Rejected;
// retry after dispatch unwinds. Failed removal retains cleanup ownership. Never
// retire the window or image after a failed disable, even if filtering has stopped.
// SDL key delivery precedes the AppKit filter in the pinned SDL3.4.20 dispatcher.
CSE_SDL_NATIVE_MOUSE_API uint32_t CSE_SDL_NATIVE_MOUSE_CALL
CseSdl_SetCocoaGameplayShortcutsV1(uint32_t version, void *nativeWindow, uint32_t enabled);

#ifdef __cplusplus
}
#endif
#endif
