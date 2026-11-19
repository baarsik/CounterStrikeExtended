#pragma once

#include "goldsrc_frame_timing.h"

inline float TriggerCameraTurnRate(float seconds) {
    if (seconds <= 0.0f) {
        return 0.0f;
    }

    // At 100 FPS the old angular velocity was error * 40 * 0.01.
    // SV_Physics_Noclip then integrated that velocity over the same 10 ms.
    constexpr double referenceErrorChange = 0.004;
    const double referenceSteps = static_cast<double>(seconds) * CseReferenceFramesPerSecond;
    return static_cast<float>(-expm1(log1p(-referenceErrorChange) * referenceSteps) / seconds);
}
