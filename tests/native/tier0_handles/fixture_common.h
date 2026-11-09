#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "tier0/platform.h"
// Preserve the actual platform/types and class declarations, while binding methods into this executable.
#undef TIER0_API
#define TIER0_API
#include "tier0/dbg.h"

namespace fixture {

inline void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

inline std::size_t FormatArguments(const char* format) {
    std::size_t count = 0;
    for (const char* cursor = format; *cursor; ++cursor) {
        if (*cursor == '%') {
            if (cursor[1] == '%') {
                ++cursor;
            } else {
                ++count;
            }
        }
    }
    return count;
}

template<typename... Arguments>
int Format(char* output, std::size_t bytes, const char* format, Arguments... arguments) {
    // Reject the frozen missing-varargs defect before invoking printf and reading nonexistent arguments.
    const std::size_t conversions = FormatArguments(format);
    if (conversions != sizeof...(arguments)) {
        throw std::runtime_error("Printf argument mismatch: " + std::string(format) + " has "
            + std::to_string(conversions) + " conversions and " + std::to_string(sizeof...(arguments))
            + " arguments");
    }
#if defined(_WIN32)
    return ::_snprintf(output, bytes, format, arguments...);
#else
    return std::snprintf(output, bytes, format, arguments...);
#endif
}

inline std::string& EventDiagnostic() {
    static std::string diagnostic;
    return diagnostic;
}

template<typename Argument>
void CaptureEvent(bool, const char* format, Argument argument, int) {
    char output[128]{};
    Format(output, sizeof(output), format, argument);
    EventDiagnostic() = output;
}

template<typename T>
T OpaqueThreadId(std::uintptr_t bits) {
    if constexpr (std::is_pointer<T>::value) {
        return reinterpret_cast<T>(bits);
    } else {
        return static_cast<T>(bits);
    }
}

inline std::string ExpectedThreadName(void* object, void* thread) {
    char expected[96]{};
    if constexpr (sizeof(void*) > sizeof(std::uint32_t)) {
        std::snprintf(expected, sizeof(expected), "Thread(%p/%p)", object, thread);
    } else {
        std::snprintf(expected, sizeof(expected), "Thread(0x%x/0x%x)",
            static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(object)),
            static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(thread)));
    }
    return expected;
}

inline void CheckEventDiagnostic(void* handle) {
    char expected[128]{};
    if constexpr (sizeof(void*) > sizeof(std::uint32_t)) {
        std::snprintf(expected, sizeof(expected), "Bad Event handle (handle %p)", handle);
    } else {
        std::snprintf(expected, sizeof(expected), "Bad Event handle (handle 0x%x)",
            static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(handle)));
    }
    std::printf("event diagnostic=%s expected=%s\n", EventDiagnostic().c_str(), expected);
    Require(EventDiagnostic() == expected, "Event diagnostic lost pointer identity or changed Win32 output");
}

} // namespace fixture

#undef AssertAtFile
#undef AssertMsgAtFile
#undef AssertMsgAt
#undef AssertMsg1At
#define AssertAtFile(expression, file, line) ::fixture::Require(!!(expression), "Production assertion: " #expression)
#define AssertMsgAtFile(expression, message, file, line) ::fixture::Require(!!(expression), message)
#define AssertMsgAt(expression, message, line) ::fixture::Require(!!(expression), message)
#define AssertMsg1At(expression, message, argument, line) \
    ::fixture::CaptureEvent(!!(expression), message, argument, line)

#include "selected_threadtools.h"
