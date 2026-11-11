#pragma once

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

// Private implementation owned by host_cmd.cpp; all IO uses the engine filesystem leaves.
namespace cse::save_archive {
constexpr std::uint32_t kLegacyNameBytes = 260;
constexpr std::uint32_t kPriorNativeNameBytes = 1024;
constexpr std::uint32_t kLengthBytes = 4;

struct Layout {
    std::uint32_t start;
    std::uint32_t end;
    std::int32_t fileCount;
    std::uint32_t nameBytes;
};

namespace {
constexpr std::uint32_t kMaximumSeek = INT_MAX; // FS_Seek accepts a signed int absolute position.
constexpr unsigned int kByteBits = 8;
constexpr std::size_t kCopyBytes = 4096;
constexpr std::size_t kPathBytes = MAX_PATH + kPriorNativeNameBytes;

enum class Validation {
    Invalid,
    Valid,
    IoFailure
};

bool Seek(FileHandle_t file, std::uint32_t position) {
    if (!file || position > kMaximumSeek) {
        return false;
    }
    FS_Seek(file, static_cast<int>(position), FILESYSTEM_SEEK_HEAD);
    return FS_Tell(file) == position;
}

bool ReadExact(FileHandle_t file, void *output, std::uint32_t bytes) {
    const auto start = FS_Tell(file);
    if (bytes > kMaximumSeek || start > kMaximumSeek - bytes) {
        return false;
    }
    return bytes == 0 || (FS_Read(output, static_cast<int>(bytes), 1, file) == static_cast<int>(bytes)
        && FS_Tell(file) == start + bytes);
}

bool ValidName(const char *name, std::size_t bytes, std::size_t &length) {
    length = 0;
    while (length < bytes && name[length] != '\0') {
        ++length;
    }
    constexpr std::size_t kExtensionBytes = 4; // A dot followed by the retained *.HL? map-state extension.
    if (length == bytes || length <= kExtensionBytes || name[length - kExtensionBytes] != '.'
        || name[length - kExtensionBytes + 1] != 'H' || name[length - kExtensionBytes + 2] != 'L'
        || name[length - 1] == ' ' || name[length - 1] == '.') {
        return false;
    }
    for (std::size_t index = 0; index < length; ++index) {
        const auto character = static_cast<unsigned char>(name[index]);
        constexpr unsigned char kFirstPrintable = 32;
        constexpr unsigned char kDeleteCharacter = 127;
        if (character < kFirstPrintable || character == kDeleteCharacter
            || std::strchr("/\\:~<>\"|?*", character)
            || (character == '.' && index != 0 && name[index - 1] == '.')) {
            return false;
        }
    }
    return true;
}

bool MakePath(const char *name, char (&path)[kPathBytes]) {
    const int written = std::snprintf(path, sizeof(path), "%s%s", Host_SaveGameDirectory(), name);
    if (written < 0 || static_cast<std::size_t>(written) >= sizeof(path)) {
        return false;
    }
    COM_FixSlashes(path);
    return true;
}

bool ReadRecord(FileHandle_t file, std::uint32_t width, char (&name)[kPriorNativeNameBytes],
    std::uint32_t &length) {
    unsigned char bytes[kLengthBytes]{};
    if (!ReadExact(file, name, width) || !ReadExact(file, bytes, sizeof(bytes))) {
        return false;
    }
    length = 0;
    for (unsigned int index = 0; index < sizeof(bytes); ++index) {
        length |= static_cast<std::uint32_t>(bytes[index]) << (index * kByteBits);
    }
    return true;
}

Validation Validate(FileHandle_t file, const Layout &layout) {
    if (!Seek(file, layout.start)) {
        return Validation::IoFailure;
    }
    const auto recordBytes = layout.nameBytes + kLengthBytes;
    if (static_cast<std::uint32_t>(layout.fileCount) > (layout.end - layout.start) / recordBytes) {
        return Validation::Invalid;
    }
    for (std::int32_t index = 0; index < layout.fileCount; ++index) {
        const auto start = FS_Tell(file);
        if (start > layout.end || recordBytes > layout.end - start) {
            return Validation::Invalid;
        }
        char name[kPriorNativeNameBytes]{};
        std::uint32_t length = 0;
        if (!ReadRecord(file, layout.nameBytes, name, length)) {
            return Validation::IoFailure;
        }
        std::size_t nameLength = 0;
        char path[kPathBytes]{};
        if (!ValidName(name, layout.nameBytes, nameLength) || !MakePath(name, path)
            || length > kMaximumSeek || length > layout.end - FS_Tell(file)) {
            return Validation::Invalid;
        }
        if (!Seek(file, FS_Tell(file) + length)) {
            return Validation::IoFailure;
        }
    }
    return FS_Tell(file) == layout.end ? Validation::Valid : Validation::Invalid;
}

bool Copy(FileHandle_t output, FileHandle_t input, std::uint32_t bytes);
} // namespace

// Also used by SaveGameSlot for its existing envelope: byte counts and cursor progress must agree.
bool WriteExact(FileHandle_t file, const void *input, std::uint32_t bytes) {
    if (!file) {
        return false;
    }
    const auto start = FS_Tell(file);
    if (bytes > kMaximumSeek || start > kMaximumSeek - bytes) {
        return false;
    }
    return bytes == 0 || (FS_Write(input, static_cast<int>(bytes), 1, file) == static_cast<int>(bytes)
        && FS_Tell(file) == start + bytes);
}

namespace {
bool Copy(FileHandle_t output, FileHandle_t input, std::uint32_t bytes) {
    unsigned char buffer[kCopyBytes];
    while (bytes != 0) {
        const auto chunk = bytes > sizeof(buffer) ? static_cast<std::uint32_t>(sizeof(buffer)) : bytes;
        if (!ReadExact(input, buffer, chunk) || !WriteExact(output, buffer, chunk)) {
            return false;
        }
        bytes -= chunk;
    }
    return true;
}
} // namespace

bool ReadLayout(FileHandle_t file, std::int32_t fileCount, Layout &layout) {
    layout = {};
    if (!file || fileCount < 0) {
        return false;
    }
    const auto start = FS_Tell(file);
    const auto end = FS_Size(file);
    if (start > end || end > kMaximumSeek) {
        return false;
    }
    Layout legacy{start, end, fileCount, kLegacyNameBytes};
    if (fileCount == 0) {
        if (start != end || !Seek(file, start)) {
            return false;
        }
        layout = legacy;
        return true;
    }
    Layout native{start, end, fileCount, kPriorNativeNameBytes};
    const auto legacyResult = Validate(file, legacy);
    const auto nativeResult = legacyResult == Validation::IoFailure ? Validation::IoFailure : Validate(file, native);
    const bool restored = Seek(file, start);
    if (!restored || legacyResult == Validation::IoFailure || nativeResult == Validation::IoFailure
        || (legacyResult == Validation::Valid) == (nativeResult == Validation::Valid)) {
        return false;
    }
    layout = legacyResult == Validation::Valid ? legacy : native;
    return true;
}

bool Extract(FileHandle_t file, const Layout &layout) {
    if (!file || layout.fileCount < 0 || layout.start > layout.end || layout.end != FS_Size(file)
        || layout.end > kMaximumSeek
        || (layout.nameBytes != kLegacyNameBytes && layout.nameBytes != kPriorNativeNameBytes)
        || !Seek(file, layout.start)) {
        return false;
    }
    Layout checked{};
    if (!ReadLayout(file, layout.fileCount, checked) || checked.start != layout.start || checked.end != layout.end
        || checked.nameBytes != layout.nameBytes) {
        return false;
    }
    for (std::int32_t index = 0; index < layout.fileCount; ++index) {
        char name[kPriorNativeNameBytes]{};
        char path[kPathBytes]{};
        std::uint32_t length = 0;
        std::size_t nameLength = 0;
        if (!ReadRecord(file, layout.nameBytes, name, length) || !ValidName(name, layout.nameBytes, nameLength)
            || !MakePath(name, path) || length > kMaximumSeek || FS_Tell(file) > layout.end
            || length > layout.end - FS_Tell(file)) {
            return false;
        }
        const auto output = FS_OpenPathID(path, "wb", "GAMECONFIG");
        if (!output) {
            return false;
        }
        const bool copied = Copy(output, file, length);
        FS_Close(output);
        if (!copied) {
            return false;
        }
    }
    return FS_Tell(file) == layout.end;
}

bool WriteDirectory(const char *pattern, FileHandle_t file, std::int32_t expectedFileCount) {
    if (!pattern || !file || expectedFileCount < 0) {
        return false;
    }
    char findBuffer[MAX_PATH]{};
    std::int32_t copiedFiles = 0;
    bool success = true;
    for (const char *name = Sys_FindFirst(pattern, findBuffer); name; name = Sys_FindNext(findBuffer)) {
        std::size_t nameLength = 0;
        char path[kPathBytes]{};
        if (copiedFiles == expectedFileCount || !ValidName(name, kLegacyNameBytes, nameLength)
            || !MakePath(name, path)) {
            success = false;
            break;
        }
        const auto input = FS_OpenPathID(path, "rb", "GAMECONFIG");
        if (!input) {
            success = false;
            break;
        }
        const auto length = FS_Size(input);
        char record[kLegacyNameBytes]{};
        std::memcpy(record, name, nameLength);
        unsigned char bytes[kLengthBytes]{};
        for (unsigned int index = 0; index < sizeof(bytes); ++index) {
            bytes[index] = static_cast<unsigned char>(length >> (index * kByteBits));
        }
        success = length <= kMaximumSeek && FS_Tell(input) == 0
            && WriteExact(file, record, sizeof(record)) && WriteExact(file, bytes, sizeof(bytes))
            && Copy(file, input, length) && FS_Tell(input) == length && FS_Size(input) == length;
        FS_Close(input);
        if (!success) {
            break;
        }
        ++copiedFiles;
    }
    Sys_FindClose();
    return success && copiedFiles == expectedFileCount;
}
} // namespace cse::save_archive
