#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace cse { namespace pe {

struct ExportName {
    const char *data;
    std::size_t size;
};

namespace detail {
// PE/COFF file widths are independent of the process pointer width.
constexpr std::size_t DosHeaderBytes = 64;
constexpr std::size_t NtHeaderOffset = 60;
constexpr std::size_t SignatureBytes = 4;
constexpr std::size_t CoffHeaderBytes = 20;
constexpr std::size_t SectionCountOffset = 2;
constexpr std::size_t OptionalBytesOffset = 16;
constexpr std::size_t OptionalHeaderSizeOffset = 60;
constexpr std::size_t DirectoryCount32Offset = 92;
constexpr std::size_t DirectoryCount64Offset = 108;
constexpr std::size_t DirectoryBytes = 8;
constexpr std::size_t SectionBytes = 40;
constexpr std::size_t SectionRvaOffset = 12;
constexpr std::size_t SectionRawBytesOffset = 16;
constexpr std::size_t SectionRawOffset = 20;
constexpr std::size_t ExportDirectoryBytes = 40;
constexpr std::size_t ExportNameCountOffset = 24;
constexpr std::size_t ExportNamesRvaOffset = 32;
constexpr std::uint16_t DosSignature = UINT16_C(0x5a4d);
constexpr std::uint32_t PeSignature = UINT32_C(0x00004550);
constexpr std::uint16_t Pe32Magic = UINT16_C(0x10b);
constexpr std::uint16_t Pe64Magic = UINT16_C(0x20b);
constexpr unsigned int ByteBits = 8;

class Image {
public:
    Image(const void *bytes, std::size_t size)
        : bytes_(static_cast<const unsigned char *>(bytes)), size_(size) {}

    bool Initialize(std::uint32_t &exportRva, std::uint32_t &exportBytes) {
        std::uint16_t dos = 0;
        std::uint32_t nt = 0;
        std::uint32_t signature = 0;
        if (!bytes_ || !Span(0, DosHeaderBytes) || !Read16(0, dos) || dos != DosSignature
            || !Read32(NtHeaderOffset, nt) || !Span(nt, SignatureBytes + CoffHeaderBytes)
            || !Read32(nt, signature) || signature != PeSignature) {
            return false;
        }
        const auto coff = static_cast<std::size_t>(nt) + SignatureBytes;
        std::uint16_t sections = 0;
        std::uint16_t optionalBytes = 0;
        if (!Read16(coff + SectionCountOffset, sections)
            || !Read16(coff + OptionalBytesOffset, optionalBytes)) {
            return false;
        }
        const auto optional = coff + CoffHeaderBytes;
        std::uint16_t magic = 0;
        if (!Span(optional, optionalBytes) || optionalBytes < sizeof(magic) || !Read16(optional, magic)) {
            return false;
        }
        const auto directoryCountOffset = magic == Pe32Magic ? DirectoryCount32Offset : DirectoryCount64Offset;
        if ((magic != Pe32Magic && magic != Pe64Magic)
            || optionalBytes < directoryCountOffset + sizeof(std::uint32_t)) {
            return false;
        }
        std::uint32_t directoryCount = 0;
        if (!Read32(optional + directoryCountOffset, directoryCount)
            || !Read32(optional + OptionalHeaderSizeOffset, headerBytes_)) {
            return false;
        }
        const auto directories = directoryCountOffset + sizeof(directoryCount);
        if (directoryCount > (optionalBytes - directories) / DirectoryBytes) {
            return false;
        }
        sectionOffset_ = optional + optionalBytes;
        sectionCount_ = sections;
        const auto sectionBytes = sectionCount_ * SectionBytes;
        if (!Span(sectionOffset_, sectionBytes) || headerBytes_ > size_
            || headerBytes_ < sectionOffset_ + sectionBytes) {
            return false;
        }
        for (std::size_t index = 0; index < sectionCount_; ++index) {
            const auto section = sectionOffset_ + index * SectionBytes;
            std::uint32_t rva = 0;
            std::uint32_t rawBytes = 0;
            std::uint32_t raw = 0;
            if (!Read32(section + SectionRvaOffset, rva)
                || !Read32(section + SectionRawBytesOffset, rawBytes)
                || !Read32(section + SectionRawOffset, raw)
                || (rawBytes != 0 && !Span(raw, rawBytes)) || rawBytes > UINT32_MAX - rva) {
                return false;
            }
        }
        exportRva = 0;
        exportBytes = 0;
        return directoryCount == 0
            || (Read32(optional + directories, exportRva)
                && Read32(optional + directories + sizeof(exportRva), exportBytes));
    }

