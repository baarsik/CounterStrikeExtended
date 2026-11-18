#pragma once

#include <sys/stat.h>

namespace cse {
namespace console {

inline bool IsNullDevice(int descriptor) {
    // Darwin poll reports POLLNVAL for /dev/null even though reading it returns EOF.
    struct stat input{};
    struct stat nullDevice{};
    return fstat(descriptor, &input) == 0 && S_ISCHR(input.st_mode) &&
        stat("/dev/null", &nullDevice) == 0 && input.st_rdev == nullDevice.st_rdev;
}

} // namespace console
} // namespace cse
