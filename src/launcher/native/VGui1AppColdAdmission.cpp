#if !defined(__APPLE__)
#error This unselected file reader implements only the Apple Mach-O boundary
#endif

#include "VGui1AppColdAdmission.h"
#include <CommonCrypto/CommonDigest.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <limits.h>
#include <mach-o/loader.h>
#include <sys/stat.h>
#include <unistd.h>

namespace cse_vgui1_admission {
namespace {
constexpr std::size_t MachHeaderBytes = 32;
constexpr std::size_t CommandHeaderBytes = 8;
constexpr std::uint32_t LoadCommandAlignment = 8; // Mach-O64 load commands are eight-byte aligned.
constexpr std::size_t SegmentBytes = 72;
constexpr std::size_t SectionBytes = 80;
constexpr std::size_t UuidCommandBytes = 24;
constexpr std::size_t DylibCommandBytes = 24;
constexpr std::uint32_t MaximumLoadCommands = 4096;
constexpr std::uint32_t MaximumSections = 1024;
constexpr std::uint32_t MaximumCommandBytes = 1024 * 1024;
constexpr std::size_t DigestChunkBytes = 64 * 1024;
constexpr int MaximumReadInterruptions = 8;
static_assert(sizeof(mach_header_64) == MachHeaderBytes && sizeof(segment_command_64) == SegmentBytes
    && sizeof(section_64) == SectionBytes && sizeof(uuid_command) == UuidCommandBytes,
    "Explicit Mach-O byte offsets agree with the actual declaring SDK");
static_assert(sizeof(CC_LONG) == sizeof(std::uint32_t), "Checked CommonCrypto length domain");
static_assert(MaximumNameBytes >= PATH_MAX, "Canonical Apple path buffer is bounded correctly");

struct Source {
    void* context;
    std::uint64_t bytes;
    bool (*read)(void*, std::uint64_t, void*, std::size_t);
};
struct Span {
    const unsigned char* data;
    std::size_t bytes;
};
struct File {
    int descriptor;
    bool ioError;
};

bool Range(std::uint64_t offset, std::uint64_t bytes, std::uint64_t length) noexcept {
    return offset <= length && bytes <= length - offset;
}

std::uint32_t U32(const unsigned char* bytes) noexcept {
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        value |= static_cast<std::uint32_t>(bytes[index]) << (index * CHAR_BIT);
    }
    return value;
}

std::uint64_t U64(const unsigned char* bytes) noexcept {
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        value |= static_cast<std::uint64_t>(bytes[index]) << (index * CHAR_BIT);
    }
    return value;
}

bool Nonzero(const unsigned char* bytes, std::size_t count) noexcept {
    for (std::size_t index = 0; index < count; ++index) {
        if (bytes[index]) {
            return true;
        }
    }
    return false;
}

bool Name(const unsigned char* bytes, const char* expected) noexcept {
    constexpr std::size_t MachNameBytes = 16;
    const std::size_t count = std::strlen(expected);
    if (count >= MachNameBytes || std::memcmp(bytes, expected, count) != 0) {
        return false;
    }
    for (std::size_t index = count; index < MachNameBytes; ++index) {
        if (bytes[index] != 0) {
            return false;
        }
    }
    return true;
}

bool SpanRead(void* context, std::uint64_t offset, void* output, std::size_t bytes) noexcept {
    const auto& span = *static_cast<Span*>(context);
    if (!Range(offset, bytes, span.bytes)) {
        return false;
    }
    std::memcpy(output, span.data + static_cast<std::size_t>(offset), bytes);
    return true;
}

