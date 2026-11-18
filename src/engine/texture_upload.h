#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

// Checked pixel storage and sampling, informed by CS16Reconstruction e5c25197f, engine/src/textures.cpp.
// Keep these private to the hardware renderer; no engine or GL interfaces depend on them.
namespace texture_upload {

constexpr std::size_t kRgbaChannels = 4;
constexpr std::size_t kLuminanceChannels = 1;
// Preserve the old admission limits, while allocating actual pixel storage rather than counting bytes as pixels.
constexpr std::size_t kMaxIndexedPixels = 640 * 480 * sizeof(std::uint32_t);
constexpr std::size_t kMaxScaledPixels = 1024 * 512;
constexpr unsigned int kFractionBits = 16;
constexpr std::uint64_t kSampleDivisor = 4;

inline bool ByteCount(int width, int height, std::size_t channels, std::size_t& bytes) {
    bytes = 0;
    if (width <= 0 || height <= 0 || (channels != kRgbaChannels && channels != kLuminanceChannels)) {
        return false;
    }

    // The legacy copy and neighbor-filter interfaces use signed int byte offsets.
    constexpr std::size_t kMaxBytes = (std::numeric_limits<int>::max)();
    if (static_cast<std::size_t>(width) > kMaxBytes / channels / static_cast<std::size_t>(height)) {
        return false;
    }
    bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * channels;
    return true;
}

inline bool ScaledSize(int width, int height, float maxSize, float roundDown, float picmip,
    int& scaledWidth, int& scaledHeight) {
    scaledWidth = 0;
    scaledHeight = 0;
    std::size_t bytes;
    if (!ByteCount(width, height, kLuminanceChannels, bytes)) {
        return false;
    }
    for (float value : {maxSize, roundDown, picmip}) {
        if (!std::isfinite(value) || static_cast<double>(value) > (std::numeric_limits<int>::max)() ||
            static_cast<double>(value) < (std::numeric_limits<int>::min)()) {
            return false;
        }
    }

    constexpr int kMinimumMaxSize = 128;
    constexpr int kShiftLimit = 32;
    const int maximum = (std::max)(static_cast<int>(maxSize), kMinimumMaxSize);
    const int rounding = static_cast<int>(roundDown);
    const int mip = static_cast<int>(picmip);
    const auto scale = [=](int dimension) {
        std::uint64_t result = 1;
        while (result < static_cast<std::uint64_t>(dimension)) {
            result <<= 1;
        }
        if (rounding > 0 && rounding < kShiftLimit && result > static_cast<std::uint64_t>(dimension) &&
            (rounding == 1 || result - dimension > (result >> rounding))) {
            result >>= 1;
        }
        if (mip > 0 && mip < kShiftLimit) {
            result >>= mip;
        }
        return static_cast<int>(std::max<std::uint64_t>(1, std::min<std::uint64_t>(result, maximum)));
    };
    scaledWidth = scale(width);
    scaledHeight = scale(height);
    return true;
}

inline bool Resample(const unsigned char* source, std::size_t sourceBytes, int width, int height,
    unsigned char* destination, std::size_t destinationBytes, int outWidth, int outHeight,
    std::size_t channels, bool point = false) {
    std::size_t requiredSource;
    std::size_t requiredDestination;
    if (!source || !destination || !ByteCount(width, height, channels, requiredSource) ||
        !ByteCount(outWidth, outHeight, channels, requiredDestination) || sourceBytes < requiredSource ||
        destinationBytes < requiredDestination) {
        return false;
    }

    const std::uint64_t step = (static_cast<std::uint64_t>(width) << kFractionBits) / outWidth;
    const std::uint64_t quarterStep = step / kSampleDivisor;
    const std::uint64_t verticalStep = (static_cast<std::uint64_t>(height) << kFractionBits) / outHeight;
    for (int y = 0; y < outHeight; ++y) {
        // The indexed point path historically starts at row zero, then advances by a quarter-offset step.
        const std::size_t row1 = point ? static_cast<std::size_t>(y == 0 ? 0 :
            (verticalStep / kSampleDivisor + static_cast<std::uint64_t>(y) * verticalStep) >> kFractionBits) :
            static_cast<std::size_t>((y + 0.25) * height / outHeight);
        const std::size_t row2 = point ? row1 : static_cast<std::size_t>((y + 0.75) * height / outHeight);
        const unsigned char* firstRow = source +
            (std::min)(row1, static_cast<std::size_t>(height - 1)) * width * channels;
        const unsigned char* secondRow = source +
            (std::min)(row2, static_cast<std::size_t>(height - 1)) * width * channels;
        std::uint64_t fraction = quarterStep;
        for (int x = 0; x < outWidth; ++x, fraction += step) {
            const std::size_t first = static_cast<std::size_t>(std::min<std::uint64_t>(
                fraction >> kFractionBits, width - 1)) * channels;
            const std::size_t second = static_cast<std::size_t>(std::min<std::uint64_t>(
                (fraction + 2 * quarterStep) >> kFractionBits, width - 1)) * channels;
            for (std::size_t channel = 0; channel < channels; ++channel) {
                *destination++ = point ? firstRow[first + channel] : static_cast<unsigned char>(
                    (firstRow[first + channel] + firstRow[second + channel] +
                        secondRow[first + channel] + secondRow[second + channel]) / kSampleDivisor);
            }
        }
    }
    return true;
}

inline bool MipMap(unsigned char* pixels, std::size_t bytes, int width, int height, std::size_t channels) {
    std::size_t required;
    if (!pixels || !ByteCount(width, height, channels, required) || bytes < required) {
        return false;
    }

    constexpr int kMipScale = 2;
    const int outWidth = (std::max)(1, width / kMipScale);
    const int outHeight = (std::max)(1, height / kMipScale);
    unsigned char* out = pixels;
    for (int y = 0; y < outHeight; ++y) {
        const std::size_t firstRow = static_cast<std::size_t>(y * kMipScale) * width * channels;
        const std::size_t secondRow = static_cast<std::size_t>((std::min)(y * kMipScale + 1, height - 1)) *
            width * channels;
        for (int x = 0; x < outWidth; ++x) {
            const std::size_t first = static_cast<std::size_t>(x * kMipScale) * channels;
            const std::size_t second = static_cast<std::size_t>((std::min)(x * kMipScale + 1, width - 1)) * channels;
            for (std::size_t channel = 0; channel < channels; ++channel) {
                *out++ = static_cast<unsigned char>((pixels[firstRow + first + channel] +
                    pixels[firstRow + second + channel] + pixels[secondRow + first + channel] +
                    pixels[secondRow + second + channel]) / kSampleDivisor);
            }
        }
    }
    return true;
}

inline void AddToCounter(int& counter, std::size_t amount) {
    counter = static_cast<int>(std::min<std::int64_t>((std::numeric_limits<int>::max)(),
        static_cast<std::int64_t>(counter) + static_cast<std::int64_t>(amount)));
}

} // namespace texture_upload
