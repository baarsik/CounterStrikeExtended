#pragma once
#include <SDL3/SDL.h>
#include <cstdint>
#include <vector>

// CPU storage survives presentation loss. The caller retires the texture before
// destroying its borrowed renderer; Present also recognizes SDL renderer reuse.
class CNativeSoftwareFramebuffer {
public:
    bool Allocate(int width, int height, bool rgb555);
    bool Present(SDL_Renderer *renderer);
    void Release(SDL_Renderer *liveRenderer);
    unsigned char *Pixels() {
        return reinterpret_cast<unsigned char *>(m_pixels.data());
    }
    int Pitch() const {
        return m_width * static_cast<int>(sizeof(std::uint16_t));
    }

private:
    std::vector<std::uint16_t> m_pixels;
    std::vector<unsigned char> m_rgba;
    SDL_Texture *m_texture = nullptr;
    SDL_Renderer *m_renderer = nullptr;
    Sint64 m_generation = 0;
    int m_width = 0;
    int m_height = 0;
    bool m_rgb555 = false;
};
