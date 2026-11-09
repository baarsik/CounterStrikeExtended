#ifndef CSE_SDL3_NATIVE_COCOA_MOUSE_H
#define CSE_SDL3_NATIVE_COCOA_MOUSE_H

#include "sdl3_native_mouse_bridge.h"

#define CSE_SDL_COCOA_MOUSE_CONTRACT_V1 UINT32_C(1)

enum CseSdlCocoaMouseResultV1 {
    CseSdlCocoaMouseNotReady = 0,
    CseSdlCocoaMouseReady = 1,
    CseSdlCocoaMouseRejected = 2,
    CseSdlCocoaMouseWrongThread = 3,
    CseSdlCocoaMouseContractMismatch = 4,
    CseSdlCocoaMouseForeignDispatch = 5,
    CseSdlCocoaMouseInvalidArgument = 6,
    CseSdlCocoaMouseWrongMode = 7
};

#ifdef __cplusplus
extern "C" {
#endif

// Apple native64 SDL3.4.20+CSE extension. Only Ready succeeds. nativeWindow is
// borrowed from the current owned SDL window's Cocoa property, never an arbitrary
// pointer. It remains valid through the synchronous call. Serialize SDL/window
// teardown; invalidate the engine binding before destroying the SDL window.
// No input is pumped, consumed or warped, and no argument/callback is retained.
CSE_SDL_NATIVE_MOUSE_API uint32_t CSE_SDL_NATIVE_MOUSE_CALL
CseSdl_CheckCocoaMouseOwnedV1(uint32_t version, void *nativeWindow);

// Requires actual Relative mode. Validates native focus and the same logical
// extent captured by the caller; this does not read any absolute cursor position.
CSE_SDL_NATIVE_MOUSE_API uint32_t CSE_SDL_NATIVE_MOUSE_CALL
CseSdl_CheckCocoaRelativeFocusV1(uint32_t version, void *nativeWindow,
                               int32_t logicalWidth, int32_t logicalHeight);

// Requires actual Absolute mode. Outputs are writable caller-owned floats; both
// are committed only on Ready. Failure leaves both untouched. Coordinates are
// top-left content-view logical points; outside-window positions are permitted.
CSE_SDL_NATIVE_MOUSE_API uint32_t CSE_SDL_NATIVE_MOUSE_CALL
CseSdl_ReadCocoaMouseAbsoluteV1(uint32_t version, void *nativeWindow,
                              int32_t logicalWidth, int32_t logicalHeight, float *x, float *y);

#ifdef __cplusplus
}
#endif
#endif
