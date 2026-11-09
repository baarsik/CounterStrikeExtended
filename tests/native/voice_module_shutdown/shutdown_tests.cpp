#include "ivoicerecord.h"
#include "voice_mixer_controls.h"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>

IVoiceRecord* g_pVoiceRecord = nullptr;
IMixerControls* g_pMixerControls = nullptr;
void Voice_ReleaseCaptureForModuleUnload();
void Voice_ReleaseMixerForModuleUnload();

namespace {
enum class Step { Worker, Capture, Mixer, Music, Effects };
std::vector<Step> steps;
bool workerCloses = true;

struct Worker {
    bool Shutdown() {
        steps.push_back(Step::Worker);
        return workerCloses;
    }
};
Worker worker;
Worker* thread = &worker;

struct Music {
    void Shutdown() {
        steps.push_back(Step::Music);
    }
};
Music musicPlayer;

void SNDDMA_Shutdown() {
    steps.push_back(Step::Effects);
}

class Recorder final : public IVoiceRecord {
public:
    void Release() override {
        ++releases;
        visibleDuringRelease = visibleDuringRelease || g_pVoiceRecord != nullptr;
        steps.push_back(Step::Capture);
        if (reenter && releases == 1) {
            Voice_ReleaseCaptureForModuleUnload();
        }
        if (fail) {
            throw std::runtime_error("Controlled recorder close failure");
        }
    }

    bool RecordStart() override {
        return true;
    }

    void RecordStop() override {
    }

    void Idle() override {
    }

    int GetRecordedData(short*, int) override {
        return 0;
    }

    int releases = 0;
    bool visibleDuringRelease = false;
    bool reenter = false;
    bool fail = false;
};

class Mixer final : public IMixerControls {
public:
    void Release() override {
        ++releases;
        visibleDuringRelease = visibleDuringRelease || g_pMixerControls != nullptr;
        steps.push_back(Step::Mixer);
        if (releases == 1) {
            Voice_ReleaseMixerForModuleUnload();
        }
    }

    bool GetValue_Float(Control, float&) override {
        return false;
    }

    bool SetValue_Float(Control, float) override {
        return false;
    }

    bool SelectMicrophoneForWaveInput() override {
        return false;
    }

    int releases = 0;
    bool visibleDuringRelease = false;
};

#define CSE_SOUND_USE_SDL 1
#include "actual_audio_close.inc"

void Check(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::exit(EXIT_FAILURE);
    }
}
}

int main() {
    Mixer mixer;
    g_pMixerControls = &mixer;
    Voice_ReleaseMixerForModuleUnload();
    Voice_ReleaseMixerForModuleUnload();
    Check(mixer.releases == 1 && !mixer.visibleDuringRelease && g_pMixerControls == nullptr,
        "Mixer remains callable during Release or is released more than once");
    steps.clear();
    Recorder record;
    record.reenter = true;
    g_pVoiceRecord = &record;
    Voice_ReleaseCaptureForModuleUnload();
    Voice_ReleaseCaptureForModuleUnload();
    Check(record.releases == 1 && !record.visibleDuringRelease && g_pVoiceRecord == nullptr,
        "Recorder remains callable during Release or is released more than once");
    steps.clear();
    Recorder guarded;
    Mixer guardedMixer;
    g_pVoiceRecord = &guarded;
    g_pMixerControls = &guardedMixer;
    Check(CDAudio_ShutdownForModuleUnload(), "Owned audio close failed");
    Check(steps == std::vector<Step>{Step::Worker, Step::Capture, Step::Mixer, Step::Music, Step::Effects},
        "Capture was not closed before independent music/effects cleanup");
    Check(CDAudio_ShutdownForModuleUnload() && guarded.releases == 1 && guardedMixer.releases == 1,
        "Repeated module cleanup releases an owner twice");
    steps.clear();
    Recorder blocked;
    g_pVoiceRecord = &blocked;
    workerCloses = false;
    Check(!CDAudio_ShutdownForModuleUnload() && steps == std::vector<Step>{Step::Worker}
        && g_pVoiceRecord == &blocked, "Failed worker join proceeded toward unload");
    workerCloses = true;
    Voice_ReleaseCaptureForModuleUnload();
    Recorder throwing;
    throwing.fail = true;
    g_pVoiceRecord = &throwing;
    bool threw = false;
    try {
        CDAudio_ShutdownForModuleUnload();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Check(threw && g_pVoiceRecord == nullptr && throwing.releases == 1,
        "Close failure was swallowed or left a stale recorder pointer");
    std::puts("PASS actual capture clear-before-Release, reentry, repeated cleanup and module-close ordering");
}
