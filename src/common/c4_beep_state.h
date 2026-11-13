#ifndef CSE_C4_BEEP_STATE_H
#define CSE_C4_BEEP_STATE_H

#include <cstring>

namespace cse {
namespace save {

// Stable save symbols. Zero is reserved for files that predate this field.
enum class C4BeepPhase : int {
    LegacyMissing = 0,
    Silent = 1,
    Beep1 = 2,
    Beep2 = 3,
    Beep3 = 4,
    Beep4 = 5,
    Beep5 = 6
};

struct C4BeepDefinition {
    C4BeepPhase phase;
    const char *sound;
    float legacyAttenuation;
    int legacyNextWave;
};

// These attenuation values identify the phases assigned by CGrenade::C4Think.
constexpr C4BeepDefinition C4BeepDefinitions[] = {
    {C4BeepPhase::Silent, nullptr, 0.0f, 0},
    {C4BeepPhase::Beep1, "weapons/c4_beep1.wav", 1.5f, 1},
    {C4BeepPhase::Beep2, "weapons/c4_beep2.wav", 1.0f, 2},
    {C4BeepPhase::Beep3, "weapons/c4_beep3.wav", 0.8f, 3},
    {C4BeepPhase::Beep4, "weapons/c4_beep4.wav", 0.5f, 4},
    {C4BeepPhase::Beep5, "weapons/c4_beep5.wav", 0.2f, 5}
};

inline bool CaptureC4Beep(const char *sound, C4BeepPhase &phase) {
    phase = C4BeepPhase::LegacyMissing;
    for (const auto &definition : C4BeepDefinitions) {
        if ((!sound && !definition.sound)
            || (sound && definition.sound && std::strcmp(sound, definition.sound) == 0)) {
            phase = definition.phase;
            return true;
        }
    }
    return false;
}

inline bool RestoreC4Beep(C4BeepPhase phase, float attenuation, int &nextWave, const char *&sound) {
    sound = nullptr;
    for (const auto &definition : C4BeepDefinitions) {
        const bool legacy = phase == C4BeepPhase::LegacyMissing;
        if ((legacy && attenuation == definition.legacyAttenuation) || phase == definition.phase) {
            sound = definition.sound;
            if (legacy) {
                nextWave = definition.legacyNextWave;
            }
            return true;
        }
    }
    return false;
}

} // namespace save
} // namespace cse

#endif
