#pragma once
#include <cctype>
#include <cstring>
#include <string>

namespace cse::dedicated {
inline bool BuildStartupCommands(const char* hostname, const char* password, std::string& command) {
    command.clear();
    constexpr std::size_t MaximumSettingBytes = 63;
    constexpr std::size_t MinimumPasswordBytes = 3;
    if (!hostname || !password || std::strlen(hostname) > MaximumSettingBytes ||
        std::strlen(password) > MaximumSettingBytes || std::strlen(password) < MinimumPasswordBytes) {
        return false;
    }
    for (const auto* cursor = reinterpret_cast<const unsigned char*>(hostname); *cursor; ++cursor) {
        if (*cursor < ' ' || *cursor == '"' || *cursor == '\\') {
            return false;
        }
    }
    for (const auto* cursor = reinterpret_cast<const unsigned char*>(password); *cursor; ++cursor) {
        if (!std::isalnum(*cursor)) {
            return false;
        }
    }
    command = "rcon_password \"" + std::string(password) + "\"\nsetmaster enable\nhostname \"" + hostname + "\"\n";
    return true;
}
} // namespace cse::dedicated
