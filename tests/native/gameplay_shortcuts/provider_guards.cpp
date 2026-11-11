#define SDL_MAIN_HANDLED 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "sdl3_native_cocoa_keyboard.h"
#include <cstdio>
#include <thread>

int main() {
    void *poison = reinterpret_cast<void *>(static_cast<uintptr_t>(1));
    if (CseSdl_SetCocoaGameplayShortcutsV1(0, poison, 2) != CseSdlCocoaMouseContractMismatch ||
        CseSdl_SetCocoaGameplayShortcutsV1(1, nullptr, 1) != CseSdlCocoaMouseInvalidArgument ||
        CseSdl_SetCocoaGameplayShortcutsV1(1, poison, 2) != CseSdlCocoaMouseInvalidArgument ||
        CseSdl_SetCocoaGameplayShortcutsV1(1, poison, 1) != CseSdlCocoaMouseNotReady) {
        return 17;
    }
    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return 18;
    }
    uint32_t workerResult = CseSdlCocoaMouseReady;
    std::thread worker([&] { workerResult = CseSdl_SetCocoaGameplayShortcutsV1(1, poison, 0); });
    worker.join();
    SDL_Quit();
    if (workerResult != CseSdlCocoaMouseWrongThread ||
        CseSdl_SetCocoaGameplayShortcutsV1(1, poison, 0) != CseSdlCocoaMouseNotReady) {
        return 19;
    }
    std::puts("real exported provider: C guards, preinit, worker, post-quit passed; dummy video only");
    return 0;
}
