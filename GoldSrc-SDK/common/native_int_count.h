#pragma once

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace cse { namespace counts {

// Legacy index/RNG and save-buffer interfaces retain signed 32-bit counts.
inline bool TryIntCount(std::size_t value, int &result) {
    static_assert(sizeof(int) == 4, "Legacy index counts use signed 32-bit integers");
    if (value > INT_MAX) {
        return false;
    }
    result = static_cast<int>(value);
    return true;
}

inline int IntCount(std::size_t value) {
    int result = 0;
    if (!TryIntCount(value, result)) {
        std::abort();
    }
    return result;
}

inline bool TryAddIntCount(int current, std::size_t increment, int &result) {
    if (current < 0 || increment > static_cast<std::size_t>(INT_MAX - current)) {
        return false;
    }
    result = current + static_cast<int>(increment);
    return true;
}

inline int AddIntCount(int current, std::size_t increment) {
    int result = 0;
    if (!TryAddIntCount(current, increment, result)) {
        std::abort();
    }
    return result;
}

inline bool TryUint32Count(std::size_t value, std::uint32_t &result) {
    if (value > UINT32_MAX) {
        return false;
    }
    result = static_cast<std::uint32_t>(value);
    return true;
}

inline std::uint32_t Uint32Count(std::size_t value) {
    std::uint32_t result = 0;
    if (!TryUint32Count(value, result)) {
        std::abort();
    }
    return result;
}
} } // namespace cse::counts
