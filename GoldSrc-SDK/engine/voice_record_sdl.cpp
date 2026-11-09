#include "voice_record_sdl.h"
#include "ivoicerecord.h"
#include "conprint.h"

#include <SDL3/SDL.h>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>

namespace cse {
namespace {

constexpr std::size_t pcmSampleBytes = sizeof(short);
constexpr std::size_t waveInHeaderBytes = 256;
constexpr std::size_t captureChunkBytes = waveInHeaderBytes;
constexpr std::uint64_t waveInHeaderCount = 15;
constexpr std::uint64_t waveInBufferBytes = waveInHeaderCount * waveInHeaderBytes;

static_assert(CHAR_BIT == 8 && sizeof(int) == 4 && sizeof(short) == 2 && SHRT_MIN == -32768 && SHRT_MAX == 32767,
    "Retained capture PCM and integer widths");
static_assert(SDL_BYTEORDER == SDL_LIL_ENDIAN, "Supported capture targets store native short PCM as S16LE");

class VoiceRecordSDL final : public IVoiceRecord {
public:
    explicit VoiceRecordSDL(VoiceCapturePolicy policy) : policy_(policy) {
    }

    VoiceRecordSDL(const VoiceRecordSDL&) = delete;
    VoiceRecordSDL& operator=(const VoiceRecordSDL&) = delete;

    ~VoiceRecordSDL() override {
        Close();
    }

    void Release() override {
        delete this;
    }

    bool Init(int sampleRate, std::size_t capacity) {
        capacity_ = capacity;
        queue_ = static_cast<unsigned char*>(SDL_malloc(capacity));
        if (queue_ == nullptr) {
            return false;
        }
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            return false;
        }
        ownsAudioReference_ = true;
        const SDL_AudioSpec requested = {SDL_AUDIO_S16LE, 1, sampleRate};
        stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_RECORDING, &requested, Receive, this);
        if (stream_ == nullptr) {
            return false;
        }
        SDL_AudioSpec actual = {};
        if (!SDL_GetAudioStreamFormat(stream_, nullptr, &actual) || actual.format != requested.format
                || actual.channels != requested.channels || actual.freq != requested.freq) {
            return false;
        }
        return SDL_ResumeAudioStreamDevice(stream_);
    }

    bool RecordStart() override {
        if (stream_ == nullptr) {
            return false;
        }
        if (!SDL_LockAudioStream(stream_)) {
            return false;
        }
        const bool healthy = failure_ == Failure::None;
        if (healthy && policy_ == VoiceCapturePolicy::WaveIn) {
            recording_ = true;
        }
        SDL_UnlockAudioStream(stream_);
        if (!healthy || policy_ == VoiceCapturePolicy::WaveIn) {
            Idle();
        }
        return healthy && stream_ != nullptr;
    }

    void RecordStop() override {
        if (stream_ != nullptr && policy_ == VoiceCapturePolicy::WaveIn && SDL_LockAudioStream(stream_)) {
            recording_ = false;
            SDL_UnlockAudioStream(stream_);
        }
    }

    void Idle() override {
        if (stream_ == nullptr || !SDL_LockAudioStream(stream_)) {
            return;
        }
        const bool failed = failure_ != Failure::None;
        if (!failed && policy_ == VoiceCapturePolicy::WaveIn) {
            if (!recording_) {
                FlushWaveIn();
            }
        } else if (!failed && pendingWrap_) {
            wrapOffset_ += static_cast<std::uint32_t>(capacity_);
            pendingWrap_ = false;
        }
        SDL_UnlockAudioStream(stream_);
        if (failed) {
            if (!reportedFailure_) {
                reportedFailure_ = true;
                Con_DPrintf("SDL voice capture stream failed.\n");
            }
            Close();
        }
    }

    int GetRecordedData(short* output, int maxSamples) override {
        if (maxSamples < 0 || (output == nullptr && maxSamples != 0)
                || (policy_ == VoiceCapturePolicy::WaveIn
                    && maxSamples > INT_MAX / static_cast<int>(pcmSampleBytes))) {
            return 0;
        }
        if (policy_ == VoiceCapturePolicy::DirectSound) {
            Idle();
        }
        if (stream_ == nullptr || !SDL_LockAudioStream(stream_)) {
            return 0;
        }
        std::size_t bytes = 0;
        if (failure_ != Failure::None) {
            SDL_UnlockAudioStream(stream_);
            Idle();
            return 0;
        }
        if (policy_ == VoiceCapturePolicy::WaveIn) {
            const std::size_t limit = static_cast<std::size_t>(maxSamples) * pcmSampleBytes;
            if (waveCount_ > limit) {
                AdvanceWaveIn(waveCount_ - limit);
            }
            bytes = waveCount_;
            if (bytes != 0) {
                Copy(output, waveRead_, bytes, false);
                AdvanceWaveIn(bytes);
            }
        } else {
            // These are the retained DWORD cursor/event arithmetic and byte limit.
            const std::uint32_t readPosition = static_cast<std::uint32_t>(write_) + wrapOffset_;
            const std::uint32_t limit = static_cast<std::uint32_t>(maxSamples);
            if (limit == 0) {
                lastRead_ = readPosition;
            } else {
                std::uint32_t start = limit < readPosition ? readPosition - limit : 0;
                if (start < lastRead_) {
                    start = lastRead_;
                }
                const std::uint32_t requested = readPosition - start;
                if (requested <= limit && requested <= capacity_) {
                    bytes = requested;
                    if (bytes != 0) {
                        Copy(output, start % capacity_, bytes, true);
                    }
                    lastRead_ = readPosition;
                }
            }
        }
        SDL_UnlockAudioStream(stream_);
        if (policy_ == VoiceCapturePolicy::WaveIn) {
            Idle();
        }
        return static_cast<int>(bytes / pcmSampleBytes);
    }

private:
    enum class Failure {
        None,
        Availability,
        Read,
        InvalidFrame
    };

