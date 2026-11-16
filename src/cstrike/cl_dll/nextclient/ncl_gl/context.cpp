#include <ncl_gl/context.h>
#include <SDL3/SDL.h>

void* ncl_gl::CurrentContext() {
    return SDL_GL_GetCurrentContext();
}
