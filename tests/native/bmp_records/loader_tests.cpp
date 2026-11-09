#include "../../../GoldSrc-SDK/common/legacy_bmp_records.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#if defined(CSE_BMP_WINDOWS_REFERENCE)
#include <windows.h>
#else
using BITMAPFILEHEADER = cse_bmp::FileHeader;
using BITMAPINFOHEADER = cse_bmp::InfoHeader;
using RGBQUAD = cse_bmp::PaletteEntry;
constexpr std::uint32_t BI_RGB = cse_bmp::RgbCompression;
#endif

using byte = unsigned char;
using qboolean = int;
enum FileSystemSeek_t { FILESYSTEM_SEEK_HEAD, FILESYSTEM_SEEK_CURRENT, FILESYSTEM_SEEK_TAIL };
struct MemoryFile {
    std::vector<byte> bytes;
    std::size_t cursor = 0;
    int closes = 0;
    int reads = 0;
};
using FileHandle_t = MemoryFile*;

static int FS_Read(void* output, int size, int count, FileHandle_t file) {
    if (count != 1 || size < 0 || file->cursor > file->bytes.size()) {
        std::abort();
    }
    ++file->reads;
    const auto copied = (std::min)(static_cast<std::size_t>(size), file->bytes.size() - file->cursor);
    std::memcpy(output, file->bytes.data() + file->cursor, copied);
    file->cursor += copied;
    return static_cast<int>(copied);
}
static unsigned int FS_Size(FileHandle_t file) {
    return static_cast<unsigned int>(file->bytes.size());
}
static unsigned int FS_Tell(FileHandle_t file) {
    return static_cast<unsigned int>(file->cursor);
}
static void FS_Close(FileHandle_t file) {
    ++file->closes;
}
static void FS_Seek(FileHandle_t file, int position, FileSystemSeek_t origin) {
    const std::int64_t base = origin == FILESYSTEM_SEEK_HEAD ? 0
        : (origin == FILESYSTEM_SEEK_CURRENT ? static_cast<std::int64_t>(file->cursor)
            : static_cast<std::int64_t>(file->bytes.size()));
    if (base + position < 0 || static_cast<std::uint64_t>(base + position) > file->bytes.size()) {
        std::abort();
    }
    file->cursor = static_cast<std::size_t>(base + position);
}
static void* Mem_Malloc(std::size_t bytes) {
    void* result = std::calloc(bytes, 1);
    if (!result) {
        std::abort();
    }
    return result;
}
static void Mem_Free(void* pointer) {
    std::free(pointer);
}
static void* Hunk_AllocName(int bytes, const char*) {
    if (bytes < 0) {
        std::abort();
    }
    return Mem_Malloc(static_cast<std::size_t>(bytes));
}
#define Q_memset std::memset
#include "loader_bodies.inc"
#undef Q_memset

static MemoryFile AuthoredBmp(int width, int height) {
    constexpr std::size_t PaletteEntries = 256;
    const std::size_t stride = (static_cast<std::size_t>(width) + 3) & ~std::size_t(3);
    cse_bmp::FileHeader file{};
    cse_bmp::InfoHeader info{};
    file.bfType = 0x4d42;
    file.bfOffBits = sizeof(file) + sizeof(info) + PaletteEntries * sizeof(cse_bmp::PaletteEntry);
    file.bfSize = file.bfOffBits + static_cast<std::uint32_t>(stride * static_cast<std::size_t>(height));
    info.biSize = sizeof(info);
    info.biWidth = width;
    info.biHeight = height;
    info.biPlanes = 1;
    info.biBitCount = 8;
    info.biSizeImage = static_cast<std::uint32_t>(stride * static_cast<std::size_t>(height));
    MemoryFile result;
    result.bytes.resize(file.bfSize);
    std::memcpy(result.bytes.data(), &file, sizeof(file));
    std::memcpy(result.bytes.data() + sizeof(file), &info, sizeof(info));
    for (std::size_t index = 0; index < PaletteEntries; ++index) {
        const auto value = static_cast<byte>(index);
        const cse_bmp::PaletteEntry entry{value, static_cast<byte>(255 - value), static_cast<byte>(value ^ 0x55), 0};
        std::memcpy(result.bytes.data() + sizeof(file) + sizeof(info) + index * sizeof(entry), &entry, sizeof(entry));
    }
    for (std::size_t index = file.bfOffBits; index < result.bytes.size(); ++index) {
        result.bytes[index] = static_cast<byte>((index - file.bfOffBits) * 17 + 3);
    }
    return result;
}

static void PrintBytes(const byte* bytes, std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) {
        std::printf("%02x", static_cast<unsigned int>(bytes[index]));
    }
    std::putchar('\n');
}

int main() {
    constexpr int MaximumWidth = 12;
    constexpr int MaximumHeight = 3;
    for (int height = 1; height <= MaximumHeight; ++height) {
        for (int width = 1; width <= MaximumWidth; ++width) {
            MemoryFile file = AuthoredBmp(width, height);
            byte* palette = nullptr;
            byte* image = nullptr;
            int decodedWidth = 0;
            int decodedHeight = 0;
            int paletteCount = 731;
            LoadBMP8(&file, &palette, &paletteCount, &image, &decodedWidth, &decodedHeight);
            if (!palette || !image || decodedWidth != width || decodedHeight != height || file.closes != 1
                || file.reads != 4 || paletteCount != 731) {
                return 1;
            }
            std::printf("bmp8 %d %d %d %d %zu\n", width, height, file.reads, file.closes, file.cursor);
            PrintBytes(palette, 256 * 3);
            const std::size_t stride = (static_cast<std::size_t>(width) + 3) & ~std::size_t(3);
            PrintBytes(image, stride * static_cast<std::size_t>(height));
            std::free(palette);
            Mem_Free(image);
            // Preserve the existing byte-count-vs-item-count failure in LoadBMP16.
            file = AuthoredBmp(width, height);
            if (LoadBMP16(&file, 1) || file.reads != 1 || file.cursor != sizeof(cse_bmp::FileHeader)
                || file.closes != 1) {
                return 2;
            }
            std::printf("bmp16-current-rejection %d %d %zu\n", width, height, file.cursor);
            // The retained VGUI pitch calculation is measured at aligned widths.
            if (width % 4 == 0) {
                file = AuthoredBmp(width, height);
                std::vector<byte> rgba(static_cast<std::size_t>(width * height) * 4);
                if (VGui_LoadBMP(&file, rgba.data(), static_cast<int>(rgba.size()), &decodedWidth, &decodedHeight) != 1
                    || decodedWidth != width || decodedHeight != height || file.closes != 1 || file.reads != 2) {
                    return 3;
                }
                std::printf("vgui8 %d %d %d %d %zu\n", width, height, file.reads, file.closes, file.cursor);
                PrintBytes(rgba.data(), rgba.size());
            }
        }
    }
    return 0;
}
