#include "sdl3_native_cocoa_mouse.h"

_Static_assert(CseSdlCocoaMouseNotReady == 0, "preserve existing status");
_Static_assert(CseSdlCocoaMouseReady == 1, "preserve existing status");
_Static_assert(CseSdlCocoaMouseRejected == 2, "preserve existing status");
_Static_assert(CseSdlCocoaMouseWrongThread == 3, "preserve existing status");
_Static_assert(CseSdlCocoaMouseContractMismatch == 4, "preserve existing status");
_Static_assert(CseSdlCocoaMouseForeignDispatch == 5, "preserve existing status");
_Static_assert(CseSdlCocoaMouseInvalidArgument == 6, "preserve existing status");
_Static_assert(CseSdlCocoaMouseWrongMode == 7, "preserve existing status");
_Static_assert(CseSdlCocoaMouseNotInteractionOwner == 8, "add distinct interaction result");

uint32_t (CSE_SDL_NATIVE_MOUSE_CALL *cse_interaction_c_import)(uint32_t, void *) =
    &CseSdl_CheckCocoaInteractionOwnerV1;
