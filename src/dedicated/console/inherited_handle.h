#pragma once
#include <cstdint>
#include <limits>
namespace cse::dedicated {
inline bool ParseInheritedHandle(const char* text, std::uintptr_t& result) noexcept {
    constexpr std::uintptr_t DecimalBase = 10;
    constexpr std::uintptr_t InvalidHandle = (std::numeric_limits<std::uintptr_t>::max)();
    if (!text || !*text) {
        return false;
    }
    std::uintptr_t value = 0;
    for (const char* cursor = text; *cursor; ++cursor) {
        if (*cursor < '0' || *cursor > '9') {
            return false;
        }
        const auto digit = static_cast<std::uintptr_t>(*cursor - '0');
        if (value > (InvalidHandle - digit) / DecimalBase) {
            return false;
        }
        value = value * DecimalBase + digit;
    }
    if (value == 0 || value == InvalidHandle) {
        return false;
    }
    result = value;
    return true;
}
} // namespace cse::dedicated
