#pragma once

#include "filesystem.h"
#include "../common/image_codec.h"

#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <vector>

namespace cse {
namespace capture {

// The WebP bitstream limits each image dimension to 16383 pixels.
constexpr int MaximumDimension = 16383;

inline bool AllocateRGB(int width, int height, std::vector<unsigned char>& pixels) {
    pixels.clear();
    if (width <= 0 || height <= 0 || width > MaximumDimension || height > MaximumDimension) {
        return false;
    }
    const std::size_t rowBytes = static_cast<std::size_t>(width) * image::RgbChannels;
    if (static_cast<std::size_t>(height) > image::MaximumDecodedBytes / rowBytes) {
        return false;
    }
    try {
        pixels.resize(rowBytes * height);
    } catch (const std::bad_alloc&) {
        return false;
    }
    return true;
}

inline bool EncodeSoftware(const unsigned char* buffer, unsigned int width, unsigned int height,
    unsigned int rowBytes, bool fifteenBit, std::vector<unsigned char>& encoded) {
    encoded.clear();
    constexpr std::size_t SourcePixelBytes = sizeof(std::uint16_t);
    if (!buffer || width > MaximumDimension || height > MaximumDimension || rowBytes < width * SourcePixelBytes ||
        (height != 0 && rowBytes > image::MaximumDecodedBytes / height)) {
        return false;
    }
    std::vector<unsigned char> rgb;
    if (!AllocateRGB(static_cast<int>(width), static_cast<int>(height), rgb)) {
        return false;
    }

    constexpr std::uint16_t BlueMask = 0x001f;
    constexpr std::uint16_t Green555Mask = 0x03e0;
    constexpr std::uint16_t Red555Mask = 0x7c00;
    constexpr std::uint16_t Green565Mask = 0x07e0;
    constexpr std::uint16_t Red565Mask = 0xf800;
    constexpr int FiveBitExpansion = 3;
    constexpr int Green555Shift = 2;
    constexpr int Green565Shift = 3;
    constexpr int Red555Shift = 7;
    constexpr int Red565Shift = 8;
    for (unsigned int y = 0; y < height; ++y) {
        const unsigned char* source = buffer + static_cast<std::size_t>(y) * rowBytes;
        unsigned char* destination = rgb.data() + static_cast<std::size_t>(y) * width * image::RgbChannels;
        for (unsigned int x = 0; x < width; ++x) {
            std::uint16_t color;
            std::memcpy(&color, source + x * SourcePixelBytes, SourcePixelBytes);
            // Preserve the renderer's original zero-filled 5/6-bit channel expansion.
            destination[0] = static_cast<unsigned char>((color & (fifteenBit ? Red555Mask : Red565Mask)) >>
                (fifteenBit ? Red555Shift : Red565Shift));
            destination[1] = static_cast<unsigned char>((color & (fifteenBit ? Green555Mask : Green565Mask)) >>
                (fifteenBit ? Green555Shift : Green565Shift));
            destination[2] = static_cast<unsigned char>((color & BlueMask) << FiveBitExpansion);
            destination += image::RgbChannels;
        }
    }
    return image::EncodeWebPLossless(rgb.data(), static_cast<int>(width), static_cast<int>(height),
        static_cast<int>(width) * image::RgbChannels, image::RgbChannels, false, encoded);
}

inline bool WriteWebP(const char* filename, const std::vector<unsigned char>& encoded) {
    if (encoded.empty() || encoded.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        return false;
    }
    constexpr const char* CapturePathID = "GAMECONFIG";
    FileHandle_t file = FS_OpenPathID(filename, "wb", CapturePathID);
    if (!file) {
        return false;
    }
    const int length = static_cast<int>(encoded.size());
    bool written = FS_Write(encoded.data(), length, 1, file) == length;
    FS_Flush(file);
    written = written && FS_IsOk(file);
    FS_Close(file);
    if (!written) {
        FS_RemoveFile(filename, CapturePathID);
    }
    return written;
}

} // namespace capture
} // namespace cse
