// Link-only SDL seams for complete owning-TU fixtures. The private typed tests
// replace these expensive/native lookups; accidentally reaching them is fatal.
#include <SDL3/SDL.h>
#include <cstdlib>
extern "C" SDL_Window **SDL_GetWindows(int *) {
    std::abort();
}

extern "C" SDL_WindowID SDL_GetWindowID(SDL_Window *) {
    std::abort();
}

extern "C" SDL_Window *SDL_GetWindowFromID(SDL_WindowID) {
    std::abort();
}

extern "C" SDL_PropertiesID SDL_GetWindowProperties(SDL_Window *) {
    std::abort();
}

extern "C" void *SDL_GetPointerProperty(SDL_PropertiesID, const char *, void *) {
    std::abort();
}

extern "C" void SDL_free(void *) {
    std::abort();
}
