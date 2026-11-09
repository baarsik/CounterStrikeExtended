#include "cse_vgui1_app_cold_metadata.h"
#include "VGui1AppColdAdmission.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <fcntl.h>
#include <unistd.h>

namespace {
namespace admission = cse_vgui1_admission;
namespace metadata = cse::vgui1_app_cold_metadata;
constexpr std::size_t PayloadOffset = 4096;
constexpr std::size_t HeaderBytes = 32;
constexpr std::size_t SegmentOffset = HeaderBytes + 24;
constexpr std::size_t SectionOffset = SegmentOffset + 72;

void Check(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "FAIL AppCold reader: %s\n", message);
        std::abort();
    }
}

void Put32(std::vector<unsigned char>& bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t index = 0; index < 4; ++index) {
        bytes[offset + index] = static_cast<unsigned char>(value >> (index * 8));
    }
}

void Put64(std::vector<unsigned char>& bytes, std::size_t offset, std::uint64_t value) {
    for (std::size_t index = 0; index < 8; ++index) {
        bytes[offset + index] = static_cast<unsigned char>(value >> (index * 8));
    }
}

std::size_t Dylib(std::vector<unsigned char>& bytes, std::size_t cursor, std::uint32_t kind, const char* name) {
    constexpr std::size_t CommandBytes = 24;
    constexpr std::size_t Alignment = 8;
    const std::size_t size = (CommandBytes + std::strlen(name) + 1 + Alignment - 1) & ~(Alignment - 1);
    Put32(bytes, cursor, kind);
    Put32(bytes, cursor + 4, static_cast<std::uint32_t>(size));
    Put32(bytes, cursor + 8, CommandBytes);
    std::memcpy(bytes.data() + cursor + CommandBytes, name, std::strlen(name));
    return cursor + size;
}

std::vector<unsigned char> Image(metadata::Kind kind, const char* provider = "/owned/provider.dylib",
    bool duplicateSection = false, bool duplicateUuid = false) {
    metadata::Digest provenance{};
    provenance.bytes[0] = 1;
    const auto catalog = cse::abi::Describe({{metadata::RoleFamily, 0, 1},
        {metadata::CppProfileFamily, 0, 2}, {metadata::AppColdFamily, 0, 3}});
    const auto encoded = metadata::Encode(kind, provenance, catalog);
    constexpr unsigned char Framing[]{0x43, 0x53, 0x45, 0x41, 0x43, 0x44, 0x30, 0x31,
        1, 0, 0, 0, 0x60, 1, 0, 0, 2, 0, 0, 0, 6, 2, 0x4d, 0x4f, 0, 0, 0, 0, 0, 0, 0, 0};
    if (kind == metadata::Kind::Requirement) {
        Check(std::memcmp(encoded.bytes, Framing, sizeof(Framing)) == 0, "independent exact framing bytes");
        Check(encoded.bytes[64] == 0x20 && encoded.bytes[65] == 1 && encoded.bytes[72] == 10
            && encoded.bytes[88] == 3, "independent descriptor byte offsets/encoding");
    }
    std::vector<unsigned char> bytes(PayloadOffset + sizeof(encoded));
    Put32(bytes, 0, 0xfeedfacf);
    Put32(bytes, 4, admission::Arm64Cpu);
    Put32(bytes, 12, 6); // MH_DYLIB.
    Put32(bytes, HeaderBytes, 0x1b); // LC_UUID, independently authored byte fields.
    Put32(bytes, HeaderBytes + 4, 24);
    bytes[HeaderBytes + 8] = 1;
    Put32(bytes, SegmentOffset, 0x19); // LC_SEGMENT_64.
    Put32(bytes, SegmentOffset + 4, duplicateSection ? 232 : 152);
    std::memcpy(bytes.data() + SegmentOffset + 8, "__DATA_CONST", 12);
    Put64(bytes, SegmentOffset + 40, PayloadOffset);
    Put64(bytes, SegmentOffset + 48, sizeof(encoded));
    Put32(bytes, SegmentOffset + 64, duplicateSection ? 2 : 1);
    std::memcpy(bytes.data() + SectionOffset, "__cse_vgui1", 11);
    std::memcpy(bytes.data() + SectionOffset + 16, "__DATA_CONST", 12);
    Put64(bytes, SectionOffset + 40, sizeof(encoded));
    Put32(bytes, SectionOffset + 48, PayloadOffset);
    std::size_t cursor = SectionOffset + 80;
    if (duplicateSection) {
        std::memcpy(bytes.data() + cursor, bytes.data() + SectionOffset, 80);
        cursor += 80;
    }
    cursor = Dylib(bytes, cursor, 0xd, kind == metadata::Kind::Offer ? provider : "/owned/root.dylib");
    cursor = Dylib(bytes, cursor, 0xc, provider);
    if (duplicateUuid) {
        std::memcpy(bytes.data() + cursor, bytes.data() + HeaderBytes, 24);
        cursor += 24;
    }
    Put32(bytes, 16, duplicateUuid ? 5 : 4);
    Put32(bytes, 20, static_cast<std::uint32_t>(cursor - HeaderBytes));
    std::memcpy(bytes.data() + PayloadOffset, encoded.bytes, sizeof(encoded));
    return bytes;
}

