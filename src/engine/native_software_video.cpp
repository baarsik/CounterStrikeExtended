#include "native_software_video.h"
#include <new>

namespace {
constexpr int kMaximumSoftwareWidth = 5120;
constexpr int kMaximumSoftwareHeight = 1600; // d_local.h MAXHEIGHT scan tables.
constexpr int kPresentationChannels = 4;
constexpr const char *kGenerationProperty = "cse.software.renderer.generation";
Sint64 RendererGeneration(SDL_Renderer *renderer) {
    if (!renderer) {
        return 0;
    }
    const SDL_PropertiesID properties = SDL_GetRendererProperties(renderer);
    Sint64 generation = SDL_GetNumberProperty(properties, kGenerationProperty, 0);
    if (!generation) {
        static Sint64 nextGeneration = 0;
        generation = ++nextGeneration;
        if (!SDL_SetNumberProperty(properties, kGenerationProperty, generation)) {
            return 0;
        }
    }
    return generation;
}
} // namespace

bool CNativeSoftwareFramebuffer::Allocate(int width, int height, bool rgb555) {
    if (width <= 0 || height <= 0 || width > kMaximumSoftwareWidth || height > kMaximumSoftwareHeight || m_texture) {
        return false;
    }
    const std::size_t pixels = static_cast<std::size_t>(width) * height;
    try {
        m_pixels.assign(pixels, 0);
        m_rgba.resize(pixels * kPresentationChannels);
    } catch (const std::bad_alloc &) {
        m_pixels.clear();
        m_rgba.clear();
        return false;
    }
    m_width = width;
    m_height = height;
    m_rgb555 = rgb555;
    return true;
}

void CNativeSoftwareFramebuffer::Release(SDL_Renderer *liveRenderer) {
    if (m_texture && liveRenderer == m_renderer && RendererGeneration(liveRenderer) == m_generation) {
        SDL_DestroyTexture(m_texture);
    }
    // SDL already destroys child textures when the old renderer is retired.
    m_texture = nullptr;
    m_renderer = nullptr;
    m_generation = 0;
}

bool CNativeSoftwareFramebuffer::Present(SDL_Renderer *renderer) {
    const Sint64 generation = RendererGeneration(renderer);
    if (!generation || m_pixels.empty()) {
        return false;
    }
    if (renderer != m_renderer || generation != m_generation) {
        Release(renderer);
        m_renderer = renderer;
        m_generation = generation;
    }
    if (!m_texture) {
        m_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, m_width, m_height);
        if (!m_texture) {
            return false;
        }
        if (!SDL_SetTextureScaleMode(m_texture, SDL_SCALEMODE_NEAREST) ||
            !SDL_SetTextureBlendMode(m_texture, SDL_BLENDMODE_NONE)) {
            Release(renderer);
            return false;
        }
    }
    // Preserve the retained snapshot/rasterizer's channel quantization: low bits
    // remain zero rather than expanding 5/6-bit maxima to 255.
    for (std::size_t index = 0; index < m_pixels.size(); ++index) {
        const std::uint16_t color = m_pixels[index];
        unsigned char *pixel = m_rgba.data() + index * kPresentationChannels;
        pixel[0] = static_cast<unsigned char>(color >> (m_rgb555 ? 7 : 8)) & 0xf8;
        pixel[1] = static_cast<unsigned char>(color >> (m_rgb555 ? 2 : 3)) & (m_rgb555 ? 0xf8 : 0xfc);
        pixel[2] = static_cast<unsigned char>(color << 3);
        pixel[3] = 255;
    }
    return SDL_UpdateTexture(m_texture, nullptr, m_rgba.data(), m_width * kPresentationChannels) &&
           SDL_RenderTexture(renderer, m_texture, nullptr, nullptr) && SDL_RenderPresent(renderer);
}
