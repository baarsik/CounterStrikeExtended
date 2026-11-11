#pragma once

#include <cstddef>

namespace cse::native_arguments {
// Bound to MAX_NUM_ARGVS by the actual Sys_InitArgv owner. Slot zero is synthetic.
inline constexpr int ArgumentCapacity = 50;
inline constexpr int MaximumEncodedArguments = ArgumentCapacity - 1;
inline constexpr std::size_t CommandCapacity = 1024;

inline bool TokenByte(unsigned char value) noexcept {
    constexpr unsigned char Delete = 127;
    return value >= ' ' && value != Delete;
}

// The native host emits whole-token quotes only; no escaping or response files.
// Validate before any provider callbacks, then Sys_InitArgv can decode in place.
inline bool Validate(const char* command) noexcept {
    if (!command) {
        return false;
    }
    std::size_t length = 0;
    while (length < CommandCapacity && command[length]) {
        ++length;
    }
    if (length == CommandCapacity) {
        return false;
    }
    int arguments = 0;
    const char* cursor = command;
    while (*cursor) {
        if (*cursor == ' ') {
            ++cursor;
            continue;
        }
        if (++arguments > MaximumEncodedArguments) {
            return false;
        }
        const bool quoted = *cursor == '"';
        if (quoted) {
            ++cursor;
        }
        while (*cursor && (quoted ? *cursor != '"' : *cursor != ' ')) {
            if (!TokenByte(static_cast<unsigned char>(*cursor)) || *cursor == '"' || *cursor == '@') {
                return false;
            }
            ++cursor;
        }
        if (quoted) {
            if (*cursor != '"') {
                return false;
            }
            ++cursor;
            if (*cursor && *cursor != ' ') {
                return false;
            }
        }
    }
    return true;
}
} // namespace cse::native_arguments