admission::ImageFacts Parse(const std::vector<unsigned char>& bytes) {
    admission::ImageFacts result{};
    Check(admission::ReadMachO(bytes.data(), bytes.size(), admission::NativeCpu, result) == admission::Status::Ready,
        "authored bounded image parses");
    return result;
}

void Reject(std::vector<unsigned char> bytes, std::size_t offset, std::uint64_t value,
    admission::Status expected, bool wide = false) {
    if (wide) {
        Put64(bytes, offset, value);
    } else {
        Put32(bytes, offset, static_cast<std::uint32_t>(value));
    }
    admission::ImageFacts result{};
    result.fileBytes = UINT64_MAX;
    const auto status = admission::ReadMachO(bytes.data(), bytes.size(), admission::NativeCpu, result);
    if (status != expected) {
        std::fprintf(stderr, "offset=%zu expected=%s actual=%s\n", offset,
            admission::StatusName(expected), admission::StatusName(status));
    }
    Check(status == expected && result.fileBytes == UINT64_MAX, "malformed image refused atomically");
}

void Malformed() {
    const auto image = Image(metadata::Kind::Requirement);
    using S = admission::Status;
    Reject(image, 0, 0xcafebabe, S::Format);
    Reject(image, 0, 0xcffaedfe, S::Format);
    Reject(image, 4, admission::X64Cpu, S::Cpu);
    Reject(image, 8, 2, S::Cpu);
    Reject(image, 28, 1, S::Format);
    Reject(image, 16, 4097, S::Capacity);
    Reject(image, 20, 1024 * 1024 + 1, S::Capacity);
    Reject(image, HeaderBytes + 4, 0, S::Range);
    Reject(image, HeaderBytes + 4, UINT32_MAX, S::Range);
    Reject(image, SegmentOffset + 40, UINT64_MAX, S::Range, true);
    Reject(image, SegmentOffset + 48, UINT64_MAX, S::Range, true);
    Reject(image, SectionOffset + 40, UINT64_MAX, S::Range, true);
    Reject(image, SectionOffset + 48, UINT32_MAX, S::Range);
    Reject(image, SectionOffset + 64, 1, S::Range);
    Reject(image, PayloadOffset + 8, 2, S::Version);
    Reject(image, PayloadOffset + 12, 351, S::Metadata);
    Reject(image, PayloadOffset + 16, 0, S::Role);
    Reject(image, PayloadOffset + 20, 1, S::Role);
    Reject(image, PayloadOffset + 24, 1, S::Metadata);
    Reject(image, PayloadOffset + 32, 0, S::Metadata);
    const std::size_t descriptor = PayloadOffset + metadata::DescriptorOffset;
    Reject(image, descriptor, 287, S::Version);
    Reject(image, descriptor + 4, 2, S::Version);
    Reject(image, descriptor + 8, 11, S::Version);
    Reject(image, descriptor + 12, 2, S::Cpu);
    Reject(image, descriptor + 16, 32, S::Cpu);
    Reject(image, descriptor + 20, 1, S::Cpu);
    Reject(image, descriptor + 24, 17, S::Tables);
    Reject(image, descriptor + 28, 1, S::Tables);
    Reject(image, descriptor + 32 + 4, 1, S::Tables);
    Reject(image, descriptor + 48, metadata::RoleFamily, S::Duplicate);
    Reject(image, descriptor + 48, 1, S::Tables);
    Reject(image, descriptor + 40, 0, S::Tables, true);
    Reject(image, descriptor + 32 + 3 * 16 + 8, 1, S::Tables, true);
    auto oddCommand = image;
    std::size_t last = HeaderBytes;
    for (int index = 0; index < 3; ++index) {
        last += static_cast<std::size_t>(oddCommand[last + 4])
            | (static_cast<std::size_t>(oddCommand[last + 5]) << 8);
    }
    const std::uint32_t lastBytes = oddCommand[last + 4];
    const std::uint32_t totalCommandBytes = oddCommand[20]
        | (static_cast<std::uint32_t>(oddCommand[21]) << 8);
    Put32(oddCommand, last + 4, lastBytes + 1);
    Put32(oddCommand, 20, totalCommandBytes + 1);
    admission::ImageFacts oddOutput{};
    Check(admission::ReadMachO(oddCommand.data(), oddCommand.size(), admission::NativeCpu, oddOutput)
        == S::Range, "misaligned64-bit load command refused before constructor admission");
    admission::ImageFacts output{};
    auto duplicate = Image(metadata::Kind::Requirement, "/owned/provider.dylib", true);
    Check(admission::ReadMachO(duplicate.data(), duplicate.size(), admission::NativeCpu, output) == S::Duplicate,
        "duplicate section refused");
    duplicate = Image(metadata::Kind::Requirement, "/owned/provider.dylib", false, true);
    Check(admission::ReadMachO(duplicate.data(), duplicate.size(), admission::NativeCpu, output) == S::Duplicate,
        "duplicate UUID refused");
    auto missing = image;
    missing[SectionOffset] = 'x';
    Check(admission::ReadMachO(missing.data(), missing.size(), admission::NativeCpu, output) == S::MissingMetadata,
        "missing metadata refused");
    for (std::size_t bytes : {std::size_t{0}, std::size_t{1}, HeaderBytes - 1}) {
        Check(admission::ReadMachO(image.data(), bytes, admission::NativeCpu, output) == S::Truncated,
            "truncated image/header refused");
    }
}

