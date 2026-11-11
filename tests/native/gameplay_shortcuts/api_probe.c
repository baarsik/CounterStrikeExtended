#include "sdl3_native_cocoa_keyboard.h"
_Static_assert(CSE_SDL_COCOA_KEYBOARD_CONTRACT_V1 == 1, "keyboard ABI version");
_Static_assert(CseSdlCocoaMouseReady == 1, "shared result ABI");
uint32_t (CSE_SDL_NATIVE_MOUSE_CALL *cse_shortcut_c_signature)(uint32_t, void *, uint32_t) =
    CseSdl_SetCocoaGameplayShortcutsV1;
int main(void) {
    return cse_shortcut_c_signature(0, 0, 0) == CseSdlCocoaMouseContractMismatch ? 0 : 17;
}
