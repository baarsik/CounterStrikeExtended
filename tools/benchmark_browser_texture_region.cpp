#include "legacy_texture.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>

namespace {
constexpr int kViewWidth = 728;
constexpr int kViewHeight = 432;
constexpr int kDamageX = 8;
constexpr int kDamageY = 55;
constexpr int kDamageWidth = 500;
constexpr int kDamageHeight = 220;
constexpr std::size_t kPixelBytes = 4;
constexpr std::size_t kIterations = 1000;
constexpr std::uint32_t kTextureName = 17;

void Require(bool valid, const char* message) {
    if (!valid) {
        std::fprintf(stderr, "%s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

std::vector<std::uint8_t> Run(bool narrow, std::size_t trial) {
    using namespace cse::render;
    LegacyState state;
    TextureStore store;
    Require(store.Bind(state, TextureTarget::Texture2D, kTextureName) == Status::Ok, "bind texture");
    std::vector<std::uint8_t> source(static_cast<std::size_t>(kViewWidth) * kViewHeight * kPixelBytes);
    TextureUpload image;
    image.width = kViewWidth;
    image.height = kViewHeight;
    image.format = PixelFormat::BGRA;
    PixelStoreState unpack;
    unpack.unpackRowLength = kViewWidth;
    Require(store.Upload(state, TextureTarget::Texture2D, image, unpack, false, source.data(), source.size()) ==
        Status::Ok, "initialize image");
    TextureUpload update;
    update.x = narrow ? kDamageX : 0;
    update.y = kDamageY;
    update.width = narrow ? kDamageWidth : kViewWidth;
    update.height = kDamageHeight;
    update.format = PixelFormat::BGRA;
    const auto started = std::chrono::steady_clock::now();
    for (std::size_t iteration = 0; iteration < kIterations; ++iteration) {
        const auto row = kDamageY + static_cast<int>(iteration % kDamageHeight);
        const auto changed = (static_cast<std::size_t>(row) * kViewWidth + kDamageX) * kPixelBytes;
        source[changed] = static_cast<std::uint8_t>(iteration);
        const auto first = (static_cast<std::size_t>(update.y) * kViewWidth + update.x) * kPixelBytes;
        Require(store.Upload(state, TextureTarget::Texture2D, update, unpack, true,
            source.data() + first, source.size() - first) == Status::Ok, "update image");
    }
    const auto milliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count() / kIterations;
    const auto imageBytes = source.size();
    const auto updatePixels = static_cast<std::size_t>(update.width) * update.height;
    std::printf("%zu,%s,%zu,%zu,%.6f\n", trial, narrow ? "dirty_rectangle" : "full_rows", imageBytes,
        updatePixels * kPixelBytes, milliseconds);
    const auto texture = store.Find(kTextureName);
    Require(texture && texture->levels.count(0), "retained image");
    return texture->levels.at(0)->pixels;
}
}

int main() {
    std::puts("trial,mode,image_bytes,update_pixels_bytes,milliseconds_per_update");
    constexpr std::array<bool, 8> kOrder{{false, true, true, false, false, true, true, false}};
    std::vector<std::uint8_t> reference;
    for (std::size_t index = 0; index < kOrder.size(); ++index) {
        auto image = Run(kOrder[index], index / 2 + 1);
        if (reference.empty()) {
            reference = std::move(image);
        } else {
            Require(reference == image, "full rows and dirty region differ");
        }
    }
}
