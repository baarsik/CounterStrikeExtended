#ifndef CSE_GAME_LIBRARY_NAME_H
#define CSE_GAME_LIBRARY_NAME_H

#include <cstddef>
#include <cstring>
#include <array>

namespace cse {
enum class GameLibraryPlatform {
    MacOS,
    Linux
};

constexpr std::size_t GameLibraryValueBytes = 256; // Retained liblist value capacity, including its terminator.

template<std::size_t Limit, std::size_t NameBytes>
class GameLibraryFallbacks {
public:
    static_assert(Limit > 0 && NameBytes > 0, "Game library fallback bounds");

    bool Remember(const char* name) noexcept {
        if (!name) {
            return false;
        }
        std::size_t length = 0;
        while (length < NameBytes && name[length] != 0) {
            ++length;
        }
        if (length == NameBytes) {
            return false;
        }
        if (count_ == names_.size()) {
            overflowed_ = true;
            return false;
        }
        std::memcpy(names_[count_].data(), name, length + 1);
        ++count_;
        return true;
    }

    void MarkPrimary() noexcept {
        primary_ = true;
    }

    bool UseFallbacks() const noexcept {
        return !primary_;
    }

    std::size_t Count() const noexcept {
        return count_;
    }

    const char* Name(std::size_t index) const noexcept {
        return index < count_ ? names_[index].data() : nullptr;
    }

    bool Overflowed() const noexcept {
        return overflowed_;
    }

private:
    std::array<std::array<char, NameBytes>, Limit> names_{};
    std::size_t count_ = 0;
    bool primary_ = false;
    bool overflowed_ = false;
};

constexpr const char* GameLibraryMetadataKey(GameLibraryPlatform platform) noexcept {
    return platform == GameLibraryPlatform::MacOS ? "gamedll_osx" : "gamedll_linux";
}

constexpr GameLibraryPlatform NativeGameLibraryPlatform() noexcept {
#if defined(__APPLE__)
    return GameLibraryPlatform::MacOS;
#else
    return GameLibraryPlatform::Linux;
#endif
}

namespace game_library_detail {
inline char LowerAscii(char value) noexcept {
    constexpr int kCaseOffset = 'a' - 'A';
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + kCaseOffset) : value;
}

inline bool EndsWith(const char* name, std::size_t length, const char* suffix) noexcept {
    const std::size_t suffixLength = std::strlen(suffix);
    if (length < suffixLength) {
        return false;
    }
    for (std::size_t index = 0; index < suffixLength; ++index) {
        if (LowerAscii(name[length - suffixLength + index]) != suffix[index]) {
            return false;
        }
    }
    return true;
}
}

// This is filename adaptation only. The caller retains the existing path policy
// and asks the virtual filesystem to resolve the resulting game-relative name.
inline bool NormalizeGameLibraryName(char* name, std::size_t capacity, GameLibraryPlatform platform,
    bool legacyLinuxSuffix) noexcept {
    if (!name || capacity == 0
        || (platform != GameLibraryPlatform::MacOS && platform != GameLibraryPlatform::Linux)) {
        return false;
    }
    std::size_t length = 0;
    while (length < capacity && name[length] != 0) {
        ++length;
    }
    if (length == capacity) {
        return false;
    }
    const char* extension = platform == GameLibraryPlatform::MacOS ? ".dylib" : ".so";
    const std::size_t extensionLength = std::strlen(extension);
    std::size_t prefixLength = length;
    if (platform == GameLibraryPlatform::Linux && legacyLinuxSuffix) {
        for (std::size_t index = 0; index < length; ++index) {
            if (name[index] == '_') {
                // Retain the existing Linux liblist suffix rule, including its
                // first-underscore selection. It is never applied to macOS.
                prefixLength = index;
                break;
            }
        }
    }
    if (prefixLength == length) {
        if (game_library_detail::EndsWith(name, length, extension)) {
            prefixLength -= extensionLength;
        } else if (game_library_detail::EndsWith(name, length, ".dll")) {
            constexpr std::size_t kWindowsExtensionBytes = sizeof(".dll") - 1;
            prefixLength -= kWindowsExtensionBytes;
        } else {
            return false;
        }
    }
    if (extensionLength > capacity - 1 - prefixLength) {
        return false;
    }
    for (std::size_t index = 0; index < prefixLength; ++index) {
        if (name[index] == '\\') {
            name[index] = '/';
        }
    }
    std::memcpy(name + prefixLength, extension, extensionLength + 1);
    return true;
}
}

#endif
