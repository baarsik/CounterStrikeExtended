#pragma once

#include "goldsrc_frame_timing.h"

inline void RecoverCrosshair(float& distance, double& alpha, double seconds) {
    constexpr double gapRetention = 0.987;
    constexpr double gapReduction = 0.1;
    constexpr double gapOffset = gapReduction / (1.0 - gapRetention);
    constexpr double alphaPerReferenceFrame = 2.0;

    const double retention = CseFrameRetention(gapRetention, seconds);
    distance = static_cast<float>(distance - (1.0 - retention) * (distance + gapOffset));
    alpha += seconds * CseReferenceFramesPerSecond * alphaPerReferenceFrame;
}
