#if defined(_WIN32)
#include CSE_A3D_MIXER_SOURCE
#else
// The source is a Windows LLP64 mixer. Model its original long limits in this
// component adapter; selected PCM values also fit the host lrint/long domain.
#include <cstdint>
#include <limits.h>
#undef LONG_MIN
#undef LONG_MAX
#define LONG_MIN INT32_MIN
#define LONG_MAX INT32_MAX
#include "portable_mixer.inc"
#endif
#include "multiply_fixed_body.inc"