    static void SDLCALL Receive(void* userdata, SDL_AudioStream* stream, int, int) {
        auto* record = static_cast<VoiceRecordSDL*>(userdata);
        if (record->failure_ != Failure::None) {
            SDL_ClearAudioStream(stream);
            return;
        }
        int remaining = SDL_GetAudioStreamAvailable(stream);
        if (remaining < 0) {
            record->Fail(stream, Failure::Availability);
            return;
        }
        unsigned char scratch[captureChunkBytes];
        while (remaining >= static_cast<int>(pcmSampleBytes)) {
            const int requested = remaining < static_cast<int>(captureChunkBytes)
                ? remaining - remaining % static_cast<int>(pcmSampleBytes) : static_cast<int>(captureChunkBytes);
            const int count = SDL_GetAudioStreamData(stream, scratch, requested);
            if (count == 0) {
                return;
            }
            if (count < 0) {
                record->Fail(stream, Failure::Read);
                return;
            }
            if (count > requested || count % static_cast<int>(pcmSampleBytes) != 0) {
                record->Fail(stream, Failure::InvalidFrame);
                return;
            }
            if (record->policy_ == VoiceCapturePolicy::DirectSound || record->recording_) {
                record->Write(scratch, static_cast<std::size_t>(count));
            }
            remaining -= count;
        }
    }

    void Fail(SDL_AudioStream* stream, Failure reason) {
        if (failure_ == Failure::None) {
            failure_ = reason;
        }
        // Terminal only: copied recording tracks can be cleared under the recursive
        // stream lock without device teardown; later callbacks cannot grow the SDK queue.
        SDL_ClearAudioStream(stream);
    }

    void Write(const unsigned char* data, std::size_t bytes) {
        std::size_t remaining = bytes;
        while (remaining != 0) {
            const std::size_t available = capacity_ - write_;
            const std::size_t count = remaining < available ? remaining : available;
            std::memcpy(queue_ + write_, data, count);
            data += count;
            remaining -= count;
            write_ += count;
            if (write_ == capacity_) {
                write_ = 0;
                pendingWrap_ = true;
            }
        }
        if (policy_ == VoiceCapturePolicy::WaveIn) {
            if (bytes > capacity_ - waveCount_) {
                waveRead_ = write_;
                waveCount_ = capacity_;
            } else {
                waveCount_ += bytes;
            }
        }
    }

    void Copy(short* output, std::size_t start, std::size_t bytes, bool directSound) const {
        const std::size_t available = capacity_ - start;
        const std::size_t first = bytes < available ? bytes : available;
        std::memcpy(output, queue_ + start, first);
        // The legacy DS second span advances a short*, preserving its odd-byte overlap.
        const std::size_t secondOffset = directSound ? first / pcmSampleBytes * pcmSampleBytes : first;
        std::memcpy(reinterpret_cast<unsigned char*>(output) + secondOffset, queue_, bytes - first);
    }

    void FlushWaveIn() {
        waveRead_ = 0;
        waveCount_ = 0;
        write_ = 0;
    }

    void AdvanceWaveIn(std::size_t bytes) {
        waveRead_ = (waveRead_ + bytes) % capacity_;
        waveCount_ -= bytes;
        if (waveCount_ == 0) {
            FlushWaveIn();
        }
    }

    void Close() {
        SDL_AudioStream* stream = stream_;
        stream_ = nullptr;
        if (stream != nullptr) {
            SDL_PauseAudioStreamDevice(stream);
            SDL_SetAudioStreamPutCallback(stream, nullptr, nullptr);
            SDL_DestroyAudioStream(stream);
        }
        if (queue_ != nullptr) {
            SDL_free(queue_);
            queue_ = nullptr;
        }
        capacity_ = 0;
        if (ownsAudioReference_) {
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            ownsAudioReference_ = false;
        }
    }

    unsigned char* queue_ = nullptr;
    SDL_AudioStream* stream_ = nullptr;
    const VoiceCapturePolicy policy_;
    std::size_t capacity_ = 0;
    std::size_t write_ = 0;
    std::size_t waveRead_ = 0;
    std::size_t waveCount_ = 0;
    std::uint32_t wrapOffset_ = 0;
    std::uint32_t lastRead_ = 0;
    bool pendingWrap_ = false;
    bool recording_ = false;
    Failure failure_ = Failure::None;
    bool reportedFailure_ = false;
    bool ownsAudioReference_ = false;
};

}

IVoiceRecord* CreateVoiceRecordSDL(int sampleRate, VoiceCapturePolicy policy) {
    if (sampleRate <= 0 || (policy != VoiceCapturePolicy::DirectSound && policy != VoiceCapturePolicy::WaveIn)) {
        return nullptr;
    }
    const std::uint64_t capacity = policy == VoiceCapturePolicy::WaveIn
        ? waveInBufferBytes : static_cast<std::uint64_t>(sampleRate) * pcmSampleBytes;
    if (capacity > (std::numeric_limits<std::uint32_t>::max)()
            || capacity > (std::numeric_limits<std::size_t>::max)()) {
        return nullptr;
    }
    auto* record = new (std::nothrow) VoiceRecordSDL(policy);
    if (record == nullptr) {
        return nullptr;
    }
    if (!record->Init(sampleRate, static_cast<std::size_t>(capacity))) {
        record->Release();
        return nullptr;
    }
    return record;
}

}
