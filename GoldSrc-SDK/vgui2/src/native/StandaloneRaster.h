#pragma once

#include <SDL3/SDL.h>
#include <cstdint>
#include <map>
#include <vector>
#include <string>

namespace cse::vgui {
constexpr int PixelChannels = 4;
constexpr int MaximumRasterDimension = 16384;

struct RasterImage {
    int width = 0;
    int height = 0;
    bool filtered = false;
    std::string filename;
    std::uint64_t revision = 0;
    std::vector<unsigned char> pixels;
    bool Assign(const unsigned char* source, int wide, int tall, bool bgra, bool filter);
    // Update accepts compact BGRA regions; HTML callbacks instead supply the complete backing image.
    bool Update(int x, int y, const unsigned char* source, int wide, int tall);
    bool UpdateFromBacking(int x, int y, const unsigned char* source, int wide, int tall);

private:
    bool UpdateRegion(int x, int y, const unsigned char* source, int wide, int tall, bool fullBacking);
};

class StandaloneRaster {
public:
    StandaloneRaster() = default;
    StandaloneRaster(const StandaloneRaster&) = delete;
    StandaloneRaster& operator=(const StandaloneRaster&) = delete;
    ~StandaloneRaster();
    bool Open(SDL_Renderer* renderer, std::uint64_t generation);
    void Close();
    bool Resize(int width, int height, float scale);
    SDL_Texture* Texture(int id, const RasterImage& image);
    void DeleteTexture(int id);
    void Present();
    SDL_Renderer* Renderer() const {
        return renderer_;
    }

private:
    struct Entry {
        SDL_Texture* texture = nullptr;
        std::uint64_t revision = 0;
        std::uint64_t generation = 0;
    };
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* backing_ = nullptr;
    std::uint64_t generation_ = 0;
    int width_ = 0;
    int height_ = 0;
    float scale_ = 1.0f;
    std::map<int, Entry> textures_;
};
} // namespace cse::vgui
