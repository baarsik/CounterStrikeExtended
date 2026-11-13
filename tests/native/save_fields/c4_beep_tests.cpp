#include "../../../src/common/c4_beep_state.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "Check failed at line %d: %s\n", __LINE__, #condition); \
    return 1; \
} } while (false)

int main() {
    using namespace cse::save;
    struct Case {
        const char *sound;
        float attenuation;
        C4BeepPhase phase;
        int legacyWave;
    };
    const Case cases[] = {
        {nullptr, 0.0f, C4BeepPhase::Silent, 0},
        {"weapons/c4_beep1.wav", 1.5f, C4BeepPhase::Beep1, 1},
        {"weapons/c4_beep2.wav", 1.0f, C4BeepPhase::Beep2, 2},
        {"weapons/c4_beep3.wav", 0.8f, C4BeepPhase::Beep3, 3},
        {"weapons/c4_beep4.wav", 0.5f, C4BeepPhase::Beep4, 4},
        {"weapons/c4_beep5.wav", 0.2f, C4BeepPhase::Beep5, 5}
    };
    for (const auto &item : cases) {
        C4BeepPhase phase = C4BeepPhase::LegacyMissing;
        // Capture by name, including a different allocation with identical bytes.
        char copiedSound[64]{};
        if (item.sound) {
            const auto soundBytes = std::strlen(item.sound) + 1;
            CHECK(soundBytes <= sizeof(copiedSound));
            std::memcpy(copiedSound, item.sound, soundBytes);
        }
        CHECK(CaptureC4Beep(item.sound ? copiedSound : nullptr, phase));
        CHECK(phase == item.phase);
        const char *sound = "stale runtime state";
        int wave = 37;
        CHECK(RestoreC4Beep(phase, -1.0f, wave, sound));
        CHECK(wave == 37);
        CHECK(item.sound ? sound && std::strcmp(sound, item.sound) == 0 : sound == nullptr);
        // Legacy files omit both symbolic phase and wave. Pointer bits are not an input.
        wave = 0;
        sound = "stale runtime state";
        CHECK(RestoreC4Beep(C4BeepPhase::LegacyMissing, item.attenuation, wave, sound));
        CHECK(wave == item.legacyWave);
        CHECK(item.sound ? sound && std::strcmp(sound, item.sound) == 0 : sound == nullptr);
    }
    C4BeepPhase phase = C4BeepPhase::Silent;
    CHECK(!CaptureC4Beep("weapons/unregistered.wav", phase));
    CHECK(phase == C4BeepPhase::LegacyMissing);
    const char *sound = "stale runtime state";
    int wave = 9;
    CHECK(!RestoreC4Beep(static_cast<C4BeepPhase>(-1), 0.2f, wave, sound));
    CHECK(sound == nullptr && wave == 9);
    CHECK(!RestoreC4Beep(static_cast<C4BeepPhase>(7), 0.2f, wave, sound));
    CHECK(!RestoreC4Beep(C4BeepPhase::LegacyMissing, 0.3f, wave, sound));
    CHECK(!RestoreC4Beep(C4BeepPhase::LegacyMissing,
        std::nextafter(0.8f, 1.0f), wave, sound));
    CHECK(!RestoreC4Beep(C4BeepPhase::LegacyMissing,
        std::numeric_limits<float>::quiet_NaN(), wave, sound));
    CHECK(sound == nullptr && wave == 9);
    // The final sound repeats after wave 4. Never rewind an explicitly saved counter.
    CHECK(RestoreC4Beep(C4BeepPhase::Beep5, 0.2f, wave, sound));
    CHECK(wave == 9 && std::strcmp(sound, "weapons/c4_beep5.wav") == 0);
    return 0;
}
