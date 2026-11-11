#include "StandaloneRaster.h"
#include <algorithm>
#include <climits>
#include <cstring>
#include <cmath>

namespace cse::vgui {
namespace {
bool ImageSize(int width, int height, std::size_t& size) {
    if (width <= 0 || height <= 0 || width > MaximumRasterDimension || height > MaximumRasterDimension) {
        return false;
    }
    size = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * PixelChannels;
    return size <= static_cast<std::size_t>(INT_MAX);
}
} // namespace

bool RasterImage::Assign(const unsigned char* source, int wide, int tall, bool bgra, bool filter) {
    std::size_t size = 0;
    if (!source || !ImageSize(wide, tall, size)) {
        return false;
    }
    std::vector<unsigned char> replacement(source, source + size);
    if (bgra) {
        for (std::size_t offset = 0; offset < size; offset += PixelChannels) {
            std::swap(replacement[offset], replacement[offset + 2]);
        }
    }
    pixels.swap(replacement);
    filename.clear();
    width = wide;
    height = tall;
    filtered = filter;
    ++revision;
    return true;
}

bool RasterImage::Update(int x, int y, const unsigned char* source, int wide, int tall) {
    std::size_t size = 0;
    if (!source || !ImageSize(wide, tall, size) || x < 0 || y < 0 || x > width || y > height || wide > width - x ||
        tall > height - y) {
        return false;
    }
    for (int row = 0; row < tall; ++row) {
        for (int column = 0; column < wide; ++column) {
            const std::size_t from = (static_cast<std::size_t>(row) * wide + column) * PixelChannels;
            const std::size_t to = (static_cast<std::size_t>(y + row) * width + x + column) * PixelChannels;
            pixels[to] = source[from + 2];
            pixels[to + 1] = source[from + 1];
            pixels[to + 2] = source[from];
            pixels[to + 3] = source[from + 3];
        }
    }
    ++revision;
    return true;
}

StandaloneRaster::~StandaloneRaster() {
    Close();
}

bool StandaloneRaster::Open(SDL_Renderer* renderer, std::uint64_t generation) {
    Close();
    renderer_ = renderer;
    generation_ = generation;
    return renderer_ != nullptr;
}

void StandaloneRaster::Close() {
    if (renderer_) {
        SDL_SetRenderTarget(renderer_, nullptr);
    }
    for (auto& entry : textures_) {
        SDL_DestroyTexture(entry.second.texture);
    }
    textures_.clear();
    SDL_DestroyTexture(backing_);
    backing_ = nullptr;
    renderer_ = nullptr;
    width_ = 0;
    height_ = 0;
}

bool StandaloneRaster::Resize(int width, int height, float scale) {
    if (!renderer_ || !std::isfinite(scale) || scale <= 0.0f || width <= 0 || height <= 0 ||
        width > MaximumRasterDimension / scale || height > MaximumRasterDimension / scale) {
        return false;
    }
    if (backing_ && width == width_ && height == height_ && scale == scale_) {
        return SDL_SetRenderTarget(renderer_, backing_) && SDL_SetRenderScale(renderer_, scale, scale);
    }
    const int pixelsWide = static_cast<int>(std::ceil(width * scale));
    const int pixelsTall = static_cast<int>(std::ceil(height * scale));
    SDL_Texture* replacement =
        SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, pixelsWide, pixelsTall);
    if (!replacement) {
        return false;
    }
    SDL_SetRenderTarget(renderer_, nullptr);
    SDL_DestroyTexture(backing_);
    backing_ = replacement;
    width_ = width;
    height_ = height;
    scale_ = scale;
    SDL_SetTextureBlendMode(backing_, SDL_BLENDMODE_NONE);
    SDL_SetRenderTarget(renderer_, backing_);
    SDL_SetRenderScale(renderer_, scale, scale);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer_);
    return true;
}

SDL_Texture* StandaloneRaster::Texture(int id, const RasterImage& image) {
    if (!renderer_ || image.pixels.empty()) {
        return nullptr;
    }
    auto& entry = textures_[id];
    if (entry.texture && entry.revision == image.revision && entry.generation == generation_) {
        return entry.texture;
    }
    SDL_Texture* replacement =
        SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, image.width, image.height);
    if (!replacement) {
        return nullptr;
    }
    if (!SDL_UpdateTexture(replacement, nullptr, image.pixels.data(), image.width * PixelChannels)) {
        SDL_DestroyTexture(replacement);
        return nullptr;
    }
    SDL_SetTextureScaleMode(replacement, image.filtered ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(replacement, SDL_BLENDMODE_BLEND);
    SDL_DestroyTexture(entry.texture);
    entry.texture = replacement;
    entry.revision = image.revision;
    entry.generation = generation_;
    return entry.texture;
}

void StandaloneRaster::DeleteTexture(int id) {
    const auto found = textures_.find(id);
    if (found != textures_.end()) {
        SDL_DestroyTexture(found->second.texture);
        textures_.erase(found);
    }
}

void StandaloneRaster::Present() {
    if (!renderer_ || !backing_) {
        return;
    }
    SDL_SetRenderTarget(renderer_, nullptr);
    SDL_SetRenderScale(renderer_, 1.0f, 1.0f);
    SDL_SetRenderClipRect(renderer_, nullptr);
    SDL_RenderTexture(renderer_, backing_, nullptr, nullptr);
    SDL_RenderPresent(renderer_);
    SDL_SetRenderTarget(renderer_, backing_);
    SDL_SetRenderScale(renderer_, scale_, scale_);
}
} // namespace cse::vgui
