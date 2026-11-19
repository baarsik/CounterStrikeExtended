#pragma once

#include <math.h>

// The legacy per-frame coefficients were tuned at 100 simulation steps per second.
#ifdef __cplusplus
constexpr double CseReferenceFramesPerSecond = 100.0;
#else
static const double CseReferenceFramesPerSecond = 100.0;
#endif

static inline double CseFrameRetention(double referenceRetention, double seconds) {
    return pow(referenceRetention, seconds * CseReferenceFramesPerSecond);
}
