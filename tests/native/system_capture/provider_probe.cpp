#define SDL_MAIN_HANDLED 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "sdl3_native_cocoa_mouse.h"
#include <cstdio>
#include <cstring>

int main(int argc, char **argv) {
    bool relative = false;
    if (argc == 2 && std::strcmp(argv[1], "--relative") == 0) {
        relative = true;
    } else if (argc != 1 && !(argc == 2 && std::strcmp(argv[1], "--absolute") == 0)) {
        std::fprintf(stderr, "usage: provider_probe [--absolute|--relative]\n");
        return 2;
    }
    SDL_SetMainReady();
    // Intentionally video only. Never initialize SDL audio or recording devices.
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "video: %s\n", SDL_GetError());
        return 3;
    }
    constexpr int Width = 960;
    constexpr int Height = 640;
    SDL_Window *window = SDL_CreateWindow("CSE interaction owner probe — Cmd+Shift+4", Width, Height, 0);
    if (!window || (relative && !SDL_SetWindowRelativeMouseMode(window, true))) {
        std::fprintf(stderr, "window/mode: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 4;
    }
    constexpr Uint64 SampleIntervalNanoseconds = UINT64_C(10000000);
    constexpr Uint64 DurationNanoseconds = UINT64_C(60000000000);
    const Uint64 start = SDL_GetTicksNS();
    Uint64 nextSample = start;
    bool running = true;
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    while (running && SDL_GetTicksNS() - start < DurationNanoseconds) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                running = false;
            }
        }
        const Uint64 now = SDL_GetTicksNS();
        if (now >= nextSample) {
            // Fetch the genuine current borrowed property for each synchronous call.
            void *native = SDL_GetPointerProperty(SDL_GetWindowProperties(window),
                SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
            const uint32_t result = CseSdl_CheckCocoaInteractionOwnerV1(CSE_SDL_COCOA_MOUSE_CONTRACT_V1, native);
            const uint32_t mode = CseSdl_QueryMouseModeV1(CSE_SDL_MOUSE_MODE_CONTRACT_V1);
            std::printf("capture_owner elapsed_ns=%llu mode=%u result=%u\n",
                static_cast<unsigned long long>(now - start), mode, result);
            // Never catch up with a burst after a slow/event-tracking interval.
            nextSample = SDL_GetTicksNS() + SampleIntervalNanoseconds;
        }
        SDL_Delay(1);
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
