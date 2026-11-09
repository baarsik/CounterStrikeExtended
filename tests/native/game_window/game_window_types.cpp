#include <type_traits>
#include <utility>

#include "sys_sdlwind.h"

#if defined(_WIN32)
using ExpectedGameWindow = HWND;
#else
using ExpectedGameWindow = SDL_Window*;
#endif

static_assert(std::is_same<decltype(std::declval<IGame&>().GetMainWindow()), ExpectedGameWindow>::value,
    "IGame retains a pointer-width game window handle");
static_assert(std::is_same<decltype(std::declval<IGame&>().GetMainWindowAddress()), ExpectedGameWindow*>::value,
    "IGame passes the address of its game window handle without a narrower proxy");
static_assert(std::is_same<decltype(std::declval<CGame&>().GetMainWindow()), ExpectedGameWindow>::value,
    "The actual implementation declaration uses the same handle type");
static_assert(sizeof(ExpectedGameWindow) == sizeof(void*), "Game windows retain native pointer width");