bool FileRead(void* context, std::uint64_t offset, void* output, std::size_t bytes) noexcept {
    auto& file = *static_cast<File*>(context);
    auto* destination = static_cast<unsigned char*>(output);
    int interruptions = 0;
    while (bytes) {
        if (offset > static_cast<std::uint64_t>((std::numeric_limits<off_t>::max)())) {
            file.ioError = true;
            return false;
        }
        const ssize_t read = pread(file.descriptor, destination, bytes, static_cast<off_t>(offset));
        if (read < 0) {
            if (errno == EINTR && ++interruptions <= MaximumReadInterruptions) {
                continue;
            }
            file.ioError = true;
            return false;
        }
        if (read == 0) {
            return false;
        }
        const std::size_t received = static_cast<std::size_t>(read);
        if (received > bytes) {
            file.ioError = true;
            return false;
        }
        destination += received;
        offset += received;
        bytes -= received;
    }
    return true;
}

bool Read(const Source& source, std::uint64_t offset, void* output, std::size_t bytes) noexcept {
    return Range(offset, bytes, source.bytes) && source.read(source.context, offset, output, bytes);
}

bool Hash(const void* bytes, std::size_t length, metadata::Digest& output) noexcept {
    return length <= (std::numeric_limits<CC_LONG>::max)()
        && CC_SHA256(bytes, static_cast<CC_LONG>(length), output.bytes) != nullptr;
}

Status Catalog(const cse::abi::Descriptor& value, std::uint32_t cpu) noexcept {
    if (cpu != Arm64Cpu && cpu != X64Cpu) {
        return Status::Cpu;
    }
    const std::uint32_t architecture = cpu == Arm64Cpu
        ? static_cast<std::uint32_t>(cse::abi::Architecture::Arm64)
        : static_cast<std::uint32_t>(cse::abi::Architecture::X64);
    if (value.descriptorBytes != sizeof(value) || value.descriptorVersion != cse::abi::DescriptorVersion
        || value.runtimeVersion != cse::abi::RuntimeVersion) {
        return Status::Version;
    }
    constexpr std::uint32_t PointerBits = 64;
    if (value.architecture != architecture || value.pointerBits != PointerBits
        || value.compilerAbi != static_cast<std::uint32_t>(cse::abi::CompilerAbi::Itanium)) {
        return Status::Cpu;
    }
    if (value.reserved || value.tableCount != metadata::CatalogTables || value.tableCount > cse::abi::MaxTables) {
        return Status::Tables;
    }
    bool role = false;
    bool profile = false;
    bool app = false;
    for (std::size_t index = 0; index < cse::abi::MaxTables; ++index) {
        const auto& table = value.tables[index];
        if (table.reserved) {
            return Status::Tables;
        }
        if (index >= value.tableCount) {
            if (table.identifier || table.signature) {
                return Status::Tables;
            }
            continue;
        }
        if (!table.signature) {
            return Status::Tables;
        }
        bool* found = nullptr;
        if (table.identifier == metadata::RoleFamily) {
            found = &role;
        } else if (table.identifier == metadata::CppProfileFamily) {
            found = &profile;
        } else if (table.identifier == metadata::AppColdFamily) {
            found = &app;
        } else {
            return Status::Tables;
        }
        if (*found) {
            return Status::Duplicate;
        }
        *found = true;
    }
    return role && profile && app ? Status::Ready : Status::Tables;
}

