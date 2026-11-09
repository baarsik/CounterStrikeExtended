#ifndef CSE_SDL3_NATIVE_MOUSE_BRIDGE_H
#define CSE_SDL3_NATIVE_MOUSE_BRIDGE_H

#include <stdint.h>

#if defined(_WIN32)
#define CSE_SDL_NATIVE_MOUSE_CALL __cdecl
#if defined(CSE_SDL_NATIVE_MOUSE_BRIDGE_BUILD)
#define CSE_SDL_NATIVE_MOUSE_API __declspec(dllexport)
#else
#define CSE_SDL_NATIVE_MOUSE_API __declspec(dllimport)
#endif
#else
#define CSE_SDL_NATIVE_MOUSE_CALL
#define CSE_SDL_NATIVE_MOUSE_API __attribute__((visibility("default")))
#endif

#define CSE_SDL_MOUSE_MODE_CONTRACT_V1 UINT32_C(1)

enum CseSdlMouseModeV1Result {
    CseSdlMouseModeNotReady = 0,
    CseSdlMouseModeAbsolute = 1,
    CseSdlMouseModeRelative = 2,
    CseSdlMouseModeWrongThread = 3,
    CseSdlMouseModeContractMismatch = 4,
    CseSdlMouseModeForeignDispatch = 5
};

#ifdef __cplusplus
extern "C" {
#endif

// SDL3.4.20+CSE native64 extension; only Absolute and Relative authorize a sample.
// Read on the SDL video owner thread after public SDL video initialization.
// SDL initialization/teardown must be serialized with calls. Wrong-thread rejection
// does not make racing SDL_Quit on another thread a valid SDK operation.
// No SDL state, errors, callbacks, input queues or caller-owned storage are changed.
CSE_SDL_NATIVE_MOUSE_API uint32_t CSE_SDL_NATIVE_MOUSE_CALL
CseSdl_QueryMouseModeV1(uint32_t contractVersion);

#ifdef __cplusplus
}
#endif
#endif
