#ifndef CSE_KEYVALUES_DIRECTORIES_H
#define CSE_KEYVALUES_DIRECTORIES_H

#include <cstddef>
#include <cstring>

namespace cse {
constexpr std::size_t KeyValuesParentPathBytes = 1024; // Retained SaveToFile parent-buffer capacity.

template<typename FileSystem>
void PrepareKeyValuesParentDirectory(FileSystem* filesystem, const char* resourceName, const char* pathID) {
    if (!filesystem || !resourceName) {
        return;
    }
    const char* slash = std::strrchr(resourceName, '/');
    const char* backslash = std::strrchr(resourceName, '\\');
    const char* separator = slash && (!backslash || slash > backslash) ? slash : backslash;
    if (!separator || separator == resourceName) {
        return;
    }
    const std::size_t length = static_cast<std::size_t>(separator - resourceName);
    if (length >= KeyValuesParentPathBytes) {
        return;
    }
    char parent[KeyValuesParentPathBytes];
    std::memcpy(parent, resourceName, length);
    parent[length] = 0;
    for (std::size_t index = 0; index < length; ++index) {
        if (parent[index] == '\\') {
            parent[index] = '/';
        }
    }
    // Keep writable-root and path-ID resolution in the owning virtual filesystem.
    filesystem->CreateDirHierarchy(parent, pathID);
}
}

#endif
