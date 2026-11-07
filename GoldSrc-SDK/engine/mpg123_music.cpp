#include "mpg123_music.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <exception>

namespace cse {

Mpg123MusicPlayer::~Mpg123MusicPlayer() {
    Shutdown();
}

bool Mpg123MusicPlayer::EnsureAudio() {
    if (!ownsAudio_) {
        ownsAudio_ = SDL_InitSubSystem(SDL_INIT_AUDIO);
    }
    return ownsAudio_;
}

bool Mpg123MusicPlayer::Init() {
    std::lock_guard<std::mutex> control(controlMutex_);
    return EnsureAudio();
}

bool Mpg123MusicPlayer::Open(const char *filename, bool looping) {
    std::lock_guard<std::mutex> control(controlMutex_);
    StopLocked();
    if (!EnsureAudio() || !decoder_.Open(filename)) {
        return false;
    }
    const SDL_AudioSpec format = decoder_.Format();
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &format, nullptr, nullptr);
    if (!stream_) {
        decoder_.Close();
        return false;
    }
    SDL_SetAudioStreamGain(stream_, volume_);
    stopping_ = false;
    finished_ = false;
    looping_ = looping;
    try {
        producer_ = std::thread(&Mpg123MusicPlayer::Produce, this);
    } catch (const std::exception &) {
        SDL_DestroyAudioStream(stream_);
        stream_ = nullptr;
        decoder_.Close();
        return false;
    }
    return true;
}

bool Mpg123MusicPlayer::Start() {
    std::lock_guard<std::mutex> control(controlMutex_);
    std::lock_guard<std::mutex> data(dataMutex_);
    if (!stream_ || finished_) {
        return false;
    }
    started_ = SDL_ResumeAudioStreamDevice(stream_);
    paused_ = false;
    wake_.notify_one();
    return started_;
}

void Mpg123MusicPlayer::StopLocked() {
    {
        std::lock_guard<std::mutex> data(dataMutex_);
        stopping_ = true;
        wake_.notify_one();
    }
    if (producer_.joinable()) {
        producer_.join();
    }
    // The producer is joined; no decoder or stream access can remain in flight.
    if (stream_) {
        SDL_DestroyAudioStream(stream_);
        stream_ = nullptr;
    }
    decoder_.Close();
    started_ = false;
    paused_ = false;
    finished_ = false;
}

void Mpg123MusicPlayer::Stop() {
    std::lock_guard<std::mutex> control(controlMutex_);
    StopLocked();
}

void Mpg123MusicPlayer::Shutdown() {
    std::lock_guard<std::mutex> control(controlMutex_);
    StopLocked();
    if (ownsAudio_) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        ownsAudio_ = false;
    }
}

void Mpg123MusicPlayer::Pause(bool paused) {
    std::lock_guard<std::mutex> control(controlMutex_);
    std::lock_guard<std::mutex> data(dataMutex_);
    if (!stream_) {
        return;
    }
    const bool changed = paused ? SDL_PauseAudioStreamDevice(stream_) : SDL_ResumeAudioStreamDevice(stream_);
    if (changed) {
        paused_ = paused;
    }
    wake_.notify_one();
}

void Mpg123MusicPlayer::SetVolume(float volume) {
    std::lock_guard<std::mutex> control(controlMutex_);
    std::lock_guard<std::mutex> data(dataMutex_);
    volume_ = std::isfinite(volume) ? std::clamp(volume, 0.0f, 1.0f) : 0.0f;
    if (stream_) {
        SDL_SetAudioStreamGain(stream_, volume_);
    }
}

void Mpg123MusicPlayer::SetLooping(bool looping) {
    std::lock_guard<std::mutex> control(controlMutex_);
    std::lock_guard<std::mutex> data(dataMutex_);
    looping_ = looping;
}

bool Mpg123MusicPlayer::IsPlaying() {
    std::lock_guard<std::mutex> control(controlMutex_);
    std::lock_guard<std::mutex> data(dataMutex_);
    return stream_ && started_ && !paused_ && !finished_;
}

void Mpg123MusicPlayer::Produce() {
    constexpr std::size_t decodeBufferBytes = 16384;
    constexpr int queueMilliseconds = 200;
    constexpr int millisecondsPerSecond = 1000;
    constexpr int bytesPerSample = sizeof(short);
    constexpr auto pollInterval = std::chrono::milliseconds(5);
    std::array<unsigned char, decodeBufferBytes> buffer{};
    std::unique_lock<std::mutex> data(dataMutex_);
    const SDL_AudioSpec format = decoder_.Format();
    const int queueLimit = format.freq * format.channels * bytesPerSample * queueMilliseconds / millisecondsPerSecond;
    bool end = false;
    bool producedSinceRewind = false;
    while (!stopping_) {
        const int queued = SDL_GetAudioStreamQueued(stream_);
        if (queued < 0) {
            finished_ = true;
            break;
        }
        if (end) {
            if (queued == 0 && SDL_GetAudioStreamAvailable(stream_) == 0) {
                finished_ = true;
                break;
            }
        } else if (queued < queueLimit) {
            const auto result = decoder_.Read(buffer.data(), buffer.size());
            if (result.bytes > 0) {
                producedSinceRewind = true;
                if (!SDL_PutAudioStreamData(stream_, buffer.data(), static_cast<int>(result.bytes))) {
                    finished_ = true;
                    break;
                }
            }
            if (result.error) {
                finished_ = true;
                break;
            }
            if (result.end) {
                if (looping_ && producedSinceRewind && decoder_.Rewind()) {
                    producedSinceRewind = false;
                } else {
                    end = true;
                    SDL_FlushAudioStream(stream_);
                }
            }
        }
        wake_.wait_for(data, pollInterval, [this] { return stopping_; });
    }
}

}
