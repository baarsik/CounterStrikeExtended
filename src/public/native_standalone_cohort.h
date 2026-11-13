#pragma once
#include <CommonCrypto/CommonDigest.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace cse::standalone_cohort {
inline bool ImageName(const char* path) {
    if (!path) {
        return false;
    }
    const char* separator = std::strrchr(path, '/');
    const char* name = separator ? separator + 1 : path;
    return std::strcmp(name, "vgui2.dylib") == 0 || std::strcmp(name, "filesystem_stdio.dylib") == 0;
}

inline bool AlreadyMapped() {
    for (std::uint32_t index = 0; index < _dyld_image_count(); ++index) {
        if (ImageName(_dyld_get_image_name(index))) {
            return true;
        }
        const auto* header = _dyld_get_image_header(index);
        if (!header || header->magic != MH_MAGIC_64) {
            return true;
        }
        const auto* native = reinterpret_cast<const mach_header_64*>(header);
        const auto* cursor = reinterpret_cast<const unsigned char*>(native + 1);
        std::size_t remaining = native->sizeofcmds;
        for (std::uint32_t commandIndex = 0; commandIndex < native->ncmds; ++commandIndex) {
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
                const auto* library = reinterpret_cast<const dylib_command*>(command);
                const std::size_t offset = library->dylib.name.offset;
                if (offset < sizeof(dylib_command) || offset >= command->cmdsize) {
                    return true;
                }
                const char* name = reinterpret_cast<const char*>(cursor + offset);
                if (!std::memchr(name, '\0', command->cmdsize - offset) || ImageName(name)) {
                    return true;
                }
            }
            remaining -= command->cmdsize;
            cursor += command->cmdsize;
        }
    }
    return false;
}

inline bool Matches(const char* path, const char* expected) {
    constexpr off_t MaximumImageBytes = 512 * 1024 * 1024;
    constexpr std::size_t HexCharactersPerByte = 2;
    if (!path || path[0] != '/' || !expected ||
        std::strlen(expected) != CC_SHA256_DIGEST_LENGTH * HexCharactersPerByte) {
        return false;
    }
    const int file = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (file < 0) {
        return false;
    }
    struct stat before{};
    bool valid = fstat(file, &before) == 0 && S_ISREG(before.st_mode) && before.st_size > 0 &&
                 before.st_size <= MaximumImageBytes;
    const std::size_t length = valid ? static_cast<std::size_t>(before.st_size) : 0;
    void* bytes = valid ? mmap(nullptr, length, PROT_READ, MAP_PRIVATE, file, 0) : MAP_FAILED;
    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> digest{};
    valid = valid && bytes != MAP_FAILED && CC_SHA256(bytes, static_cast<CC_LONG>(length), digest.data());
    if (bytes != MAP_FAILED) {
        munmap(bytes, length);
    }
    struct stat after{};
    valid = valid && fstat(file, &after) == 0 && before.st_dev == after.st_dev && before.st_ino == after.st_ino &&
            before.st_size == after.st_size && before.st_mtimespec.tv_sec == after.st_mtimespec.tv_sec &&
            before.st_mtimespec.tv_nsec == after.st_mtimespec.tv_nsec;
    valid = close(file) == 0 && valid;
    constexpr char Digits[] = "0123456789abcdef";
    char hex[CC_SHA256_DIGEST_LENGTH * HexCharactersPerByte + 1]{};
    constexpr unsigned int NibbleBits = 4;
    constexpr unsigned int NibbleMask = 0xf;
    for (std::size_t index = 0; index < digest.size(); ++index) {
        hex[index * HexCharactersPerByte] = Digits[digest[index] >> NibbleBits];
        hex[index * HexCharactersPerByte + 1] = Digits[digest[index] & NibbleMask];
    }
    return valid && std::strcmp(hex, expected) == 0;
}

inline bool Admit(const char* vgui, const char* filesystem, const char* vguiHash, const char* filesystemHash) {
    return !AlreadyMapped() && Matches(vgui, vguiHash) && Matches(filesystem, filesystemHash);
}
} // namespace cse::standalone_cohort
