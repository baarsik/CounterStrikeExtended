#pragma once

#include "sound_backend_config.h"

void Snd_WriteLinearBlastStereo16() {
    constexpr int SoundVolumeShift = 8;
    for (int index = 0; index < snd_linear_count; index += 2) {
        int left = (snd_p[index] * snd_vol) >> SoundVolumeShift;
        int right = (snd_p[index + 1] * snd_vol) >> SoundVolumeShift;
        snd_out[index] = static_cast<short>((std::clamp)(left, SOUND_SAMPLE_MIN, SOUND_SAMPLE_MAX));
        snd_out[index + 1] = static_cast<short>((std::clamp)(right, SOUND_SAMPLE_MIN, SOUND_SAMPLE_MAX));
    }
}

void S_TransferStereo16(int end) {
    constexpr float SoundVolumeScale = 256.0f;
    constexpr int StereoChannels = 2;
    void *data;
#if !CSE_SOUND_USE_SDL
    constexpr int SoundBufferLockRetries = 10000;
    DWORD dataSize = 0;
    void *secondData = nullptr;
    DWORD secondSize = 0;
#endif
    snd_vol = static_cast<int>(volume.value * SoundVolumeScale);
    snd_p = reinterpret_cast<int *>(paintbuffer);
    int position = paintedtime * StereoChannels;
    int endPosition = end * StereoChannels;
#if CSE_SOUND_USE_SDL
    if (!SNDDMA_LockBuffer()) {
        Con_Printf("S_TransferStereo16: SDL::Lock Sound Buffer Failed\n");
        S_Shutdown();
        S_Startup();
        return;
    }
    data = shm->buffer;
#else
    if (pDSBuf) {
        int retries = 0;
        HRESULT result;
        while ((result = pDSBuf->Lock(0, gSndBufSize, &data, &dataSize, &secondData, &secondSize, 0)) != DS_OK) {
            if (result != DSERR_BUFFERLOST) {
                Con_Printf("S_TransferStereo16: DS::Lock Sound Buffer Failed\n");
                S_Shutdown();
                S_Startup();
                return;
            }
            if (++retries > SoundBufferLockRetries) {
                Con_Printf("S_TransferStereo16: DS: couldn't restore buffer\n");
                S_Shutdown();
                S_Startup();
                return;
            }
        }
    } else {
        data = shm->buffer;
    }
#endif
    while (position < endPosition) {
        int frameCount = shm->samples >> 1;
        int frame = position & (frameCount - 1);
        snd_out = static_cast<short *>(data) + frame * StereoChannels;
        int count = frameCount - frame;
        if (count > endPosition - position) {
            count = endPosition - position;
        }
        snd_linear_count = count * StereoChannels;
        Snd_WriteLinearBlastStereo16();
        snd_p += snd_linear_count;
        position += snd_linear_count >> 1;
    }
#if CSE_SOUND_USE_SDL
    SNDDMA_UnlockBuffer();
#else
    if (pDSBuf) {
        pDSBuf->Unlock(data, dataSize, nullptr, 0);
    }
#endif
}
