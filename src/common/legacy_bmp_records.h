#ifndef CSE_LEGACY_BMP_RECORDS_H
#define CSE_LEGACY_BMP_RECORDS_H

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace cse_bmp {

// BMP disk records use little-endian Windows widths on every supported CPU.
// The 14-byte file prefix has the SDK's two-byte packing; it is never a native long.
#pragma pack(push, 2)
struct FileHeader {
    std::uint16_t bfType;
    std::uint32_t bfSize;
    std::uint16_t bfReserved1;
    std::uint16_t bfReserved2;
    std::uint32_t bfOffBits;
};
#pragma pack(pop)

struct InfoHeader {
    std::uint32_t biSize;
    std::int32_t biWidth;
    std::int32_t biHeight;
    std::uint16_t biPlanes;
    std::uint16_t biBitCount;
    std::uint32_t biCompression;
    std::uint32_t biSizeImage;
    std::int32_t biXPelsPerMeter;
    std::int32_t biYPelsPerMeter;
    std::uint32_t biClrUsed;
    std::uint32_t biClrImportant;
};

struct PaletteEntry {
    std::uint8_t rgbBlue;
    std::uint8_t rgbGreen;
    std::uint8_t rgbRed;
    std::uint8_t rgbReserved;
};

constexpr std::uint32_t RgbCompression = 0;
static_assert(sizeof(FileHeader) == 14 && alignof(FileHeader) == 2, "BMP file prefix");
static_assert(offsetof(FileHeader, bfOffBits) == 10, "BMP pixel offset");
static_assert(sizeof(InfoHeader) == 40 && alignof(InfoHeader) == 4, "BMP info envelope");
static_assert(offsetof(InfoHeader, biWidth) == 4 && offsetof(InfoHeader, biClrImportant) == 36,
    "BMP info field boundaries");
static_assert(sizeof(PaletteEntry) == 4 && alignof(PaletteEntry) == 1, "BMP BGR palette envelope");
static_assert(std::is_trivially_copyable<FileHeader>::value && std::is_standard_layout<FileHeader>::value
    && std::is_trivially_copyable<InfoHeader>::value && std::is_standard_layout<InfoHeader>::value
    && std::is_trivially_copyable<PaletteEntry>::value && std::is_standard_layout<PaletteEntry>::value,
    "BMP disk records contain no native ownership");

}
#endif
