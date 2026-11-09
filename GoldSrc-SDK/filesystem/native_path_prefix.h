#pragma once

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

namespace cse::filesystem {

inline char* ResolveSteamContentPrefixForHome(const char* home, char* link, std::size_t capacity) {
    if (!home || !link || capacity == 0) {
        return nullptr;
    }
    std::snprintf(link, capacity, "%s/.steam/steam", home);
    link[capacity - 1] = '\0';
    char* resolved = static_cast<char*>(std::malloc(capacity + 1));
    if (!resolved) {
        return nullptr;
    }
    resolved[0] = '\0';
    if (realpath(link, resolved)) {
        std::strcat(resolved, "/");
    } else {
        resolved[0] = '\0';
    }
    return resolved;
}

inline bool MatchesSteamContentPrefix(const char* path, const char* prefix, std::size_t prefixBytes) {
    return prefixBytes != 0 && strncasecmp(path, prefix, prefixBytes) == 0;
}

}
