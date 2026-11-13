#include "../../../src/common/legacy_bmp_records.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>

#if defined(_WIN32)
#include <windows.h>
#endif

int main() {
    using namespace cse_bmp;
    // A negative height and high-bit lengths expose accidental LP64 signed/long records.
    constexpr unsigned char FileBytes[] = {
        0x42, 0x4d, 0x98, 0xba, 0xdc, 0xfe, 0x34, 0x12, 0x78, 0x56, 0x36, 0x04, 0x00, 0x00
    };
    constexpr unsigned char InfoBytes[] = {
        0x28, 0x00, 0x00, 0x00, 0x34, 0x12, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
        0x01, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x98, 0xba, 0xdc, 0xfe,
        0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x80, 0x00, 0x01, 0x00, 0x00,
        0x80, 0x00, 0x00, 0x00
    };
    FileHeader file{};
    InfoHeader info{};
    std::memcpy(&file, FileBytes, sizeof(file));
    std::memcpy(&info, InfoBytes, sizeof(info));
    if (file.bfType != 0x4d42 || file.bfSize != UINT32_C(0xfedcba98) || file.bfReserved1 != 0x1234
        || file.bfReserved2 != 0x5678 || file.bfOffBits != 1078 || info.biSize != 40
        || info.biWidth != 0x1234 || info.biHeight != -1 || info.biPlanes != 1 || info.biBitCount != 8
        || info.biCompression != RgbCompression || info.biSizeImage != UINT32_C(0xfedcba98)
        || info.biXPelsPerMeter != -1 || info.biYPelsPerMeter != INT32_MIN || info.biClrUsed != 256
        || info.biClrImportant != 128) {
        return 1;
    }
    unsigned char encoded[sizeof(InfoHeader)]{};
    std::memcpy(encoded, &info, sizeof(info));
    if (std::memcmp(encoded, InfoBytes, sizeof(InfoBytes)) != 0) {
        return 2;
    }
#if defined(_WIN32)
    static_assert(sizeof(FileHeader) == sizeof(BITMAPFILEHEADER), "Actual SDK file record");
    static_assert(alignof(FileHeader) == alignof(BITMAPFILEHEADER), "Actual SDK file alignment");
    static_assert(sizeof(InfoHeader) == sizeof(BITMAPINFOHEADER), "Actual SDK info record");
    static_assert(alignof(InfoHeader) == alignof(BITMAPINFOHEADER), "Actual SDK info alignment");
    static_assert(sizeof(PaletteEntry) == sizeof(RGBQUAD), "Actual SDK palette record");
    static_assert(alignof(PaletteEntry) == alignof(RGBQUAD), "Actual SDK palette alignment");
#define CHECK_FIELD(ours, sdk, field) \
    static_assert(offsetof(ours, field) == offsetof(sdk, field), "Actual SDK " #field); \
    static_assert(sizeof(ours::field) == sizeof(sdk::field), "Actual SDK width " #field)
    CHECK_FIELD(FileHeader, BITMAPFILEHEADER, bfType);
    CHECK_FIELD(FileHeader, BITMAPFILEHEADER, bfSize);
    CHECK_FIELD(FileHeader, BITMAPFILEHEADER, bfReserved1);
    CHECK_FIELD(FileHeader, BITMAPFILEHEADER, bfReserved2);
    CHECK_FIELD(FileHeader, BITMAPFILEHEADER, bfOffBits);
    CHECK_FIELD(InfoHeader, BITMAPINFOHEADER, biSize);
    CHECK_FIELD(InfoHeader, BITMAPINFOHEADER, biWidth);
    CHECK_FIELD(InfoHeader, BITMAPINFOHEADER, biHeight);
    CHECK_FIELD(InfoHeader, BITMAPINFOHEADER, biPlanes);
    CHECK_FIELD(InfoHeader, BITMAPINFOHEADER, biBitCount);
    CHECK_FIELD(InfoHeader, BITMAPINFOHEADER, biCompression);
    CHECK_FIELD(InfoHeader, BITMAPINFOHEADER, biSizeImage);
    CHECK_FIELD(InfoHeader, BITMAPINFOHEADER, biXPelsPerMeter);
    CHECK_FIELD(InfoHeader, BITMAPINFOHEADER, biYPelsPerMeter);
    CHECK_FIELD(InfoHeader, BITMAPINFOHEADER, biClrUsed);
    CHECK_FIELD(InfoHeader, BITMAPINFOHEADER, biClrImportant);
    CHECK_FIELD(PaletteEntry, RGBQUAD, rgbBlue);
    CHECK_FIELD(PaletteEntry, RGBQUAD, rgbGreen);
    CHECK_FIELD(PaletteEntry, RGBQUAD, rgbRed);
    CHECK_FIELD(PaletteEntry, RGBQUAD, rgbReserved);
#undef CHECK_FIELD
#endif
    std::puts("PASS fixed-width BMP records and authored byte round trip");
    return 0;
}
