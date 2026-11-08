#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace fixture {
constexpr std::size_t PeOffset = 128;
constexpr std::size_t CoffBytes = 20;
constexpr std::size_t SignatureBytes = 4;
constexpr std::size_t OptionalOffset = PeOffset + SignatureBytes + CoffBytes;
constexpr std::size_t RawOffset = 512;
constexpr std::uint32_t SectionRva = 8192;
constexpr std::size_t ImageBytes = 2048;
constexpr std::size_t ExportNameCountOffset = 24;
constexpr std::size_t ExportNamesRvaOffset = 32;
constexpr std::size_t NamesOffset = 64;
constexpr std::size_t TextOffset = 96;

void Put16(std::vector<unsigned char> &data, std::size_t offset, std::uint16_t value) {
    data[offset] = static_cast<unsigned char>(value);
    data[offset + 1] = static_cast<unsigned char>(value >> 8);
}

void Put32(std::vector<unsigned char> &data, std::size_t offset, std::uint32_t value) {
    for (unsigned int index = 0; index < sizeof(value); ++index) {
        data[offset + index] = static_cast<unsigned char>(value >> (index * 8));
    }
}

std::vector<unsigned char> MakeImage(bool pe64) {
    std::vector<unsigned char> data(ImageBytes);
    Put16(data, 0, UINT16_C(0x5a4d)); // DOS MZ signature.
    Put32(data, 60, PeOffset);
    Put32(data, PeOffset, UINT32_C(0x00004550)); // PE signature.
    Put16(data, PeOffset + SignatureBytes, pe64 ? UINT16_C(0x8664) : UINT16_C(0x14c));
    Put16(data, PeOffset + SignatureBytes + 2, 1);
    constexpr std::uint16_t Optional32Bytes = 224;
    constexpr std::uint16_t Optional64Bytes = 240;
    const auto optionalBytes = pe64 ? Optional64Bytes : Optional32Bytes;
    Put16(data, PeOffset + SignatureBytes + 16, optionalBytes);
    Put16(data, OptionalOffset, pe64 ? UINT16_C(0x20b) : UINT16_C(0x10b));
    Put32(data, OptionalOffset + 60, RawOffset);
    const auto directoryCount = OptionalOffset + (pe64 ? 108 : 92);
    Put32(data, directoryCount, 16);
    Put32(data, directoryCount + sizeof(std::uint32_t), SectionRva);
    Put32(data, directoryCount + 2 * sizeof(std::uint32_t), 128);
    const auto section = OptionalOffset + optionalBytes;
    std::memcpy(data.data() + section, ".rdata", 7);
    Put32(data, section + 8, ImageBytes - RawOffset);
    Put32(data, section + 12, SectionRva);
    Put32(data, section + 16, ImageBytes - RawOffset);
    Put32(data, section + 20, RawOffset);
    // Export directory: one name RVA, independent of optional-header width.
    Put32(data, RawOffset + 20, 1);
    Put32(data, RawOffset + ExportNameCountOffset, 1);
    Put32(data, RawOffset + ExportNamesRvaOffset, SectionRva + NamesOffset);
    Put32(data, RawOffset + NamesOffset, SectionRva + TextOffset);
    const char name[] = "?Think@OwnedEntity@@QEAAXXZ";
    std::memcpy(data.data() + RawOffset + TextOffset, name, sizeof(name));
    return data;
}

} // namespace fixture