Status Decode(const unsigned char* bytes, std::uint32_t cpu, metadata::Record& output) noexcept {
    if (U64(bytes) != metadata::Magic || U32(bytes + 12) != metadata::RecordBytes) {
        return Status::Metadata;
    }
    if (U32(bytes + 8) != metadata::FormatVersion) {
        return Status::Version;
    }
    const auto kind = U32(bytes + 16);
    if ((kind != static_cast<std::uint32_t>(metadata::Kind::Offer)
        && kind != static_cast<std::uint32_t>(metadata::Kind::Requirement))
        || U32(bytes + 20) != metadata::AppColdFamily) {
        return Status::Role;
    }
    if (U32(bytes + 24) || U32(bytes + 28)
        || !Nonzero(bytes + metadata::ProvenanceOffset, metadata::DigestBytes)) {
        return Status::Metadata;
    }
    metadata::Record result{};
    result.kind = static_cast<metadata::Kind>(kind);
    std::memcpy(result.provenance.bytes, bytes + metadata::ProvenanceOffset, metadata::DigestBytes);
    const auto* descriptor = bytes + metadata::DescriptorOffset;
    auto& catalog = result.catalog;
    catalog.descriptorBytes = U32(descriptor);
    catalog.descriptorVersion = U32(descriptor + 4);
    catalog.runtimeVersion = U32(descriptor + 8);
    catalog.architecture = U32(descriptor + 12);
    catalog.pointerBits = U32(descriptor + 16);
    catalog.compilerAbi = U32(descriptor + 20);
    catalog.tableCount = U32(descriptor + 24);
    catalog.reserved = U32(descriptor + 28);
    constexpr std::size_t TableOffset = 32;
    constexpr std::size_t TableBytes = 16;
    for (std::size_t index = 0; index < cse::abi::MaxTables; ++index) {
        const auto* table = descriptor + TableOffset + index * TableBytes;
        catalog.tables[index] = {U32(table), U32(table + 4), U64(table + 8)};
    }
    const Status valid = Catalog(catalog, cpu);
    if (valid == Status::Ready) {
        output = result;
    }
    return valid;
}

Status DylibName(const Source& source, std::uint64_t command, std::uint32_t bytes,
    char* output) noexcept {
    unsigned char header[DylibCommandBytes]{};
    if (bytes < sizeof(header) || !Read(source, command, header, sizeof(header))) {
        return Status::Truncated;
    }
    const auto nameOffset = U32(header + CommandHeaderBytes);
    if (nameOffset < sizeof(header) || nameOffset >= bytes) {
        return Status::Range;
    }
    const std::size_t available = bytes - nameOffset;
    const std::size_t count = available < MaximumNameBytes ? available : MaximumNameBytes;
    if (!Read(source, command + nameOffset, output, count)) {
        return Status::Truncated;
    }
    if (!output[0] || !std::memchr(output, 0, count)) {
        return Status::Capacity;
    }
    return Status::Ready;
}

