#include "fixture_seams.h"
#include <algorithm>
#include <cstdio>

namespace {
using cse::VoiceCapturePolicy;
using fixture::Check;
using fixture::state;

std::string Extension() {
#if defined(_WIN32)
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}
void Complete() {
    Voice_Deinit();
    fixture::CheckClean();
    const unsigned int captures = state.captureReleased;
    const unsigned int mixers = state.mixerReleased;
    Voice_Deinit();
    Voice_ReleaseCaptureForModuleUnload();
    Voice_ReleaseMixerForModuleUnload();
    Check(state.captureReleased == captures && state.mixerReleased == mixers,
        "repeated actual cleanup cannot release an object twice");
}
void Selection() {
    for (float mode : {0.0f, 1.0f, -2.0f, 0.25f}) {
        for (unsigned int fail = 0; fail < 3; ++fail) {
            fixture::Reset();
            voice_dsound.value = mode;
            const auto preferred = mode == 0 ? VoiceCapturePolicy::WaveIn : VoiceCapturePolicy::DirectSound;
            state.allowDS = fail == 0 || (fail == 1 && preferred == VoiceCapturePolicy::WaveIn);
            state.allowWave = fail == 0 || (fail == 1 && preferred == VoiceCapturePolicy::DirectSound);
            fixture::SetRate(16000);
            Check(Voice_Init("voice_fixture", 7) == TRUE, "receive/codec init can succeed even with both capture modes absent");
            Check(state.attempts.size() == (fail == 0 ? 1u : 2u), "actual caller performs only the required fallback");
            Check(state.attempts[0].policy == preferred, "voice_dsound chooses the retained first capture policy");
            for (const auto& attempt : state.attempts) {
                Check(attempt.sampleRate == 16000, "actual sample-format rate passes unchanged through real delegates");
            }
            if (fail != 0) {
                Check(state.attempts[1].policy != preferred, "retry uses the other policy exactly once");
            }
            Check(state.captureCreated == (fail == 2 ? 0u : 1u) && state.messages.size() == fail,
                "actual caller logs retry/absence without fabricating a successful recorder");
            Check(state.filenames == std::vector<std::string>{"voice_fixture" + Extension()},
                "actual platform branch chooses the native codec filename");
            Check(state.qualities.size() == VOICE_NUM_CHANNELS + 1
                    && std::all_of(state.qualities.begin(), state.qualities.end(), [](int value) { return value == 7; }),
                "actual codec initialization loop forwards requested quality to encode and every channel");
            Complete();
        }
    }
    fixture::Reset();
    Check(Voice_Init("voice_fixture", 1) == TRUE && state.attempts[0].sampleRate == fixture::DefaultRate(),
        "actual default sample-format declaration supplies the production capture rate");
    Complete();
}
void Skips() {
    for (bool steam : {false, true}) {
        fixture::Reset();
        state.steam = steam;
        Check(Voice_Init(steam ? "voice_fixture" : "", 1) == TRUE && state.attempts.empty()
                && state.moduleLoads == 0 && state.codecRequests == 0,
            "Steam or empty codec skips fallback capture and external codec factories");
        Check(state.mixerCreated == 1 && state.selections == 1,
            "skipped fallback still performs the actual caller's later mixer work");
        Complete();
    }
    fixture::Reset();
    voice_enable.value = 0;
    Check(Voice_Init("voice_fixture", 1) == FALSE && state.voiceSEStarts == 0 && state.steamQueries == 0
            && state.attempts.empty() && !g_bVoiceAtLeastPartiallyInitted,
        "disabled voice returns before any initialization seam");
    fixture::Reset();
    state.voiceSE = false;
    Check(Voice_Init("voice_fixture", 1) == FALSE && state.attempts.empty() && state.steamQueries == 0
            && g_bVoiceAtLeastPartiallyInitted, "actual VoiceSE failure retains its documented partial-init flag");
    Complete();
}
void Failures() {
    for (unsigned int failure = 0; failure < 5; ++failure) {
        fixture::Reset();
        state.module = failure != 0;
        state.factory = failure != 1;
        state.missingCodec = failure == 2 ? 1 : failure == 4 ? 4 : 0;
        state.failedInit = failure == 3 ? 1 : 0;
        Check(Voice_Init("voice_fixture", 3) == FALSE, "actual module/factory/codec/channel failure returns false");
        fixture::CheckClean();
        Check(state.captureCreated == 1 && state.captureReleased == 1 && state.mixerCreated == 0,
            "actual failed init releases captured state before returning and does not reach mixer initialization");
        Complete();
    }
    fixture::Reset();
    state.failedInit = 4;
    Check(Voice_Init("voice_fixture", 2) == FALSE, "partial channel codec Init failure returns false");
    fixture::CheckClean();
}
void Reinitialize() {
    fixture::Reset();
    Check(Voice_Init("voice_fixture", 1) == TRUE, "first caller initialization succeeds");
    Check(Voice_Init("voice_fixture", 2) == TRUE && state.captureReleased == 1 && state.mixerReleased == 1
            && state.captureCreated == 2 && state.mixerCreated == 2,
        "actual second initialization deinitializes the previous owned recorder and mixer first");
    Complete();
    for (unsigned int mode = 0; mode < 3; ++mode) {
        fixture::Reset();
        voice_forcemicrecord.value = mode == 0 ? 0.0f : 1.0f;
        state.mixer = mode != 1;
        state.selection = false;
        Check(Voice_Init("", 1) == TRUE && state.selections == (mode == 2 ? 1u : 0u),
            "forced-mic option honors missing mixer and ignores unsupported selection without suppressing init");
        Complete();
    }
}
}

int main() {
    Selection();
    Skips();
    Failures();
    Reinitialize();
    fixture::CheckClean();
    std::printf("actual caller wiring passed; pointer_bytes%zu; native_delegates%d; no real provider executed\n",
        sizeof(void*), sizeof(void*) > 4 ? 1 : 0);
}
