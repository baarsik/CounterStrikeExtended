#pragma once

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/stat.h>
#endif

namespace cse {
namespace platform {

inline bool CreateDirectoryPath(const char *path) {
#if defined(_WIN32)
    return CreateDirectoryA(path, nullptr) != FALSE;
#else
    constexpr mode_t DirectoryPermissions = S_IRWXU | S_IRWXG | S_IRWXO;
    return mkdir(path, DirectoryPermissions) == 0;
#endif
}
} // namespace platform
} // namespace cse
