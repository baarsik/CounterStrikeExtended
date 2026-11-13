#pragma once

#include "mpg123_decoder.h"
#include <array>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace cse {

class Mpg123MusicPlayer {
public:
    Mpg123MusicPlayer() = default;
    ~Mpg123MusicPlayer();
    Mpg123MusicPlayer(const Mpg123MusicPlayer &) = delete;
    Mpg123MusicPlayer &operator=(const Mpg123MusicPlayer &) = delete;

    bool Init();
    bool Open(const char *filename, bool looping);
    bool Start();
    void Stop();
    void Shutdown();
    void Pause(bool paused);
    void SetVolume(float volume);
    void SetLooping(bool looping);
    bool IsPlaying();

private:
    bool EnsureAudio();
    void StopLocked();
    void Produce();

    // Public calls serialize separately so Stop can join without blocking the producer's lock.
    std::mutex controlMutex_;
    std::mutex dataMutex_;
    std::condition_variable wake_;
    std::thread producer_;
    Mpg123Decoder decoder_;
    static constexpr std::size_t decodeBufferBytes = 16384;
    // Keep the fixed scratch buffer with the player, outside the producer's
    // stack frame. Only the joined producer accesses it while a track is open.
    std::array<unsigned char, decodeBufferBytes> decodeBuffer_{};
    SDL_AudioStream *stream_ = nullptr;
    bool ownsAudio_ = false;
    bool stopping_ = false;
    bool started_ = false;
    bool paused_ = false;
    bool finished_ = false;
    bool looping_ = false;
    float volume_ = 1.0f;
};

}
