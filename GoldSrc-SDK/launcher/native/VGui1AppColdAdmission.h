#ifndef CSE_VGUI1_APP_COLD_ADMISSION_H
#define CSE_VGUI1_APP_COLD_ADMISSION_H

#include "../../public/cse_vgui1_app_cold_metadata.h"

namespace cse_vgui1_admission {
namespace metadata = cse::vgui1_app_cold_metadata;
constexpr std::uint32_t Arm64Cpu = 0x0100000c;
constexpr std::uint32_t X64Cpu = 0x01000007;
constexpr std::uint32_t NativeCpu = cse::abi::NativeArchitecture
    == static_cast<std::uint32_t>(cse::abi::Architecture::Arm64) ? Arm64Cpu : X64Cpu;
constexpr std::uint64_t MaximumImageBytes = 512ull * 1024 * 1024;
constexpr std::size_t MaximumNameBytes = 1024;
constexpr std::size_t MaximumDependencies = 32;

enum class Status : std::int32_t {
    Ready = 1, CatalogCompatible = 2, PinnedPairCompatible = 3,
    InvalidArgument = -1, Truncated = -2, Format = -3, Range = -4, Cpu = -5,
    Uuid = -6, MissingMetadata = -7, Duplicate = -8, Metadata = -9, Role = -10,
    Version = -11, Tables = -12, Hash = -13, Io = -14, Changed = -15,
    Capacity = -16, Dependency = -17, ResolutionUnsupported = -18, Contract = -19
};

struct ImageFacts {
    std::uint64_t fileBytes;
    std::uint64_t metadataOffset;
    std::uint32_t cpu;
    std::uint32_t dependencyCount;
    unsigned char uuid[metadata::UuidBytes];
    metadata::Digest imageDigest;
    metadata::Digest recordDigest;
    metadata::Record record;
    char installName[MaximumNameBytes];
    char dependencies[MaximumDependencies][MaximumNameBytes];
    char canonicalPath[MaximumNameBytes];
    bool pinned;
};

// The trusted EXTERNAL produced manifest pins a finished immutable image. No
// whole-image hash is embedded into the image itself. Paths are borrowed only
// for a call. FilePins do not replace AppCold compiler evidence or loader policy.
struct ImagePinV1 {
    std::uint64_t fileBytes;
    unsigned char uuid[metadata::UuidBytes];
    metadata::Digest imageDigest;
    metadata::Digest recordDigest;
    metadata::Kind kind;
};

// No dlopen, exported function call, native service or readiness promotion.
// Every failure leaves output untouched. ReadMachO accepts only a bounded thin
// little-endian64 image for the requested supported CPU; pinned reads require
// this executable's native CPU. Immutable regular staged files are mandatory.
Status ReadMachO(const unsigned char*, std::size_t, std::uint32_t expectedCpu, ImageFacts&) noexcept;
Status ReadPinnedFile(const char* absolutePath, const ImagePinV1&, ImageFacts&) noexcept;

// Provenance hashes are NOT compared as ABI signatures. This validates separate
// requirement/offer catalogs and the declared strong dependency. Pinned success
// additionally requires an absolute declared provider path matching its pin.
// @rpath resolution is deliberately not guessed; host loader integration must
// implement/review that separately. Even pinned success is not ColdLoadReady.
Status CheckCatalogPair(const ImageFacts& requirement, const ImageFacts& offer) noexcept;
const char* StatusName(Status) noexcept;
}
#endif
