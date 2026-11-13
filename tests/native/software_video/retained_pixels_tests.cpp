#include "../../../src/engine/vid.h"
#include "../../../src/engine/native_software_video.h"
#include <SDL3/SDL.h>
#include <dlfcn.h>
#include <cassert>
#include <cstring>
#include <cstdio>
int main(int argc, char **argv) {
    assert(argc == 2);
    void *module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!module) {
        std::fprintf(stderr, "%s\n", dlerror());
        return 1;
    }
    auto *engineVid = static_cast<viddef_t *>(dlsym(module, "vid"));
    auto *rgb555 = static_cast<int *>(dlsym(module, "is15bit"));
    auto draw = reinterpret_cast<void (*)(int, int, int, int, int, int, int)>(dlsym(module, "_Z12Draw_FillRGBiiiiiii"));
    auto cache = reinterpret_cast<int (*)(int, int)>(dlsym(module, "_Z23Sys_GetSurfaceCacheSizeii"));
    auto initArgs = reinterpret_cast<void (*)(char *)>(dlsym(module, "_Z12Sys_InitArgvPc"));
    assert(engineVid && rgb555 && draw && cache && initArgs);
    char empty[] = "";
    initArgs(empty);
    assert(cache(320, 100) == 3145728);
    assert(cache(5120, 1600) == 52105728);
    char invalid[] = "-surfcachesize 2147483647";
    initArgs(invalid);
    assert(cache(320, 200) == 0);
    char missing[] = "-surfcachesize";
    initArgs(missing);
    assert(cache(320, 200) == 0);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    assert(SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *window = SDL_CreateWindow("retained", 3, 2, SDL_WINDOW_HIDDEN);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, "software");
    assert(window && renderer);
    for (int format : {0, 1}) {
        CNativeSoftwareFramebuffer frame;
        assert(frame.Allocate(3, 2, format != 0));
        engineVid->width = 3;
        engineVid->height = 2;
        engineVid->rowbytes = frame.Pitch();
        engineVid->buffer = frame.Pixels();
        *rgb555 = format;
        draw(0, 0, 1, 2, 255, 0, 0);
        draw(1, 0, 1, 2, 0, 255, 0);
        draw(2, 0, 1, 2, 0, 0, 255);
        const auto *pixels = reinterpret_cast<const unsigned short *>(frame.Pixels());
        assert(pixels[0] == (format ? 0x7c00 : 0xf800));
        assert(pixels[1] == (format ? 0x03e0 : 0x07e0));
        assert(pixels[2] == 0x001f && pixels[3] == pixels[0]);
        assert(frame.Present(renderer));
        SDL_Surface *surface = SDL_RenderReadPixels(renderer, nullptr);
        SDL_Surface *rgba = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
        const unsigned char corners565[] = {248, 0, 0, 255, 0, 252, 0, 255, 0, 0, 248, 255};
        const unsigned char corners555[] = {248, 0, 0, 255, 0, 248, 0, 255, 0, 0, 248, 255};
        assert(std::memcmp(rgba->pixels, format ? corners555 : corners565, sizeof(corners565)) == 0);
        SDL_DestroySurface(rgba);
        SDL_DestroySurface(surface);
        frame.Release(renderer);
    }
    engineVid->buffer = nullptr;
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    dlclose(module);
    std::puts("PASS software_pixels_and_palette (actual swds Draw_FillRGB -> SDL readback)");
}
