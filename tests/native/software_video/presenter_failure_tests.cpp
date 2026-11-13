#include <SDL3/SDL.h>
#include <cassert>
static bool failTexture;
static bool failScale;
static int textures;
static SDL_Texture *ControlledCreate(SDL_Renderer *renderer, SDL_PixelFormat format, SDL_TextureAccess access,
                                     int width, int height) {
    if (failTexture) {
        return nullptr;
    }
    SDL_Texture *texture = SDL_CreateTexture(renderer, format, access, width, height);
    if (texture) {
        ++textures;
    }
    return texture;
}
static void ControlledDestroy(SDL_Texture *texture) {
    assert(textures > 0);
    --textures;
    SDL_DestroyTexture(texture);
}
static bool ControlledScale(SDL_Texture *texture, SDL_ScaleMode scale) {
    return !failScale && SDL_SetTextureScaleMode(texture, scale);
}
#define SDL_CreateTexture ControlledCreate
#define SDL_DestroyTexture ControlledDestroy
#define SDL_SetTextureScaleMode ControlledScale
#include "../../../src/engine/native_software_video.cpp"
#undef SDL_CreateTexture
#undef SDL_DestroyTexture
#undef SDL_SetTextureScaleMode
int main() {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    assert(SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *window = SDL_CreateWindow("failure", 3, 2, SDL_WINDOW_HIDDEN);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, "software");
    assert(window && renderer);
    CNativeSoftwareFramebuffer frame;
    assert(frame.Allocate(3, 2, false));
    assert(!frame.Present(nullptr));
    failTexture = true;
    assert(!frame.Present(renderer) && textures == 0);
    frame.Release(renderer);
    frame.Release(renderer);
    failTexture = false;
    failScale = true;
    assert(!frame.Present(renderer) && textures == 0);
    failScale = false;
    assert(frame.Present(renderer) && textures == 1);
    frame.Release(renderer);
    assert(textures == 0);
    assert(frame.Present(renderer));
    frame.Release(renderer);
    assert(textures == 0);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
