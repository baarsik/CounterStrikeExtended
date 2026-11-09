#include "sdl3_native_mouse_bridge.h"
#include <type_traits>

using ExpectedQuery = uint32_t (CSE_SDL_NATIVE_MOUSE_CALL *)(uint32_t);
static_assert(std::is_same<decltype(&CseSdl_QueryMouseModeV1), ExpectedQuery>::value, "Scalar C prototype");
static_assert(sizeof(uint32_t) == 4, "Fixed scalar width");
static_assert(CSE_SDL_MOUSE_MODE_CONTRACT_V1 == 1, "Contract V1");

int main() {
    return 0;
}
