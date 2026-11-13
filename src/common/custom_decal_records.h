#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

namespace cse {
static_assert(std::numeric_limits<int>::digits == 31, "Custom WAD/cache interfaces require signed 32-bit int");
constexpr std::size_t CustomWadHeaderBytes = 12;
constexpr std::size_t CustomLumpRecordBytes = 32;
constexpr std::size_t CustomMipHeaderBytes = 40;
constexpr unsigned CustomMipLevels = 4;
constexpr unsigned CustomPaletteColors = 256;
constexpr std::size_t CustomPaletteCountBytes = 2;
constexpr std::size_t CustomPaletteBytes = CustomPaletteColors * 3;
constexpr std::uint32_t MaximumCustomDimension = 256;

struct CustomMipRecord {
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t offsets[CustomMipLevels];
    std::uint32_t pixelBytes;
    std::size_t paletteOffset;
    unsigned paletteColors;
};

struct CustomWadRecord {
    int directoryOffset;
    int lumpOffset;
    int lumpBytes;
    CustomMipRecord mip;
};

inline std::uint32_t CustomReadLittle32(const unsigned char* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) | (static_cast<std::uint32_t>(bytes[1]) << 8)
        | (static_cast<std::uint32_t>(bytes[2]) << 16) | (static_cast<std::uint32_t>(bytes[3]) << 24);
}

inline unsigned CustomReadLittle16(const unsigned char* bytes) {
    return static_cast<unsigned>(bytes[0]) | (static_cast<unsigned>(bytes[1]) << 8);
}

// The caller supplies a genuine initialized borrowed byte span. Successful output
// describes every read of the retained HW/SW custom texture builders, including
// their distinct fixed-header count and first-mip-relative palette positions.
inline bool ReadCustomMip(const void* raw, std::size_t bytes, CustomMipRecord* output) {
    constexpr std::size_t MaximumSignedWireValue = 0x7fffffffU;
    if (!raw || !output || bytes < CustomMipHeaderBytes || bytes > MaximumSignedWireValue) {
        return false;
    }
    const auto* data = static_cast<const unsigned char*>(raw);
    CustomMipRecord result{};
    constexpr unsigned WidthOffset = 16;
    constexpr unsigned HeightOffset = 20;
    constexpr unsigned MipOffsetsOffset = 24;
    constexpr unsigned DiskIntegerBytes = 4;
    result.width = CustomReadLittle32(data + WidthOffset);
    result.height = CustomReadLittle32(data + HeightOffset);
    if (!result.width || !result.height || result.width > MaximumCustomDimension
        || result.height > MaximumCustomDimension) {
        return false;
    }
    std::uint32_t levelBytes = result.width * result.height;
    for (unsigned index = 0; index < CustomMipLevels; ++index) {
        result.offsets[index] = CustomReadLittle32(data + MipOffsetsOffset + index * DiskIntegerBytes);
        if (result.offsets[index] > bytes || levelBytes > bytes - result.offsets[index]) {
            return false;
        }
        if (index) {
            // Retain the original area shifts, including tiny odd dimensions.
            const std::uint32_t previousBytes = (result.width * result.height) >> ((index - 1) * 2);
            if (result.offsets[index - 1] + previousBytes != result.offsets[index]) {
                return false;
            }
        }
        result.pixelBytes += levelBytes;
        levelBytes >>= 2;
    }
    // Legacy Win32 can accept this malformed class and overwrite pixel bytes with
    // pointer-sized texture-header fields. Native caches deliberately reject that
    // representation-dependent alias; safe gaps after the 40-byte header remain valid.
    if (result.offsets[0] < CustomMipHeaderBytes) {
        return false;
    }
    const std::size_t countOffset = CustomMipHeaderBytes + result.pixelBytes;
    if (countOffset > bytes || CustomPaletteCountBytes > bytes - countOffset) {
        return false;
    }
    result.paletteColors = CustomReadLittle16(data + countOffset);
    if (result.paletteColors > CustomPaletteColors) {
        return false;
    }
    const std::size_t paletteCountOffset = static_cast<std::size_t>(result.offsets[0]) + result.pixelBytes;
    if (paletteCountOffset > bytes || CustomPaletteCountBytes > bytes - paletteCountOffset) {
        return false;
    }
    result.paletteOffset = paletteCountOffset + CustomPaletteCountBytes;
    if (CustomPaletteBytes > bytes - result.paletteOffset) {
        return false;
    }
    *output = result;
    return true;
}

inline bool ReadCustomWad(const void* raw, std::size_t initializedBytes, std::size_t logicalBytes,
                          CustomWadRecord* output) {
    constexpr std::uint32_t MaximumSignedWireValue = 0x7fffffffU;
    constexpr unsigned DirectoryOffsetField = 8;
    constexpr unsigned LumpCountField = 4;
    if (!raw || !output || logicalBytes > initializedBytes || logicalBytes > MaximumSignedWireValue
        || logicalBytes < CustomWadHeaderBytes + CustomLumpRecordBytes) {
        return false;
    }
    const auto* data = static_cast<const unsigned char*>(raw);
    if (data[0] != 'W' || data[1] != 'A' || data[2] != 'D' || data[3] != '3'
        || CustomReadLittle32(data + LumpCountField) != 1) {
        return false;
    }
    const std::uint32_t table = CustomReadLittle32(data + DirectoryOffsetField);
    if (table < CustomWadHeaderBytes || table > logicalBytes || CustomLumpRecordBytes != logicalBytes - table) {
        return false;
    }
    constexpr unsigned LumpFileOffset = 0;
    constexpr unsigned LumpDiskSizeOffset = 4;
    constexpr unsigned LumpSizeOffset = 8;
    const std::uint32_t offset = CustomReadLittle32(data + table + LumpFileOffset);
    const std::uint32_t size = CustomReadLittle32(data + table + LumpDiskSizeOffset);
    if (offset < CustomWadHeaderBytes || offset > table || !size || size > table - offset
        || size != CustomReadLittle32(data + table + LumpSizeOffset)) {
        return false;
    }
    CustomWadRecord result{};
    if (!ReadCustomMip(data + offset, size, &result.mip)) {
        return false;
    }
    result.directoryOffset = static_cast<int>(table);
    result.lumpOffset = static_cast<int>(offset);
    result.lumpBytes = static_cast<int>(size);
    *output = result;
    return true;
}

inline bool CustomCacheBytes(std::size_t mipBytes, std::size_t extraBytes, int* output) {
    constexpr std::size_t TerminatorBytes = 1;
    constexpr std::size_t MaximumCacheBytes = static_cast<std::size_t>((std::numeric_limits<int>::max)());
    if (!output || mipBytes > MaximumCacheBytes || extraBytes > MaximumCacheBytes - mipBytes
        || TerminatorBytes > MaximumCacheBytes - mipBytes - extraBytes) {
        return false;
    }
    *output = static_cast<int>(mipBytes + extraBytes + TerminatorBytes);
    return true;
}
} // namespace cse
