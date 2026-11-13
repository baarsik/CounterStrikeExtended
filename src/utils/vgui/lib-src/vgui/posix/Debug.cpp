#include "VGUI.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

namespace {

constexpr std::size_t DebugBufferBytes = 2048;
static_assert(std::numeric_limits<int>::max() == INT32_MAX, "Debug prefixes require signed 32-bit int");

std::uint32_t& CounterStorage() {
    static std::uint32_t counter = 0;
    return counter;
}

int NextCounter() {
    const std::uint32_t bits = CounterStorage()++;
    if (bits <= static_cast<std::uint32_t>(INT32_MAX)) {
        return static_cast<int>(bits);
    }
    // Preserve signed decimal interpretation without signed overflow/conversion UB.
    return -1 - static_cast<int>(UINT32_MAX - bits);
}

} // namespace

int vgui::vgui_dprintf(const char* format, ...) {
    flockfile(stderr);
    char buffer[DebugBufferBytes] = {};
    va_list arguments;
    va_start(arguments, format);
    const int length = std::vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    buffer[sizeof(buffer) - 1] = 0;
    std::fputs(buffer, stderr);
    funlockfile(stderr);
    return length;
}

int vgui::vgui_dprintf2(const char* format, ...) {
    flockfile(stderr);
    char buffer[DebugBufferBytes] = {};
    std::snprintf(buffer, sizeof(buffer), "%d:", NextCounter());
    const std::size_t prefix = std::strlen(buffer);
    va_list arguments;
    va_start(arguments, format);
    const int length = std::vsnprintf(buffer + prefix, sizeof(buffer) - prefix, format, arguments);
    va_end(arguments);
    buffer[sizeof(buffer) - 1] = 0;
    std::fputs(buffer, stderr);
    funlockfile(stderr);
    return length;
}
