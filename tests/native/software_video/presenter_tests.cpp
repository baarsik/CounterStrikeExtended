#include "../../../GoldSrc-SDK/engine/native_software_video.h"
#include <SDL3/SDL.h>
#include <cassert>
#include <cstring>
int main() {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    assert(SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *window = SDL_CreateWindow("test", 3, 2, SDL_WINDOW_HIDDEN);
    assert(window);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, "software");
    assert(renderer);
    CNativeSoftwareFramebuffer frame;
    assert(!frame.Allocate(0, 2, false));
    assert(!frame.Allocate(-1, 2, false));
    assert(!frame.Allocate(5121, 2, false));
    assert(!frame.Allocate(2, 1601, false));
    assert(frame.Allocate(3, 2, false));
    assert(!frame.Present(nullptr));
    const unsigned short pixels[] = {0xf800, 0x07e0, 0x001f, 0xffff, 0, 0x8410};
    std::memcpy(frame.Pixels(), pixels, sizeof(pixels));
    assert(frame.Present(renderer));
    SDL_Surface *surface = SDL_RenderReadPixels(renderer, nullptr);
    assert(surface);
    SDL_Surface *rgba = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
    assert(rgba);
    const unsigned char expected[] = {248, 0, 0, 255, 0, 252, 0, 255, 0, 0, 248, 255};
    assert(std::memcmp(rgba->pixels, expected, sizeof(expected)) == 0);
    SDL_DestroySurface(rgba);
    SDL_DestroySurface(surface);
    frame.Release(renderer);
    frame.Release(renderer);
    assert(frame.Present(renderer));
    // SDL destroys its child textures. A later generation must never destroy the stale handle.
    SDL_DestroyRenderer(renderer);
    renderer = SDL_CreateRenderer(window, "software");
    assert(renderer && frame.Present(renderer));
    frame.Release(renderer);
    assert(frame.Allocate(3, 2, true));
    const unsigned short rgb555[] = {0x7c00, 0x03e0, 0x001f, 0x7fff, 0, 0x4210};
    std::memcpy(frame.Pixels(), rgb555, sizeof(rgb555));
    assert(frame.Present(renderer));
    surface = SDL_RenderReadPixels(renderer, nullptr);
    rgba = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
    const unsigned char expected555[] = {248, 0, 0, 255, 0, 248, 0, 255, 0, 0, 248, 255};
    assert(std::memcmp(rgba->pixels, expected555, sizeof(expected555)) == 0);
    SDL_DestroySurface(rgba);
    SDL_DestroySurface(surface);
    frame.Release(renderer);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