Status Parse(const Source& source, std::uint32_t expectedCpu, ImageFacts& output) noexcept {
    if (expectedCpu != Arm64Cpu && expectedCpu != X64Cpu) {
        return Status::InvalidArgument;
    }
    if (source.bytes < MachHeaderBytes) {
        return Status::Truncated;
    }
    if (source.bytes > MaximumImageBytes) {
        return Status::Capacity;
    }
    unsigned char header[MachHeaderBytes]{};
    if (!Read(source, 0, header, sizeof(header))) {
        return Status::Truncated;
    }
    if (U32(header) != MH_MAGIC_64 || (U32(header + 12) != MH_DYLIB && U32(header + 12) != MH_EXECUTE)
        || U32(header + 28) != 0) {
        return Status::Format;
    }
    const std::uint32_t cpu = U32(header + 4);
    const std::uint32_t subtype = U32(header + 8);
    constexpr std::uint32_t X64AllSubtype = 3;
    if (cpu != expectedCpu || (cpu == Arm64Cpu ? subtype != 0 : subtype != X64AllSubtype)) {
        return Status::Cpu;
    }
    const std::uint32_t commands = U32(header + 16);
    const std::uint32_t commandBytes = U32(header + 20);
    if (commands > MaximumLoadCommands || commandBytes > MaximumCommandBytes) {
        return Status::Capacity;
    }
    if (commands > commandBytes / CommandHeaderBytes || !Range(MachHeaderBytes, commandBytes, source.bytes)) {
        return Status::Truncated;
    }
    ImageFacts result{};
    result.fileBytes = source.bytes;
    result.cpu = cpu;
    bool uuidFound = false;
    bool recordFound = false;
    std::uint32_t sections = 0;
    std::uint64_t cursor = MachHeaderBytes;
    const std::uint64_t commandEnd = cursor + commandBytes;
    for (std::uint32_t index = 0; index < commands; ++index) {
        unsigned char command[CommandHeaderBytes]{};
        if (!Range(cursor, sizeof(command), commandEnd) || !Read(source, cursor, command, sizeof(command))) {
            return Status::Truncated;
        }
        const std::uint32_t kind = U32(command);
        const std::uint32_t bytes = U32(command + 4);
        if (bytes < sizeof(command) || bytes % LoadCommandAlignment != 0 || !Range(cursor, bytes, commandEnd)) {
            return Status::Range;
        }
        if (kind == LC_UUID) {
            if (uuidFound) {
                return Status::Duplicate;
            }
            unsigned char uuid[UuidCommandBytes]{};
            if (bytes != sizeof(uuid) || !Read(source, cursor, uuid, sizeof(uuid))) {
                return Status::Uuid;
            }
            std::memcpy(result.uuid, uuid + CommandHeaderBytes, metadata::UuidBytes);
            if (!Nonzero(result.uuid, sizeof(result.uuid))) {
                return Status::Uuid;
            }
            uuidFound = true;
        } else if (kind == LC_SEGMENT_64) {
            unsigned char segment[SegmentBytes]{};
            if (bytes < sizeof(segment) || !Read(source, cursor, segment, sizeof(segment))) {
                return Status::Truncated;
            }
            const std::uint32_t count = U32(segment + 64);
            if (count > MaximumSections - sections || count > (bytes - SegmentBytes) / SectionBytes) {
                return Status::Capacity;
            }
            if (bytes != SegmentBytes + static_cast<std::uint64_t>(count) * SectionBytes
                || !Range(U64(segment + 40), U64(segment + 48), source.bytes)) {
                return Status::Range;
            }
            sections += count;
            for (std::uint32_t section = 0; section < count; ++section) {
                unsigned char value[SectionBytes]{};
                const std::uint64_t location = cursor + SegmentBytes + section * SectionBytes;
                if (!Read(source, location, value, sizeof(value))) {
                    return Status::Truncated;
                }
                if (!Name(value, metadata::SectionName)) {
                    continue;
                }
                if (recordFound) {
                    return Status::Duplicate;
                }
                const std::uint64_t offset = U32(value + 48);
                const std::uint64_t length = U64(value + 40);
                if (!Name(segment + 8, metadata::SegmentName) || !Name(value + 16, metadata::SegmentName)
                    || (U32(value + 64) & SECTION_TYPE) != S_REGULAR || length != metadata::RecordBytes
                    || !Range(offset, length, source.bytes) || offset < U64(segment + 40)
                    || !Range(offset - U64(segment + 40), length, U64(segment + 48))) {
                    return Status::Range;
                }
                unsigned char encoded[metadata::RecordBytes]{};
                if (!Read(source, offset, encoded, sizeof(encoded))) {
                    return Status::Truncated;
                }
                const Status decoded = Decode(encoded, cpu, result.record);
                if (decoded != Status::Ready) {
                    return decoded;
                }
                if (!Hash(encoded, sizeof(encoded), result.recordDigest)) {
                    return Status::Hash;
                }
                result.metadataOffset = offset;
                recordFound = true;
            }
        } else if (kind == LC_ID_DYLIB || kind == LC_LOAD_DYLIB || kind == LC_LOAD_WEAK_DYLIB
            || kind == LC_REEXPORT_DYLIB || kind == LC_LOAD_UPWARD_DYLIB) {
            char name[MaximumNameBytes]{};
            const Status named = DylibName(source, cursor, bytes, name);
            if (named != Status::Ready) {
                return named;
            }
            if (kind == LC_ID_DYLIB) {
                if (result.installName[0]) {
                    return Status::Duplicate;
                }
                std::memcpy(result.installName, name, sizeof(name));
            } else if (kind == LC_LOAD_DYLIB) {
                if (result.dependencyCount == MaximumDependencies) {
                    return Status::Capacity;
                }
                for (std::uint32_t dependency = 0; dependency < result.dependencyCount; ++dependency) {
                    if (std::strcmp(name, result.dependencies[dependency]) == 0) {
                        return Status::Duplicate;
                    }
                }
                std::memcpy(result.dependencies[result.dependencyCount++], name, sizeof(name));
            }
        }
        cursor += bytes;
    }
    if (cursor != commandEnd) {
        return Status::Range;
    }
    if (!uuidFound) {
        return Status::Uuid;
    }
    if (!recordFound) {
        return Status::MissingMetadata;
    }
    if (result.record.kind == metadata::Kind::Offer && !result.installName[0]) {
        return Status::Dependency;
    }
    output = result;
    return Status::Ready;
}

