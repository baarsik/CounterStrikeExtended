#if defined(CSE_NATIVE_RUNTIME_BOOTSTRAP)
#include "MacRuntimeSession.h"
#include "../../public/native_module.h"
#include "cse_native_cohort_images.h"
#include <CommonCrypto/CommonDigest.h>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <limits>
#include <sys/stat.h>
#include <unistd.h>

namespace cse_mac_runtime {
namespace {
std::uint64_t nextEpoch = 0;
constexpr off_t MaximumImageBytes = 512 * 1024 * 1024;
constexpr std::size_t HashChunkBytes = 64 * 1024;
constexpr std::size_t HexCharactersPerByte = 2;

bool CohortImageName(const char* path) noexcept {
    if (!path) {
        return false;
    }
    const char* separator = std::strrchr(path, '/');
    const char* leaf = separator ? separator + 1 : path;
    return std::strcmp(leaf, "hw.dylib") == 0 || std::strcmp(leaf, "swds.dylib") == 0
        || std::strcmp(leaf, "vgui.dylib") == 0;
}

bool CohortInstallName(const mach_header* header) noexcept {
    // All loaded images in this native Apple Silicon process have 64-bit
    // Mach-O headers. Malformed command bounds cannot establish cold admission.
    if (!header || header->magic != MH_MAGIC_64) {
        return true;
    }
    const auto* native = reinterpret_cast<const mach_header_64*>(header);
    const auto* cursor = reinterpret_cast<const unsigned char*>(native + 1);
    std::size_t remaining = native->sizeofcmds;
    for (std::uint32_t index = 0; index < native->ncmds; ++index) {
        if (remaining < sizeof(load_command)) {
            return true;
        }
        const auto* command = reinterpret_cast<const load_command*>(cursor);
        if (command->cmdsize < sizeof(load_command) || command->cmdsize > remaining) {
            return true;
        }
        if (command->cmd == LC_ID_DYLIB) {
            if (command->cmdsize < sizeof(dylib_command)) {
                return true;
            }
            const auto* library = reinterpret_cast<const dylib_command*>(cursor);
            const std::size_t offset = library->dylib.name.offset;
            if (offset < sizeof(dylib_command) || offset >= command->cmdsize) {
                return true;
            }
            const auto* name = reinterpret_cast<const char*>(cursor + offset);
            if (!std::memchr(name, '\0', command->cmdsize - offset)) {
                return true;
            }
            if (CohortImageName(name)) {
                return true;
            }
        }
        remaining -= command->cmdsize;
        cursor += command->cmdsize;
    }
    return false;
}

bool CohortAlreadyLoaded() noexcept {
    // A disk hash cannot attest an existing mapping. dyld may reuse a renamed
    // or aliased provider by install name, so inspect its mapped LC_ID_DYLIB as
    // well as its loaded path before any owned root constructor can execute.
    const std::uint32_t count = _dyld_image_count();
    for (std::uint32_t index = 0; index < count; ++index) {
        if (CohortImageName(_dyld_get_image_name(index)) || CohortInstallName(_dyld_get_image_header(index))) {
            return true;
        }
    }
    return false;
}

bool MatchesCohort(const char* path, const char* expected) noexcept {
    if (!path || path[0] != '/' || !expected) {
        return false;
    }
    const int file = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (file < 0) {
        return false;
    }
    struct stat before{};
    bool valid = fstat(file, &before) == 0 && S_ISREG(before.st_mode)
        && before.st_size > 0 && before.st_size <= MaximumImageBytes;
    CC_SHA256_CTX context{};
    valid = valid && CC_SHA256_Init(&context) == 1;
    std::array<unsigned char, HashChunkBytes> bytes{};
    off_t total = 0;
    while (valid) {
        const ssize_t count = read(file, bytes.data(), bytes.size());
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            valid = count == 0;
            break;
        }
        total += count;
        valid = total <= before.st_size && CC_SHA256_Update(&context, bytes.data(), static_cast<CC_LONG>(count)) == 1;
    }
    struct stat after{};
    valid = valid && total == before.st_size && fstat(file, &after) == 0
        && before.st_dev == after.st_dev && before.st_ino == after.st_ino && before.st_size == after.st_size
        && before.st_mtimespec.tv_sec == after.st_mtimespec.tv_sec
        && before.st_mtimespec.tv_nsec == after.st_mtimespec.tv_nsec;
    valid = close(file) == 0 && valid;
    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> digest{};
    valid = valid && CC_SHA256_Final(digest.data(), &context) == 1;
    char hex[CC_SHA256_DIGEST_LENGTH * HexCharactersPerByte + 1]{};
    if (valid) {
        constexpr char Digits[] = "0123456789abcdef";
        constexpr unsigned int NibbleBits = 4;
        constexpr unsigned int NibbleMask = 0xf;
        for (std::size_t index = 0; index < digest.size(); ++index) {
            hex[index * HexCharactersPerByte] = Digits[digest[index] >> NibbleBits];
            hex[index * HexCharactersPerByte + 1] = Digits[digest[index] & NibbleMask];
        }
    }
    return valid && std::strcmp(hex, expected) == 0;
}
}

