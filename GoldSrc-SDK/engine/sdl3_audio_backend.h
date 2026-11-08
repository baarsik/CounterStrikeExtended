#pragma once

#include "sdl3_audio_ring.h"
#include <SDL3/SDL.h>

namespace cse {

// Native effects use this stream. The maintained Win32 reference keeps DSound/WaveOut.
class SdlAudioBackend {
public:
    ~SdlAudioBackend() {
        Close();
    }

    SdlAudioBackend() = default;
    SdlAudioBackend(const SdlAudioBackend &) = delete;
    SdlAudioBackend &operator=(const SdlAudioBackend &) = delete;

    bool Open(int frequency, int channelCount,
              AudioRing::PlaybackPolicy policy = AudioRing::PlaybackPolicy::LegacyByteZero) {
        if (stream_) {
            return true;
        }
        if (frequency <= 0 || channelCount <= 0) {
            return false;
        }
        // Always acquire a reference, including when another consumer already initialized audio.
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            return false;
        }
        ownsAudio_ = true;
        ring_.Reset();
        playbackPolicy_ = policy;
        const SDL_AudioSpec input = {SDL_AUDIO_S16LE, channelCount, frequency};
        stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &input, Feed, this);
        if (!stream_) {
            Close();
            return false;
        }
        // OpenAudioDeviceStream starts paused; the engine publishes its DMA buffer before resuming.
        return true;
    }

    void Close() {
        if (stream_) {
            // Destruction synchronizes callbacks and closes the stream-owned logical device.
            SDL_DestroyAudioStream(stream_);
            stream_ = nullptr;
        }
        if (ownsAudio_) {
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            ownsAudio_ = false;
        }
        ring_.Reset();
    }

    bool IsOpen() const {
        return stream_ != nullptr;
    }

    bool SetPaused(bool paused) {
        if (!stream_) {
            return false;
        }
        return paused ? SDL_PauseAudioStreamDevice(stream_) : SDL_ResumeAudioStreamDevice(stream_);
    }

    SDL_AudioDeviceID Device() const {
        return stream_ ? SDL_GetAudioStreamDevice(stream_) : 0;
    }

    bool IsPaused() const {
        return stream_ && SDL_AudioDevicePaused(SDL_GetAudioStreamDevice(stream_));
    }

    unsigned char *Buffer() {
        return ring_.Data();
    }

    bool LockBuffer() {
        return stream_ && SDL_LockAudioStream(stream_);
    }

    void UnlockBuffer() {
        if (stream_) {
            SDL_UnlockAudioStream(stream_);
        }
    }

    int MonoSamplePosition() {
        if (!stream_ || !SDL_LockAudioStream(stream_)) {
            return 0;
        }
        const int position = ring_.MonoSamplePosition();
        SDL_UnlockAudioStream(stream_);
        return position;
    }

private:
    static void SDLCALL Feed(void *userdata, SDL_AudioStream *stream, int additionalBytes, int) {
        auto *backend = static_cast<SdlAudioBackend *>(userdata);
        // SDL already holds the stream lock; demand is in our input format, not the device format.
        backend->ring_.Drain(additionalBytes, [stream](const unsigned char *data, int bytes) {
            return SDL_PutAudioStreamData(stream, data, bytes);
        }, backend->playbackPolicy_);
    }

    AudioRing ring_;
    AudioRing::PlaybackPolicy playbackPolicy_ = AudioRing::PlaybackPolicy::LegacyByteZero;
    SDL_AudioStream *stream_ = nullptr;
    bool ownsAudio_ = false;
};

}