bool SameFile(const struct stat& before, const struct stat& after) noexcept {
    return before.st_dev == after.st_dev && before.st_ino == after.st_ino && before.st_size == after.st_size
        && before.st_mtimespec.tv_sec == after.st_mtimespec.tv_sec
        && before.st_mtimespec.tv_nsec == after.st_mtimespec.tv_nsec
        && before.st_ctimespec.tv_sec == after.st_ctimespec.tv_sec
        && before.st_ctimespec.tv_nsec == after.st_ctimespec.tv_nsec;
}
}

Status ReadMachO(const unsigned char* data, std::size_t bytes, std::uint32_t cpu, ImageFacts& output) noexcept {
    if (!data) {
        return Status::InvalidArgument;
    }
    Span span{data, bytes};
    Source source{&span, bytes, SpanRead};
    ImageFacts result{};
    const Status parsed = Parse(source, cpu, result);
    if (parsed != Status::Ready) {
        return parsed;
    }
    if (!Hash(data, bytes, result.imageDigest)) {
        return Status::Hash;
    }
    output = result;
    return Status::Ready;
}

Status ReadPinnedFile(const char* path, const ImagePinV1& pin, ImageFacts& output) noexcept {
    if (!path || path[0] != '/' || strnlen(path, MaximumNameBytes) == MaximumNameBytes
        || (pin.kind != metadata::Kind::Offer && pin.kind != metadata::Kind::Requirement)) {
        return Status::InvalidArgument;
    }
    char canonical[MaximumNameBytes]{};
    if (!realpath(path, canonical)) {
        return Status::Io;
    }
    const int descriptor = open(canonical, O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) {
        return Status::Io;
    }
    File file{descriptor, false};
    struct stat before{};
    struct stat after{};
    ImageFacts result{};
    Status status = Status::Io;
    if (fstat(descriptor, &before) == 0 && S_ISREG(before.st_mode) && before.st_size >= 0) {
        Source source{&file, static_cast<std::uint64_t>(before.st_size), FileRead};
        status = Parse(source, NativeCpu, result);
        if (status == Status::Ready) {
            CC_SHA256_CTX hash{};
            unsigned char chunk[DigestChunkBytes]{};
            bool hashed = CC_SHA256_Init(&hash) == 1;
            for (std::uint64_t offset = 0; hashed && offset < source.bytes;) {
                const std::uint64_t remaining = source.bytes - offset;
                const std::size_t count = remaining < sizeof(chunk) ? static_cast<std::size_t>(remaining)
                    : sizeof(chunk);
                hashed = Read(source, offset, chunk, count)
                    && CC_SHA256_Update(&hash, chunk, static_cast<CC_LONG>(count)) == 1;
                offset += count;
            }
            hashed = hashed && CC_SHA256_Final(result.imageDigest.bytes, &hash) == 1;
            if (!hashed) {
                status = file.ioError ? Status::Io : Status::Truncated;
            } else if (pin.fileBytes != result.fileBytes
                || std::memcmp(pin.uuid, result.uuid, sizeof(pin.uuid)) != 0
                || std::memcmp(pin.imageDigest.bytes, result.imageDigest.bytes, metadata::DigestBytes) != 0
                || std::memcmp(pin.recordDigest.bytes, result.recordDigest.bytes, metadata::DigestBytes) != 0) {
                status = Status::Hash;
            } else if (pin.kind != result.record.kind) {
                status = Status::Role;
            } else if (fstat(descriptor, &after) != 0 || !SameFile(before, after)) {
                status = Status::Changed;
            }
        }
    }
    const int closed = close(descriptor);
    if (file.ioError || closed != 0) {
        status = Status::Io;
    }
    if (status == Status::Ready) {
        std::memcpy(result.canonicalPath, canonical, sizeof(canonical));
        result.pinned = true;
        output = result;
    }
    return status;
}