    bool Read32(std::size_t offset, std::uint32_t &value) const {
        if (!Span(offset, sizeof(value))) {
            return false;
        }
        value = 0;
        for (unsigned int index = 0; index < sizeof(value); ++index) {
            value |= static_cast<std::uint32_t>(bytes_[offset + index]) << (index * ByteBits);
        }
        return true;
    }

    bool Map(std::uint32_t rva, std::size_t bytes, std::size_t &offset, std::size_t &available) const {
        if (rva < headerBytes_) {
            if (bytes > headerBytes_ - rva || !Span(rva, bytes)) {
                return false;
            }
            offset = rva;
            available = headerBytes_ - rva;
            return true;
        }
        bool found = false;
        for (std::size_t index = 0; index < sectionCount_; ++index) {
            const auto section = sectionOffset_ + index * SectionBytes;
            std::uint32_t base = 0;
            std::uint32_t rawBytes = 0;
            std::uint32_t raw = 0;
            if (!Read32(section + SectionRvaOffset, base)
                || !Read32(section + SectionRawBytesOffset, rawBytes)
                || !Read32(section + SectionRawOffset, raw)) {
                return false;
            }
            if (rva < base || rva - base >= rawBytes) {
                continue;
            }
            const auto delta = rva - base;
            if (found || bytes > rawBytes - delta) {
                return false;
            }
            offset = static_cast<std::size_t>(raw) + delta;
            available = rawBytes - delta;
            if (!Span(offset, bytes)) {
                return false;
            }
            found = true;
        }
        return found;
    }

    bool Name(std::uint32_t rva, ExportName &name) const {
        std::size_t offset = 0;
        std::size_t available = 0;
        if (!Map(rva, 1, offset, available)) {
            return false;
        }
        const auto *start = reinterpret_cast<const char *>(bytes_ + offset);
        const auto *end = static_cast<const char *>(std::memchr(start, 0, available));
        if (!end || end == start) {
            return false;
        }
        name = {start, static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(end)
            - reinterpret_cast<std::uintptr_t>(start))};
        return true;
    }

    std::size_t Size() const {
        return size_;
    }

private:
    bool Span(std::size_t offset, std::size_t bytes) const {
        return offset <= size_ && bytes <= size_ - offset;
    }

    bool Read16(std::size_t offset, std::uint16_t &value) const {
        if (!Span(offset, sizeof(value))) {
            return false;
        }
        value = static_cast<std::uint16_t>(bytes_[offset]
            | (static_cast<std::uint16_t>(bytes_[offset + 1]) << ByteBits));
        return true;
    }

    const unsigned char *bytes_;
    std::size_t size_;
    std::size_t sectionOffset_ = 0;
    std::size_t sectionCount_ = 0;
    std::uint32_t headerBytes_ = 0;
};
} // namespace detail

// Names borrow the input buffer. Consumers commit output only after the visit succeeds.
template<typename Consumer>
bool VisitExportNames(const void *bytes, std::size_t size, Consumer consumer) {
    detail::Image image(bytes, size);
    std::uint32_t exportRva = 0;
    std::uint32_t exportBytes = 0;
    if (!image.Initialize(exportRva, exportBytes)) {
        return false;
    }
    if (exportRva == 0) {
        return exportBytes == 0;
    }
    if (exportBytes < detail::ExportDirectoryBytes || exportBytes > UINT32_MAX - exportRva) {
        return false;
    }
    std::size_t directory = 0;
    std::size_t available = 0;
    std::uint32_t count = 0;
    std::uint32_t names = 0;
    if (!image.Map(exportRva, detail::ExportDirectoryBytes, directory, available)
        || !image.Read32(directory + detail::ExportNameCountOffset, count)
        || !image.Read32(directory + detail::ExportNamesRvaOffset, names)
        || count > image.Size() / sizeof(std::uint32_t)
        || count > (UINT32_MAX - names) / sizeof(std::uint32_t)) {
        return false;
    }
    for (std::uint32_t index = 0; index < count; ++index) {
        std::size_t entry = 0;
        std::uint32_t nameRva = 0;
        ExportName name{};
        const auto entryRva = names + index * static_cast<std::uint32_t>(sizeof(nameRva));
        if (!image.Map(entryRva, sizeof(nameRva), entry, available) || !image.Read32(entry, nameRva)
            || !image.Name(nameRva, name) || !consumer(name)) {
            return false;
        }
    }
    return true;
}
} } // namespace cse::pe
