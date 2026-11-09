// Owned SDL3.4.20+CSE mouse-query V1 extension. Upstream source and dynapi are unchanged.
// Compile only inside the native64 SDL3-shared target with its generated configuration.
#define CSE_SDL_NATIVE_MOUSE_BRIDGE_BUILD 1
#include "sdl3_native_mouse_bridge.h"
#include "SDL_internal.h"
#include "events/SDL_mouse_c.h"
#include "video/SDL_sysvideo.h"

#if SDL_MAJOR_VERSION != 3 || SDL_MINOR_VERSION != 4 || SDL_MICRO_VERSION != 20
#error Requalify the CSE mouse bridge against the selected SDL version
#endif
#if !SDL_DYNAMIC_API
#error The qualified CSE native SDL provider retains upstream dynapi
#endif

SDL_COMPILE_TIME_ASSERT(CseMouseBridgeNativePointers, sizeof(void *) == sizeof(uint64_t));
SDL_COMPILE_TIME_ASSERT(CseMouseBridgeScalarWidth, sizeof(uint32_t) == 4);

// SDL_internal maps public names to local REAL implementations and clears
// SDL_DECLSPEC. Explicitly retain the public getter and its original visibility.
#pragma push_macro("SDL_GetDefaultAssertionHandler")
#undef SDL_GetDefaultAssertionHandler
extern CSE_SDL_NATIVE_MOUSE_API SDL_AssertionHandler SDLCALL SDL_GetDefaultAssertionHandler(void);

static bool CseSdl_UsesLocalDispatch(void) {
    // Compare typed C function identities; neither assertion handler is called.
    return SDL_GetDefaultAssertionHandler() == SDL_GetDefaultAssertionHandler_REAL();
}

#pragma pop_macro("SDL_GetDefaultAssertionHandler")

uint32_t CSE_SDL_NATIVE_MOUSE_CALL CseSdl_QueryMouseModeV1(uint32_t contractVersion) {
    SDL_VideoDevice *video;

    if (contractVersion != CSE_SDL_MOUSE_MODE_CONTRACT_V1) {
        return CseSdlMouseModeContractMismatch;
    }
    // Use the stable SDK main-thread check before touching video-device memory.
    if (!SDL_IsMainThread()) {
        return CseSdlMouseModeWrongThread;
    }
    if ((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0) {
        return CseSdlMouseModeNotReady;
    }
    video = SDL_GetVideoDevice();
    if (!video || video->is_quitting) {
        return CseSdlMouseModeNotReady;
    }
    if (!SDL_OnVideoThread()) {
        return CseSdlMouseModeWrongThread;
    }
    // Public video initialization has already bound dynapi before this query.
    if (!CseSdl_UsesLocalDispatch()) {
        return CseSdlMouseModeForeignDispatch;
    }
    return SDL_GetRelativeMouseMode() ? CseSdlMouseModeRelative : CseSdlMouseModeAbsolute;
}
