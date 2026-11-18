#include "RadarFade.h"

#include <algorithm>

#include "NativeSdk.h"

namespace {
// FFADE_* (engine shake.h); only the bits used by V_FadeAlpha are needed.
constexpr int kFadeOut = 0x0001;
constexpr int kFadeStayout = 0x0004;
} // namespace

int RadarFade_ComputeAlpha(const screenfade_t& sf, double time) {
    if (sf.fadeReset < time && sf.fadeEnd < time && !(sf.fadeFlags & kFadeStayout)) {
        return 0;
    }

    if (!(sf.fadeFlags & kFadeOut) && sf.fadeSpeed == 0.0f && sf.fadeEnd <= sf.fadeReset) {
        return sf.fadealpha;
    }

    double alpha;

    if (!(sf.fadeFlags & kFadeStayout)) {
        alpha = sf.fadeSpeed * (sf.fadeEnd - time);

        if (sf.fadeFlags & kFadeOut) {
            alpha += sf.fadealpha;
        }
    } else {
        alpha = sf.fadealpha;

        if ((sf.fadeFlags & kFadeOut) && sf.fadeTotalEnd > time) {
            alpha += (sf.fadeTotalEnd - time) * sf.fadeSpeed;
        }
    }

    // The engine rounds the combined alpha, after applying the fade-out offset.
    return static_cast<int>(std::min(std::max(0.0, alpha), static_cast<double>(sf.fadealpha)));
}

bool RadarFade_IsNearWhite(const screenfade_t& sf) {
    bool result = sf.fader >= 200 && sf.fadeg >= 200 && sf.fadeb >= 200;
    return result;
}