Session::~Session() noexcept {
    if (active_ || entered_ || software_.Active()) {
        std::fputs("Native runtime session exited without quiescent root teardown\n", stderr);
        std::_Exit(EXIT_FAILURE);
    }
    if (fontPreload_) {
        dlclose(fontPreload_);
    }
}

bool Session::Available(EngineImage image) noexcept {
    switch (image) {
    case EngineImage::Hardware:
        return cse_native_cohort::EngineSha256[0] != '\0';
    case EngineImage::Software:
        return cse_native_cohort::SoftwareEngineSha256[0] != '\0';
    }
    return false;
}

bool Session::Admit(const char* enginePath, const char* fontPath, EngineImage image) noexcept {
    if (!Available(image) || CohortAlreadyLoaded()) {
        return false;
    }
    const char* expected = image == EngineImage::Software ? cse_native_cohort::SoftwareEngineSha256
        : cse_native_cohort::EngineSha256;
    return MatchesCohort(enginePath, expected)
        && MatchesCohort(fontPath, cse_native_cohort::VguiSha256);
}

void* Session::LoadAdmitted(const char* enginePath, const char* fontPath, EngineImage image,
                             void*& ownedFont) noexcept {
    if (ownedFont || !Admit(enginePath, fontPath, image)) {
        return nullptr;
    }
    const bool software = image == EngineImage::Software;
    if (software) {
        // Resolve the font provider's weak allocator edges before the parent is in dyld's load group.
        ownedFont = dlopen(fontPath, RTLD_NOW | RTLD_LOCAL);
        if (!ownedFont) {
            return nullptr;
        }
    }
    void* engine = dlopen(enginePath, RTLD_NOW | (software ? RTLD_LOCAL : RTLD_GLOBAL));
    if (!engine && ownedFont) {
        dlclose(ownedFont);
        ownedFont = nullptr;
    }
    return engine;
}

void* Session::Load(const char* enginePath, const char* fontPath, EngineImage image) noexcept {
    if (active_ || entered_ || software_.Active()) {
        return nullptr;
    }
    return LoadAdmitted(enginePath, fontPath, image, fontPreload_);
}

bool Session::Open(void* image, const char* enginePath, const char* fontPath, bool& adopted) noexcept {
    adopted = false;
    if (active_ || !image || nextEpoch == (std::numeric_limits<std::uint64_t>::max)()) {
        return false;
    }
    const auto open = reinterpret_cast<cse::native_runtime::Open>(
        cse_platform::GetSymbol(image, cse::native_runtime::OpenSymbol));
    close_ = reinterpret_cast<cse::native_runtime::Close>(
        cse_platform::GetSymbol(image, cse::native_runtime::CloseSymbol));
    if (!open || !close_) {
        return false;
    }
    const std::uint64_t epoch = ++nextEpoch;
    const cse_vgui1_host::AdmittedImageV1 admitted{sizeof(admitted), cse_vgui1_host::AdmittedImageVersion,
        epoch, image, enginePath};
    const auto owned = owner_.Open(admitted, cse_vgui1_host::AnchorMode::AdoptInitialReference, image);
    if (owned != cse_vgui1_host::Result::Ready) {
        if (!Close()) {
            std::_Exit(EXIT_FAILURE);
        }
        return false;
    }
    active_ = true;
    adopted = true;
    const char* leaf = std::strrchr(enginePath, '/');
    softwareSelected_ = leaf && std::strcmp(leaf + 1, "swds.dylib") == 0;
    if (softwareSelected_ && !software_.Open(image, enginePath, cse::software_lifetime::Mode::Client)) {
        return false;
    }
    entered_ = owner_.EnterRoot() == cse_vgui1_host::Result::Ready;
    const cse::native_runtime::BindingV1 binding{sizeof(binding), cse::native_runtime::BindingVersion,
        epoch, enginePath, fontPath, owner_.Contract()};
    const int result = [&]() {
        cse::software_lifetime::ImageOwner::Call call(SoftwareOwner());
        return call && entered_ ? open(&binding) : 0;
    }();
    if (result != cse::native_runtime::Ready) {
        std::fprintf(stderr, "Native engine root binding failed (%d)\n", result);
        if (!Close()) {
            std::_Exit(EXIT_FAILURE);
        }
        return false;
    }
    return true;
}

bool Session::Close() noexcept {
    // This gate closes real whole-module owners first, including the separate embedded-font runtime.
    // The host root lease remains live until after the provider has returned successfully.
    if (!software_.Retire()) {
        return false;
    }
    if (entered_) {
        const int result = close_();
        if (result != cse::native_runtime::Closed) {
            std::fprintf(stderr, "Native engine root remains active (%d); unload refused\n", result);
            return false;
        }
        if (owner_.LeaveRoot() != cse_vgui1_host::Result::Ready) {
            return false;
        }
        entered_ = false;
    }
    if (owner_.Close() != cse_vgui1_host::Result::Closed) {
        std::fputs("Native main-image root reference remains owned; unload refused\n", stderr);
        return false;
    }
    active_ = false;
    close_ = nullptr;
    if (fontPreload_) {
        if (dlclose(fontPreload_) != 0) {
            return false;
        }
        fontPreload_ = nullptr;
    }
    softwareSelected_ = false;
    return true;
}

} // namespace cse_mac_runtime

#endif // CSE_NATIVE_RUNTIME_BOOTSTRAP