Status CheckCatalogPair(const ImageFacts& requirement, const ImageFacts& offer) noexcept {
    if (requirement.record.kind != metadata::Kind::Requirement || offer.record.kind != metadata::Kind::Offer) {
        return Status::Role;
    }
    if (requirement.cpu != offer.cpu || Catalog(requirement.record.catalog, requirement.cpu) != Status::Ready
        || Catalog(offer.record.catalog, offer.cpu) != Status::Ready) {
        return Status::Contract;
    }
    if (cse::abi::CheckRequired(requirement.record.catalog, &offer.record.catalog)
        != cse::abi::Compatibility::Compatible) {
        return Status::Contract;
    }
    bool linked = false;
    if (requirement.dependencyCount > MaximumDependencies || !offer.installName[0]
        || strnlen(offer.installName, MaximumNameBytes) == MaximumNameBytes) {
        return Status::Dependency;
    }
    for (std::uint32_t index = 0; index < requirement.dependencyCount; ++index) {
        if (strnlen(requirement.dependencies[index], MaximumNameBytes) == MaximumNameBytes) {
            return Status::Dependency;
        }
        if (std::strcmp(requirement.dependencies[index], offer.installName) == 0) {
            linked = true;
        }
    }
    if (!linked) {
        return Status::Dependency;
    }
    if (!requirement.pinned || !offer.pinned) {
        return Status::CatalogCompatible;
    }
    if (offer.installName[0] != '/') {
        return Status::ResolutionUnsupported;
    }
    if (strnlen(offer.canonicalPath, MaximumNameBytes) == MaximumNameBytes) {
        return Status::Dependency;
    }
    return std::strcmp(offer.installName, offer.canonicalPath) == 0
        ? Status::PinnedPairCompatible : Status::Dependency;
}

const char* StatusName(Status status) noexcept {
    switch (status) {
    case Status::Ready: return "ready";
    case Status::CatalogCompatible: return "catalog-compatible";
    case Status::PinnedPairCompatible: return "pinned-pair-compatible";
    case Status::InvalidArgument: return "invalid-argument";
    case Status::Truncated: return "truncated";
    case Status::Format: return "unsupported-format";
    case Status::Range: return "invalid-range";
    case Status::Cpu: return "foreign-cpu-or-profile";
    case Status::Uuid: return "missing-or-invalid-uuid";
    case Status::MissingMetadata: return "missing-metadata";
    case Status::Duplicate: return "duplicate";
    case Status::Metadata: return "invalid-metadata";
    case Status::Role: return "wrong-role";
    case Status::Version: return "wrong-version";
    case Status::Tables: return "invalid-tables";
    case Status::Hash: return "image-pin-mismatch";
    case Status::Io: return "file-io-failure";
    case Status::Changed: return "file-changed";
    case Status::Capacity: return "unsupported-capacity";
    case Status::Dependency: return "dependency-mismatch";
    case Status::ResolutionUnsupported: return "loader-resolution-unqualified";
    case Status::Contract: return "contract-mismatch";
    }
    return "unknown-status";
}
}
