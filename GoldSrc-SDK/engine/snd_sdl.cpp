#include "sound.h"

int g_fUseDInput;

extern "C" void SetMouseEnable(qboolean) {
}

#if !defined(_WIN32)

#include "sdl3_audio_backend.h"
#include <cstdio>
#include <cstdlib>

extern int snd_blocked;

static cse::SdlAudioBackend audioBackend;
static int snd_buffer_count;

extern "C" void Snd_ReleaseBuffer() {
    if (--snd_buffer_count == 0) {
        S_ClearBuffer();
        S_Shutdown();
    }
}

extern "C" void Snd_AcquireBuffer() {
    if (++snd_buffer_count == 1) {
        S_Startup();
    }
}

extern "C" void S_BlockSound() {
    if (++snd_blocked == 1) {
        audioBackend.SetPaused(true);
    }
}

extern "C" void S_UnblockSound() {
    if (--snd_blocked == 0) {
        audioBackend.SetPaused(false);
    }
}

extern "C" void SNDDMA_Shutdown() {
    audioBackend.Close();
    sn.soundalive = FALSE;
    sn.buffer = nullptr;
}

extern "C" qboolean SNDDMA_Init() {
    if (audioBackend.IsOpen()) {
        return TRUE;
    }

    constexpr int engineChannels = 2;
    constexpr int engineSampleBits = 16;
    constexpr int engineMixRate = 11025;
    constexpr int engineDmaRate = 22050;
    sn.channels = engineChannels;
    sn.samplebits = engineSampleBits;
    sn.speed = engineMixRate;
    sn.dmaspeed = engineDmaRate;
    shm = &sn;
    setenv("PULSE_PROP_application.name", "Half-Life 1", 1);
    setenv("PULSE_PROP_media.role", "game", 1);

    if (!audioBackend.Open(shm->dmaspeed, shm->channels)) {
        std::puts("SDL audio initialization failed.");
        return FALSE;
    }
    shm->soundalive = TRUE;
    shm->splitbuffer = FALSE;
    shm->samples = cse::AudioRing::BufferBytes / cse::AudioRing::BytesPerMonoSample;
    shm->samplepos = 0;
    shm->submission_chunk = 1;
    shm->buffer = audioBackend.Buffer();
    if (!audioBackend.SetPaused(snd_blocked > 0)) {
        SNDDMA_Shutdown();
        return FALSE;
    }
    return TRUE;
}

extern "C" void SNDDMA_Submit() {
}

extern "C" int SNDDMA_GetDMAPos() {
    return audioBackend.MonoSamplePosition();
}

#endif
