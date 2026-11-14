#ifndef CSE_COMMON_IMAGE_CODEC_H
#define CSE_COMMON_IMAGE_CODEC_H

#include <cstddef>
#include <string>
#include <vector>

namespace cse {
namespace image {

// Bound allocations before invoking the decoder, including for downloaded images.
constexpr std::size_t MaximumEncodedBytes = 64 * 1024 * 1024;
constexpr std::size_t MaximumDecodedBytes = 256 * 1024 * 1024;
constexpr int RgbaChannels = 4;
constexpr int RgbChannels = 3;

struct Image {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> rgba;
};

bool IsWebP(const void* data, std::size_t size);
bool HasWebPExtension(const char* path);
std::string WebPPath(const char* path);
bool DecodeWebP(const void* data, std::size_t size, Image& image,
                std::size_t capacity = MaximumDecodedBytes);
// Pixels are RGB or RGBA; stride includes row padding. Output always has top-down orientation.
bool EncodeWebPLossless(const unsigned char* pixels, int width, int height, int stride,
                        int channels, bool bottomUp, std::vector<unsigned char>& encoded);

}
}

#endif
