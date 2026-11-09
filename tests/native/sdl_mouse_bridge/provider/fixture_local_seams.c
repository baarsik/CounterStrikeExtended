// Fixture-only lifecycle entry points. Never selected into the production provider.
#define CSE_SDL_NATIVE_MOUSE_BRIDGE_BUILD 1
#include "../../../../GoldSrc-SDK/common/sdl3_native_mouse_bridge.h"
#include "SDL_internal.h"
#include <SDL3/SDL_main.h>

CSE_SDL_NATIVE_MOUSE_API uint32_t CSE_SDL_NATIVE_MOUSE_CALL CseSdlFixture_InitLocalVideo(void) {
    // Explicit REAL calls set up this image after public A->B dispatch was initialized by the host.
    SDL_SetMainReady();
    return SDL_InitSubSystem(SDL_INIT_VIDEO) ? UINT32_C(1) : UINT32_C(0);
}

CSE_SDL_NATIVE_MOUSE_API void CSE_SDL_NATIVE_MOUSE_CALL CseSdlFixture_QuitLocalVideo(void) {
    SDL_Quit();
}
