#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
// Interpose external OS services while leaving the complete engine image intact.
static void Forbidden(const char *operation) {
    std::fprintf(stderr, "FORBIDDEN dedicated video call: %s\n", operation);
    std::abort();
}
static bool CheckedInit(SDL_InitFlags flags) {
    if (flags & SDL_INIT_VIDEO) {
        Forbidden("SDL_Init");
    }
    return SDL_Init(flags);
}
static bool CheckedInitSubsystem(SDL_InitFlags flags) {
    if (flags & SDL_INIT_VIDEO) {
        Forbidden("SDL_InitSubSystem");
    }
    return SDL_InitSubSystem(flags);
}
static SDL_Cursor *CheckedCursor(SDL_SystemCursor) {
    Forbidden("SDL_CreateSystemCursor");
    return nullptr;
}
static SDL_Window *CheckedWindow(const char *, int, int, SDL_WindowFlags) {
    Forbidden("SDL_CreateWindow");
    return nullptr;
}
static SDL_GLContext CheckedGL(SDL_Window *) {
    Forbidden("SDL_GL_CreateContext");
    return nullptr;
}
static bool CheckedGLAttribute(SDL_GLAttr, int) {
    Forbidden("SDL_GL_SetAttribute");
    return false;
}
#define INTERPOSE(replacement, original)                                                                               \
    __attribute__((used)) static const struct {                                                                        \
        const void *replacement;                                                                                       \
        const void *original;                                                                                          \
    } pair_##original __attribute__((section("__DATA,__interpose"))) = {reinterpret_cast<const void *>(&replacement),  \
                                                                        reinterpret_cast<const void *>(&original)}
INTERPOSE(CheckedInit, SDL_Init);
INTERPOSE(CheckedInitSubsystem, SDL_InitSubSystem);
INTERPOSE(CheckedCursor, SDL_CreateSystemCursor);
INTERPOSE(CheckedWindow, SDL_CreateWindow);
INTERPOSE(CheckedGL, SDL_GL_CreateContext);
INTERPOSE(CheckedGLAttribute, SDL_GL_SetAttribute);
