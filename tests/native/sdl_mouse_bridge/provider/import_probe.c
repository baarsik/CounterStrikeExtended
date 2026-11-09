#include "sdl3_native_mouse_bridge.h"

int main(void) {
    const int assertionFailure = 17;
    if (CseSdl_QueryMouseModeV1(0) != CseSdlMouseModeContractMismatch) {
        return assertionFailure;
    }
    if (CseSdl_QueryMouseModeV1(CSE_SDL_MOUSE_MODE_CONTRACT_V1) != CseSdlMouseModeNotReady) {
        return assertionFailure;
    }
    return 0;
}
