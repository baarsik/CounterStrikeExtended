#include "../../../src/vgui2/src/native/StandaloneWindow.h"
#include <climits>
#include <cstdio>
#include <cstdlib>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s: %s\n", message, SDL_GetError());
        std::exit(1);
    }
}
} // namespace

int main() {
    Require(SDL_InitSubSystem(SDL_INIT_VIDEO), "initialize actual SDL dummy video");
    constexpr int Cycles = 100;
    const unsigned char pixels[] = {255, 0, 0, 255, 0, 255, 0, 255};
    for (int cycle = 0; cycle < Cycles; ++cycle) {
        cse::vgui::StandaloneWindow window;
        Require(!window.Open(1, cycle + 1, 0, 0, INT_MAX, 32, false), "reject unrepresentable dimensions");
        Require(window.Open(1, cycle + 1, 0, 0, 32, 32, false), "open after rejected partial creation");
        cse::vgui::RasterImage image;
        Require(!image.Assign(pixels, INT_MAX, INT_MAX, false, false), "reject multiplication/pitch overflow");
        Require(image.Assign(pixels, 2, 1, false, false), "valid texture");
        Require(!image.Update(INT_MAX, 0, pixels, 1, 1), "reject overflowing region");
        Require(!image.Update(1, 0, pixels, 2, 1), "reject region exceeding texture");
        Require(window.raster.Texture(1, image) != nullptr, "renderer-generation texture upload");
        Require(window.raster.Resize(32, 32, 2.0f), "actual backing scale two");
        SDL_Rect clip{4, 4, 4, 4};
        auto* renderer = window.raster.Renderer();
        SDL_SetRenderClipRect(renderer, &clip);
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_FRect rectangle{0, 0, 32, 32};
        SDL_RenderFillRect(renderer, &rectangle);
        SDL_Surface* actual = SDL_RenderReadPixels(renderer, nullptr);
        Require(actual && actual->w == 64 && actual->h == 64, "backing dimension follows density");
        Uint8 r = 0;
        Uint8 g = 0;
        Uint8 b = 0;
        Uint8 a = 0;
        SDL_ReadSurfacePixel(actual, 7, 10, &r, &g, &b, &a);
        Require(r == 0, "scaled clip outside left edge");
        SDL_ReadSurfacePixel(actual, 8, 10, &r, &g, &b, &a);
        Require(r == 255 && g == 0 && b == 0, "scaled clip includes left edge");
        SDL_ReadSurfacePixel(actual, 16, 10, &r, &g, &b, &a);
        Require(r == 0, "scaled clip excludes right edge");
        SDL_DestroySurface(actual);
        window.Close();
        Require(window.Open(2, cycle + 2, 0, 0, 32, 32, false), "reopen after texture retirement");
        Require(window.raster.Texture(1, image) != nullptr, "fresh renderer generation can reupload texture");
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    std::puts("PASS 100 actual renderer/window/texture generations and scaled clip pixels");
}
