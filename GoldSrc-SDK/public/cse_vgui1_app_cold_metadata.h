#ifndef CSE_VGUI1_APP_COLD_METADATA_H
#define CSE_VGUI1_APP_COLD_METADATA_H

#include "cse_module_abi.h"

namespace cse { namespace vgui1_app_cold_metadata {
constexpr std::uint64_t Magic = 0x3130444341455343ull; // Little-endian bytes: CSEACD01.
constexpr std::uint32_t FormatVersion = 1;
constexpr std::uint32_t RoleFamily = 0x4f4d0001;
constexpr std::uint32_t CppProfileFamily = 0x4f4d0002;
constexpr std::uint32_t AppColdFamily = 0x4f4d0206;
constexpr std::size_t DigestBytes = 32;
constexpr std::size_t UuidBytes = 16;
constexpr std::size_t FramingBytes = 32;
constexpr std::size_t ProvenanceOffset = FramingBytes;
constexpr std::size_t DescriptorOffset = ProvenanceOffset + DigestBytes;
constexpr std::size_t RecordBytes = DescriptorOffset + sizeof(abi::Descriptor);
constexpr std::size_t CatalogTables = 3;
constexpr char SegmentName[] = "__DATA_CONST";
constexpr char SectionName[] = "__cse_vgui1";

enum class Kind : std::uint32_t { Offer = 1, Requirement = 2 };

struct Digest {
    unsigned char bytes[DigestBytes];
};

// The serialized object is ONLY this byte array. Encoding is explicitly little
// endian and never serializes a native struct, pointer, padding or member pointer.
struct EncodedRecord {
    unsigned char bytes[RecordBytes];
};

struct Record {
    Kind kind;
    Digest provenance;
    abi::Descriptor catalog;
};

namespace detail {
constexpr void Put32(EncodedRecord& output, std::size_t offset, std::uint32_t value) {
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        output.bytes[offset + index] = static_cast<unsigned char>(value & 0xffU);
        value >>= CHAR_BIT;
    }
}

constexpr void Put64(EncodedRecord& output, std::size_t offset, std::uint64_t value) {
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        output.bytes[offset + index] = static_cast<unsigned char>(value & 0xffU);
        value >>= CHAR_BIT;
    }
}
}

// Only trusted, compiler-generated inputs feed the publisher. The reader checks
// every field independently; provenance is freshness, NOT ABI compatibility.
constexpr EncodedRecord Encode(Kind kind, const Digest& provenance, const abi::Descriptor& catalog) {
    EncodedRecord result{};
    detail::Put64(result, 0, Magic);
    detail::Put32(result, 8, FormatVersion);
    detail::Put32(result, 12, static_cast<std::uint32_t>(RecordBytes));
    detail::Put32(result, 16, static_cast<std::uint32_t>(kind));
    detail::Put32(result, 20, AppColdFamily);
    // Bytes24..31 are reserved zero. There are no implicit native-layout bytes.
    for (std::size_t index = 0; index < DigestBytes; ++index) {
        result.bytes[ProvenanceOffset + index] = provenance.bytes[index];
    }
    const std::uint32_t prefix[]{catalog.descriptorBytes, catalog.descriptorVersion, catalog.runtimeVersion,
        catalog.architecture, catalog.pointerBits, catalog.compilerAbi, catalog.tableCount, catalog.reserved};
    for (std::size_t index = 0; index < sizeof(prefix) / sizeof(prefix[0]); ++index) {
        detail::Put32(result, DescriptorOffset + index * sizeof(prefix[0]), prefix[index]);
    }
    constexpr std::size_t TableOffset = 32;
    constexpr std::size_t TableBytes = 16;
    for (std::size_t index = 0; index < abi::MaxTables; ++index) {
        const std::size_t offset = DescriptorOffset + TableOffset + index * TableBytes;
        detail::Put32(result, offset, catalog.tables[index].identifier);
        detail::Put32(result, offset + sizeof(std::uint32_t), catalog.tables[index].reserved);
        detail::Put64(result, offset + sizeof(std::uint64_t), catalog.tables[index].signature);
    }
    return result;
}

static_assert(abi::RuntimeVersion == 10 && sizeof(abi::Descriptor) == 288 && abi::MaxTables == 16,
    "Preserve the existing bounded runtime descriptor");
static_assert(RecordBytes == 352 && sizeof(EncodedRecord) == RecordBytes, "Pointer-free fixed byte record");
} }
#endif