void PinnedFiles() {
    char directory[] = "/tmp/cse-app-cold-reader-XXXXXX";
    Check(mkdtemp(directory) != nullptr, "owned temporary directory");
    char canonical[admission::MaximumNameBytes]{};
    Check(realpath(directory, canonical) != nullptr, "canonical owned directory");
    char rootPath[admission::MaximumNameBytes]{};
    char providerPath[admission::MaximumNameBytes]{};
    Check(std::snprintf(rootPath, sizeof(rootPath), "%s/root.dylib", canonical) > 0
        && std::snprintf(providerPath, sizeof(providerPath), "%s/provider.dylib", canonical) > 0, "owned paths");
    const auto root = Image(metadata::Kind::Requirement, providerPath);
    const auto provider = Image(metadata::Kind::Offer, providerPath);
    auto rootFacts = Parse(root);
    auto providerFacts = Parse(provider);
    for (const auto* path : {rootPath, providerPath}) {
        const auto& bytes = path == rootPath ? root : provider;
        const int file = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
        Check(file >= 0 && write(file, bytes.data(), bytes.size()) == static_cast<ssize_t>(bytes.size())
            && close(file) == 0, "owned exact data file");
    }
    auto pin = [](const admission::ImageFacts& facts) {
        admission::ImagePinV1 value{};
        value.fileBytes = facts.fileBytes;
        std::memcpy(value.uuid, facts.uuid, sizeof(value.uuid));
        value.imageDigest = facts.imageDigest;
        value.recordDigest = facts.recordDigest;
        value.kind = facts.record.kind;
        return value;
    };
    const auto rootPin = pin(rootFacts);
    const auto providerPin = pin(providerFacts);
    Check(admission::CheckCatalogPair(rootFacts, providerFacts) == admission::Status::CatalogCompatible,
        "raw catalog compatibility does not claim file/loader admission");
    Check(admission::ReadPinnedFile(rootPath, rootPin, rootFacts) == admission::Status::Ready
        && admission::ReadPinnedFile(providerPath, providerPin, providerFacts) == admission::Status::Ready,
        "actual streamed file/UUID/record pins match independent span hashing");
    Check(admission::CheckCatalogPair(rootFacts, providerFacts) == admission::Status::PinnedPairCompatible,
        "actual absolute declared dependency matches the pinned provider");
    // Resolve only exact absolute install names. An actual externally pinned
    // pair with matching @rpath declarations is deliberately not admitted.
    const auto relativeRoot = Image(metadata::Kind::Requirement, "@rpath/provider.dylib");
    const auto relativeOffer = Image(metadata::Kind::Offer, "@rpath/provider.dylib");
    for (const auto* path : {rootPath, providerPath}) {
        const auto& bytes = path == rootPath ? relativeRoot : relativeOffer;
        const int file = open(path, O_WRONLY | O_TRUNC);
        Check(file >= 0 && write(file, bytes.data(), bytes.size()) == static_cast<ssize_t>(bytes.size())
            && close(file) == 0, "owned relative-resolution control");
    }
    auto relativeRootFacts = Parse(relativeRoot);
    auto relativeOfferFacts = Parse(relativeOffer);
    Check(admission::ReadPinnedFile(rootPath, pin(relativeRootFacts), relativeRootFacts) == admission::Status::Ready
        && admission::ReadPinnedFile(providerPath, pin(relativeOfferFacts), relativeOfferFacts)
            == admission::Status::Ready, "actual @rpath control file pins");
    Check(admission::CheckCatalogPair(relativeRootFacts, relativeOfferFacts)
        == admission::Status::ResolutionUnsupported, "@rpath resolution is refused explicitly");
    // Restore the initial owned bytes before the original identity mutations.
    for (const auto* path : {rootPath, providerPath}) {
        const auto& bytes = path == rootPath ? root : provider;
        const int file = open(path, O_WRONLY | O_TRUNC);
        Check(file >= 0 && write(file, bytes.data(), bytes.size()) == static_cast<ssize_t>(bytes.size())
            && close(file) == 0, "restore owned identity controls");
    }
    auto differentBytes = provider;
    differentBytes[PayloadOffset + metadata::ProvenanceOffset] ^= 2;
    const auto differentProvenance = Parse(differentBytes);
    Check(admission::CheckCatalogPair(rootFacts, differentProvenance) == admission::Status::CatalogCompatible,
        "provenance does not become an ABI signature");
    auto wrong = rootPin;
    wrong.imageDigest.bytes[0] ^= 1;
    admission::ImageFacts untouched{};
    untouched.fileBytes = UINT64_MAX;
    Check(admission::ReadPinnedFile(rootPath, wrong, untouched) == admission::Status::Hash
        && untouched.fileBytes == UINT64_MAX, "whole-image digest mismatch refused atomically");
    wrong = rootPin;
    wrong.uuid[0] ^= 1;
    Check(admission::ReadPinnedFile(rootPath, wrong, untouched) == admission::Status::Hash, "UUID mismatch");
    wrong = rootPin;
    wrong.recordDigest.bytes[0] ^= 1;
    Check(admission::ReadPinnedFile(rootPath, wrong, untouched) == admission::Status::Hash, "record binding mismatch");
    wrong = rootPin;
    ++wrong.fileBytes;
    Check(admission::ReadPinnedFile(rootPath, wrong, untouched) == admission::Status::Hash, "file size mismatch");
    wrong = rootPin;
    wrong.kind = metadata::Kind::Offer;
    Check(admission::ReadPinnedFile(rootPath, wrong, untouched) == admission::Status::Role, "role mismatch");
    Check(admission::ReadPinnedFile("relative.dylib", rootPin, untouched) == admission::Status::InvalidArgument,
        "no current-directory fallback");
    Check(admission::ReadPinnedFile(directory, rootPin, untouched) == admission::Status::Io,
        "directory is not an image");
    Check(unlink(rootPath) == 0 && unlink(providerPath) == 0 && rmdir(directory) == 0, "owned cleanup");
}
}

int main() {
    admission::ImageFacts output{};
    output.fileBytes = UINT64_MAX;
    Check(admission::ReadMachO(nullptr, 0, admission::NativeCpu, output) == admission::Status::InvalidArgument,
        "null/truncated input rejected before reading bytes");
    Check(output.fileBytes == UINT64_MAX, "failure leaves output untouched");
    Malformed();
    PinnedFiles();
    std::puts("PASS exact framing, bounded malformed corpus, streamed pins, roles and separate ABI provenance");
    return 0;
}
