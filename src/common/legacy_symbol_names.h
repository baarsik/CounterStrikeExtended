#pragma once

#include <cstddef>
#include <cstring>

namespace cse { namespace symbols {

// MSVC exports keep the established Method@Class canonical spelling.
inline const char *ExportName(const char *name, std::size_t &bytes) {
    if (!name || name[0] != '?') {
        return nullptr;
    }
    const char *methodEnd = std::strchr(name + 1, '@');
    const char *classEnd = methodEnd ? std::strchr(methodEnd + 1, '@') : nullptr;
    if (!classEnd || methodEnd == name + 1) {
        return nullptr;
    }
    bytes = static_cast<std::size_t>(classEnd - (name + 1));
    return name + 1;
}

inline bool CopyExportName(const char *name, char *output, std::size_t capacity) {
    std::size_t bytes = 0;
    const auto *start = ExportName(name, bytes);
    if (!start || !output || bytes >= capacity) {
        return false;
    }
    std::memcpy(output, start, bytes);
    output[bytes] = '\0';
    return true;
}

inline bool ReadComponent(const char *&cursor, const char *end, const char *&text, std::size_t &bytes) {
    constexpr unsigned int DecimalBase = 10;
    bytes = 0;
    bool digitFound = false;
    const auto remaining = static_cast<std::size_t>(end - cursor);
    while (cursor != end && *cursor >= '0' && *cursor <= '9') {
        const auto digit = static_cast<unsigned int>(*cursor - '0');
        if (digit > remaining || bytes > (remaining - digit) / DecimalBase) {
            return false;
        }
        bytes = bytes * DecimalBase + digit;
        digitFound = true;
        ++cursor;
    }
    if (!digitFound || bytes == 0 || bytes > static_cast<std::size_t>(end - cursor)) {
        return false;
    }
    text = cursor;
    cursor += bytes;
    return true;
}

// Decode the same two-component Itanium form used by the legacy lookup bridge.
inline bool CopyItaniumName(const char *name, char *output, std::size_t capacity) {
    if (!name || std::strncmp(name, "_ZN", 3) != 0 || !output) {
        return false;
    }
    const char *cursor = name + 3;
    const char *end = cursor + std::strlen(cursor);
    const char *className = nullptr;
    const char *methodName = nullptr;
    std::size_t classBytes = 0;
    std::size_t methodBytes = 0;
    constexpr std::size_t SeparatorAndTerminatorBytes = 2;
    if (!ReadComponent(cursor, end, className, classBytes) || !ReadComponent(cursor, end, methodName, methodBytes)
        || classBytes > capacity || methodBytes > capacity - classBytes
        || SeparatorAndTerminatorBytes > capacity - classBytes - methodBytes) {
        return false;
    }
    std::memcpy(output, methodName, methodBytes);
    output[methodBytes] = '@';
    std::memcpy(output + methodBytes + 1, className, classBytes);
    output[methodBytes + classBytes + 1] = '\0';
    return true;
}
} } // namespace cse::symbols
