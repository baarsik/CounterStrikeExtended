#pragma once

#include "../public/cse_posix_crt.h"

#include <cstring>
#include <string_view>

namespace cse::maps {

inline const char* FileName(const char* path) {
    const auto separator = std::string_view(path).find_last_of("/\\");
    return separator == std::string_view::npos ? path : path + separator + 1;
}

inline std::string_view BspName(const char* filename) {
    const std::string_view name(filename);
    const auto extension = name.find_last_of('.');
    if (extension == std::string_view::npos || _stricmp(filename + extension, ".bsp") != 0) {
        return {};
    }
    return name.substr(0, extension);
}

} // namespace cse::maps
