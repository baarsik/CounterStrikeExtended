// Reference is the retained font provider's raw glyph pixels, not a second surface renderer.
#include <SDL3/SDL.h>
#include "../../../GoldSrc-SDK/vgui2/vgui_surfacelib/FontManager.h"
#include "../../../GoldSrc-SDK/vgui2/vgui_surfacelib/LinuxFont.h"
#include <algorithm>
#include <cstdio>
#include <vector>

extern "C" __attribute__((visibility("default"))) bool
CseTestCompareGlyph(SDL_Surface* actual, vgui2::HFont fontHandle, wchar_t character, int originX, int originY) {
    auto* font = FontManager().GetFontForChar(fontHandle, character);
    if (!actual || !font) {
        return false;
    }
    int a = 0;
    int b = 0;
    int c = 0;
    font->GetCharABCWidths(character, a, b, c);
    const int width = std::max(1, font->GetUnderlined() ? a + b + c : b);
    const int height = font->GetHeight();
    constexpr int PixelChannels = 4;
    std::vector<unsigned char> expected(static_cast<std::size_t>(width) * height * PixelChannels);
    font->GetCharRGBA(character, 0, 0, width, height, expected.data());
    // SDL renderer composition and SDL_BlitSurface have different byte rounding
    // in the pinned provider. Compare the surface adapter against the real retained
    // glyph raster submitted directly to the same renderer API, without its cache.
    SDL_Surface* reference = SDL_CreateSurface(width, height, actual->format);
    SDL_Renderer* renderer = reference ? SDL_CreateSoftwareRenderer(reference) : nullptr;
    SDL_Texture* texture =
        renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height)
                 : nullptr;
    if (!texture) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroySurface(reference);
        return false;
    }
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_UpdateTexture(texture, nullptr, expected.data(), width * PixelChannels);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_RenderTexture(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
    int inkPixels = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto alpha = expected[(static_cast<std::size_t>(y) * width + x) * PixelChannels + 3];
            Uint8 red = 0;
            Uint8 green = 0;
            Uint8 blue = 0;
            Uint8 outputAlpha = 0;
            SDL_ReadSurfacePixel(actual, originX + a + x, originY + y, &red, &green, &blue, &outputAlpha);
            Uint8 expectedRed = 0;
            Uint8 expectedGreen = 0;
            Uint8 expectedBlue = 0;
            Uint8 expectedAlpha = 0;
            SDL_ReadSurfacePixel(reference, x, y, &expectedRed, &expectedGreen, &expectedBlue, &expectedAlpha);
            if (red != expectedRed || green != expectedGreen || blue != expectedBlue) {
                std::fprintf(stderr, "Glyph mismatch (%d,%d): raw coverage %u, surface RGB %u,%u,%u\n", x, y, alpha,
                             red, green, blue);
                std::fprintf(stderr, "format=%s reference=%u,%u,%u\n", SDL_GetPixelFormatName(actual->format),
                             expectedRed, expectedGreen, expectedBlue);
                SDL_DestroyTexture(texture);
                SDL_DestroyRenderer(renderer);
                SDL_DestroySurface(reference);
                return false;
            }
            if (alpha) {
                ++inkPixels;
            }
        }
    }
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(reference);
    return inkPixels > 0;
}
