#ifndef CSE_CLOCK_FIXTURE_FP_OBSERVATION_H
#define CSE_CLOCK_FIXTURE_FP_OBSERVATION_H

#include <cfenv>
#include <cstdint>
#include <cstdlib>
#if defined(_WIN32)
#include <float.h>
#if defined(_M_IX86) || defined(_M_X64)
#include <xmmintrin.h>
#endif
#endif

inline std::uint64_t ClockFixtureFpControl() {
#if defined(_WIN32)
    unsigned int control = 0;
    if (_controlfp_s(&control, 0, 0) != 0) {
        std::abort();
    }
    return control;
#elif defined(__APPLE__) && defined(__aarch64__)
    fenv_t environment;
    if (std::fegetenv(&environment) != 0) {
        std::abort();
    }
    return environment.__fpcr;
#else
#error Unmeasured fixture floating-point environment
#endif
}

inline std::uint64_t ClockFixtureSimdControl() {
#if defined(_M_IX86) || defined(_M_X64)
    constexpr unsigned int StatusMask = 0x3f;
    return _mm_getcsr() & ~StatusMask;
#else
    return 0;
#endif
}

inline std::uint64_t ClockFixtureFpStatus() {
#if defined(_WIN32)
    std::uint64_t status = _statusfp();
#if defined(_M_IX86) || defined(_M_X64)
    constexpr unsigned int StatusMask = 0x3f;
    status |= static_cast<std::uint64_t>(_mm_getcsr() & StatusMask) << 32;
#endif
    return status;
#elif defined(__APPLE__) && defined(__aarch64__)
    fenv_t environment;
    if (std::fegetenv(&environment) != 0) {
        std::abort();
    }
    return environment.__fpsr;
#else
#error Unmeasured fixture floating-point environment
#endif
}

#endif
