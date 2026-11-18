#include "image_codec.h"

#include <cctype>
#include <cstring>
#include <new>
#include <webp/decode.h>
#include <webp/encode.h>

namespace cse {
namespace image {
namespace {

constexpr std::size_t RiffHeaderBytes = 12;
constexpr std::size_t RiffSizeOffset = 4;
constexpr std::size_t RiffSizeBytes = 8;
constexpr unsigned int BitsPerByte = 8;

bool PixelBytes(int width, int height, int channels, std::size_t& bytes) {
    if (width <= 0 || height <= 0 || width > WEBP_MAX_DIMENSION || height > WEBP_MAX_DIMENSION) {
        return false;
    }
    const auto rowBytes = static_cast<std::size_t>(width) * channels;
    if (static_cast<std::size_t>(height) > MaximumDecodedBytes / rowBytes) {
        return false;
    }
    bytes = rowBytes * height;
    return true;
}

}

bool IsWebP(const void* data, std::size_t size) {
    if (!data || size < RiffHeaderBytes) {
        return false;
    }
    const auto* bytes = static_cast<const unsigned char*>(data);
    return std::memcmp(bytes, "RIFF", RiffSizeOffset) == 0
        && std::memcmp(bytes + RiffSizeBytes, "WEBP", RiffSizeOffset) == 0;
}

bool HasWebPExtension(const char* path) {
    if (!path) {
        return false;
    }
    const std::size_t length = std::strlen(path);
    constexpr char Extension[] = ".webp";
    constexpr std::size_t ExtensionLength = sizeof(Extension) - 1;
    if (length < ExtensionLength) {
        return false;
    }
    for (std::size_t index = 0; index < ExtensionLength; ++index) {
        const auto character = static_cast<unsigned char>(path[length - ExtensionLength + index]);
        if (std::tolower(character) != Extension[index]) {
            return false;
        }
    }
    return true;
}

std::string WebPPath(const char* path) {
    if (!path || !*path) {
        return {};
    }
    if (HasWebPExtension(path)) {
        return path;
    }
    std::string result(path);
    const auto slash = result.find_last_of("/\\");
    const auto dot = result.find_last_of('.');
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
        result.resize(dot);
    }
    result += ".webp";
    return result;
}

bool DecodeWebP(const void* data, std::size_t size, Image& image, std::size_t capacity) {
    return DecodeWebP(data, size, image, capacity, WEBP_MAX_DIMENSION);
}

bool DecodeWebP(const void* data, std::size_t size, Image& image, std::size_t capacity, int maximumSide) {
    image = Image{};
    if (!IsWebP(data, size) || size > MaximumEncodedBytes) {
        return false;
    }
    const auto* bytes = static_cast<const unsigned char*>(data);
    std::size_t declaredSize = 0;
    constexpr std::size_t SizeFieldBytes = 4;
    for (std::size_t index = 0; index < SizeFieldBytes; ++index) {
        declaredSize |= static_cast<std::size_t>(bytes[RiffSizeOffset + index]) << (index * BitsPerByte);
    }
    if (declaredSize < RiffSizeOffset || declaredSize > size - RiffSizeBytes) {
        return false;
    }
    WebPBitstreamFeatures features{};
    if (WebPGetFeatures(bytes, size, &features) != VP8_STATUS_OK || features.has_animation ||
        features.width > maximumSide || features.height > maximumSide) {
        return false;
    }
    std::size_t decodedBytes = 0;
    if (!PixelBytes(features.width, features.height, RgbaChannels, decodedBytes) || decodedBytes > capacity) {
        return false;
    }
    try {
        std::vector<unsigned char> decoded(decodedBytes);
        const int stride = features.width * RgbaChannels;
        if (!WebPDecodeRGBAInto(bytes, size, decoded.data(), decoded.size(), stride)) {
            return false;
        }
        image.width = features.width;
        image.height = features.height;
        image.rgba.swap(decoded);
        return true;
    } catch (const std::bad_alloc&) {
        return false;
    }
}

bool EncodeWebPLossless(const unsigned char* pixels, int width, int height, int stride,
                        int channels, bool bottomUp, std::vector<unsigned char>& encoded) {
    encoded.clear();
    if (!pixels || (channels != RgbChannels && channels != RgbaChannels)) {
        return false;
    }
    std::size_t inputBytes = 0;
    if (!PixelBytes(width, height, channels, inputBytes) || stride < width * channels
        || static_cast<std::size_t>(stride) > MaximumDecodedBytes / static_cast<std::size_t>(height)) {
        return false;
    }

    WebPConfig config;
    if (!WebPConfigInit(&config)) {
        return false;
    }
    config.lossless = 1;
    // Keep RGB under fully transparent pixels, too, for faithful asset round trips.
    config.exact = 1;
    constexpr int ScreenshotCompressionMethod = 3;
    config.method = ScreenshotCompressionMethod;
    WebPPicture picture;
    if (!WebPPictureInit(&picture)) {
        return false;
    }
    picture.use_argb = 1;
    picture.width = width;
    picture.height = height;
    const unsigned char* firstRow = pixels;
    int rowStride = stride;
    if (bottomUp) {
        firstRow += static_cast<std::size_t>(height - 1) * stride;
        rowStride = -stride;
    }
    const bool imported = channels == RgbaChannels
        ? WebPPictureImportRGBA(&picture, firstRow, rowStride) != 0
        : WebPPictureImportRGB(&picture, firstRow, rowStride) != 0;
    WebPMemoryWriter writer;
    WebPMemoryWriterInit(&writer);
    picture.writer = WebPMemoryWrite;
    picture.custom_ptr = &writer;
    bool success = imported && WebPEncode(&config, &picture) != 0 && writer.size <= MaximumEncodedBytes;
    if (success) {
        try {
            encoded.assign(writer.mem, writer.mem + writer.size);
        } catch (const std::bad_alloc&) {
            success = false;
        }
    }
    WebPMemoryWriterClear(&writer);
    WebPPictureFree(&picture);
    return success;
}

}
}
