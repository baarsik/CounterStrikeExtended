// Video-only exported-policy probe. Controller owns native input and layout observation.
#define SDL_MAIN_HANDLED 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "sdl3_native_cocoa_keyboard.h"
#include <cstdio>
#include <cstring>
#include <type_traits>
static_assert(std::is_same<decltype(&CseSdl_SetCocoaGameplayShortcutsV1),
    uint32_t (*)(uint32_t, void *, uint32_t)>::value, "fixed C policy signature");
int main(int argc, char **argv) {
    bool relative = false;
    bool enabled = false;
    bool text = true;
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--relative") == 0) {
            relative = true;
        } else if (std::strcmp(argv[index], "--enabled") == 0) {
            enabled = true;
        } else if (std::strcmp(argv[index], "--text-off") == 0) {
            text = false;
        } else {
            return 2;
        }
    }
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "video: %s\n", SDL_GetError());
        return 3;
    }
    constexpr int Width = 960;
    constexpr int Height = 640;
    SDL_Window *window = SDL_CreateWindow("CSE exported gameplay shortcuts: F1 toggle, F2 text, Esc exit",
        Width, Height, 0);
    if (!window) {
        SDL_Quit();
        return 4;
    }
    void *native = SDL_GetPointerProperty(SDL_GetWindowProperties(window),
        SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
    if (relative && !SDL_SetWindowRelativeMouseMode(window, true)) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 5;
    }
    if (text) {
        SDL_StartTextInput(window);
    } else {
        SDL_StopTextInput(window);
    }
    uint32_t result = CseSdl_SetCocoaGameplayShortcutsV1(1, native, enabled ? 1 : 0);
    std::printf("policy enabled=%d text=%d relative=%d result=%u\n", enabled, text, relative, result);
    bool running = result == CseSdlCocoaMouseReady;
    int exitCode = running ? 0 : 6;
    constexpr Uint64 DurationNanoseconds = UINT64_C(120000000000);
    const Uint64 start = SDL_GetTicksNS();
    while (running && SDL_GetTicksNS() - start < DurationNanoseconds) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
                std::printf("key type=%u scancode=%u key=%u mod=%u repeat=%d\n", event.type,
                    static_cast<unsigned int>(event.key.scancode), event.key.key, event.key.mod, event.key.repeat);
                if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                    if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                        running = false;
                    } else if (event.key.scancode == SDL_SCANCODE_F1) {
                        enabled = !enabled;
                        result = CseSdl_SetCocoaGameplayShortcutsV1(1, native, enabled ? 1 : 0);
                        std::printf("policy enabled=%d result=%u\n", enabled, result);
                    } else if (event.key.scancode == SDL_SCANCODE_F2) {
                        text = !text;
                        if (text) {
                            SDL_StartTextInput(window);
                        } else {
                            SDL_StopTextInput(window);
                        }
                        std::printf("text enabled=%d\n", text);
                    }
                }
            } else if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED ||
                       event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
                std::printf("focus type=%u\n", event.type);
            } else if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                running = false;
            }
        }
        SDL_Delay(1);
    }
    result = CseSdl_SetCocoaGameplayShortcutsV1(1, native, 0);
    std::printf("retire result=%u\n", result);
    if (result != CseSdlCocoaMouseReady) {
        // Never destroy/unload after refused retirement. Process exit owns cleanup.
        return 7;
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    return exitCode;
}
