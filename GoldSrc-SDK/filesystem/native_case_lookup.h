#pragma once

#if defined(__APPLE__)
#include <cerrno>
#include <string>
#include <unistd.h>

namespace cse::filesystem {

inline bool ParentUsesCaseInsensitiveLookup(const char* path) {
    const int savedError = errno;
    const std::string original(path);
    const auto separator = original.find_last_of('/');
    // Preserve dot components, symlinks and mount traversal. A distant existing
    // ancestor cannot establish the lookup rules of a missing immediate parent.
    const std::string parent = separator == std::string::npos ? "." :
        separator == 0 ? "/" : original.substr(0, separator);
    constexpr long CaseInsensitive = 0;
    const long capability = pathconf(parent.c_str(), _PC_CASE_SENSITIVE);
    errno = savedError;
    // -1 (unsupported, inaccessible, missing or otherwise unknown) retains the
    // existing case resolver. Never cache this decision across filesystem changes.
    return capability == CaseInsensitive;
}

}
#endif
