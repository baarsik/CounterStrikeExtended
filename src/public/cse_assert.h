#pragma once

#include <cstdio>
#include <cstdlib>

#if defined(_WIN32)
// Match the UCRT contract even when NDEBUG hides assert.h's declaration.
extern "C" _ACRTIMP void __cdecl _wassert(
    _In_z_ const wchar_t *message,
    _In_z_ const wchar_t *file,
    _In_ unsigned int line);
#endif

namespace cse {
namespace platform {

inline void UtlAssertionFailure(const wchar_t *message, const wchar_t *file, unsigned int line) {
#if defined(_WIN32)
    _wassert(message, file, line);
#else
    // UTL_ENGINE_ASSERTS is independent of the standard assert/NDEBUG policy.
    std::fprintf(stderr, "Assertion failed: %ls, file %ls, line %u\n", message, file, line);
    std::abort();
#endif
}

} // namespace platform
} // namespace cse
