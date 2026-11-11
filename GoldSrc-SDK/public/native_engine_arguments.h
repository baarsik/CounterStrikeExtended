#pragma once

#include <cstddef>

namespace cse::native_arguments {
// Bound to MAX_NUM_ARGVS by the actual Sys_InitArgv owner. Slot zero is synthetic.
inline constexpr int ArgumentCapacity = 50;
inline constexpr int MaximumEncodedArguments = ArgumentCapacity - 1;
inline constexpr std::size_t CommandCapacity = 1024;
// Cmd_TokenizeString rejects strlen(token) + 1 >= 516.
inline constexpr std::size_t MaximumStartupTokenBytes = 514;
#if defined(__APPLE__) && !defined(_WIN32)
// Module-private provenance survives COM_InitArgv's pointer copy. No exported ABI or heap ownership.
inline bool QuotedArguments[ArgumentCapacity] __attribute__((visibility("hidden"))) = {};
#endif

inline bool StartupBoundary(const char* token, bool quoted) noexcept {
    if (quoted || (token[0] != '+' && token[0] != '-')) {
        return false;
    }
    const unsigned char first = static_cast<unsigned char>(token[1]);
    // Signed numbers and standalone +/- are values, not command/option names.
    return (first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z') || first == '_';
}

inline bool CommandName(const char* begin, std::size_t length) noexcept {
    if (length == 0 || length > MaximumStartupTokenBytes) {
        return false;
    }
    for (std::size_t index = 0; index < length; ++index) {
        const unsigned char byte = static_cast<unsigned char>(begin[index]);
        if (!((byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z')
            || (byte >= '0' && byte <= '9') || byte == '_' || byte == '.')) {
            return false;
        }
    }
    return true;
}

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
    bool startupCommand = false;
    std::size_t startupBytes = 0;
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
        const char* begin = cursor;
        while (*cursor && (quoted ? *cursor != '"' : *cursor != ' ')) {
            if (!TokenByte(static_cast<unsigned char>(*cursor)) || *cursor == '"' || *cursor == '@') {
                return false;
            }
            ++cursor;
        }
        const std::size_t tokenBytes = static_cast<std::size_t>(cursor - begin);
        if (StartupBoundary(begin, quoted) && begin[0] == '+') {
            if (!CommandName(begin + 1, tokenBytes - 1)) {
                return false;
            }
            startupCommand = true;
            startupBytes = tokenBytes - 1;
        } else if (StartupBoundary(begin, quoted) && begin[0] == '-') {
            startupCommand = false;
        } else if (startupCommand) {
            constexpr std::size_t SeparatorAndQuotes = 3;
            startupBytes += SeparatorAndQuotes + tokenBytes;
            if (tokenBytes > MaximumStartupTokenBytes || startupBytes >= CommandCapacity) {
                return false;
            }
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
