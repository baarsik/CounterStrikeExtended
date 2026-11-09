#include "sdl3_native_cocoa_mouse.h"
#include <stdio.h>
#include <stdint.h>

int main(void) {
    void *const poison = (void *)(uintptr_t)1;
    float x = 123.5f;
    float y = 456.5f;
    if (CseSdl_CheckCocoaMouseOwnedV1(0, poison) != CseSdlCocoaMouseContractMismatch ||
        CseSdl_CheckCocoaMouseOwnedV1(CSE_SDL_COCOA_MOUSE_CONTRACT_V1, NULL) != CseSdlCocoaMouseInvalidArgument ||
        CseSdl_CheckCocoaMouseOwnedV1(CSE_SDL_COCOA_MOUSE_CONTRACT_V1, poison) != CseSdlCocoaMouseNotReady ||
        CseSdl_CheckCocoaRelativeFocusV1(CSE_SDL_COCOA_MOUSE_CONTRACT_V1, poison, 640, 480) !=
            CseSdlCocoaMouseNotReady ||
        CseSdl_ReadCocoaMouseAbsoluteV1(CSE_SDL_COCOA_MOUSE_CONTRACT_V1, poison, 640, 480, &x, &y) !=
            CseSdlCocoaMouseNotReady ||
        x != 123.5f || y != 456.5f) {
        fputs("cocoa-c-abi-guard-failed\n", stderr);
        return 17;
    }
    puts("C99 scalar imports: mismatch/arguments/preinit reject with atomic outputs");
    return 0;
}
