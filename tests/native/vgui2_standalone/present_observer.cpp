#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
constexpr int PixelChannels = 4;
SDL_Renderer* observed = nullptr;
int width = 0;
int height = 0;
std::vector<std::vector<unsigned char>> frames;

bool Present(SDL_Renderer* renderer) {
    if (renderer == observed) {
        // Observe the actual completed window target, immediately before SDL publishes it.
        SDL_Surface* pixels = SDL_RenderReadPixels(renderer, nullptr);
        SDL_Surface* rgba = pixels ? SDL_ConvertSurface(pixels, SDL_PIXELFORMAT_RGBA32) : nullptr;
        if (!rgba) {
            std::fprintf(stderr, "FAIL: capture actual presentation: %s\n", SDL_GetError());
            std::abort();
        }
        width = rgba->w;
        height = rgba->h;
        std::vector<unsigned char> frame(static_cast<std::size_t>(width) * height * PixelChannels);
        for (int row = 0; row < height; ++row) {
            std::memcpy(frame.data() + static_cast<std::size_t>(row) * width * PixelChannels,
                        static_cast<unsigned char*>(rgba->pixels) + row * rgba->pitch, width * PixelChannels);
        }
        frames.push_back(std::move(frame));
        SDL_DestroySurface(rgba);
        SDL_DestroySurface(pixels);
    }
    return SDL_RenderPresent(renderer);
}
} // namespace

extern "C" __attribute__((visibility("default"))) void CseTestObservePresent(SDL_Renderer* renderer) {
    observed = renderer;
    frames.clear();
}
extern "C" __attribute__((visibility("default"))) int CseTestPresentCount() {
    return static_cast<int>(frames.size());
}
extern "C" __attribute__((visibility("default"))) const unsigned char* CseTestPresentPixels(
    int index, int* wide, int* tall) {
    *wide = width;
    *tall = height;
    return frames.at(index).data();
}
__attribute__((used, section("__DATA,__interpose"))) static const struct {
    const void* replacement;
    const void* original;
} interposePresent = {reinterpret_cast<const void*>(&Present), reinterpret_cast<const void*>(&SDL_RenderPresent)};
