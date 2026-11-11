#include "native_output.h"
#include "native_mix.h"
#include <algorithm>
#include <cstring>

A3dNativeOutputOperations A3dNativeOutputOperations::Default() noexcept {
    return {SDL_InitSubSystem, SDL_QuitSubSystem, SDL_GetAudioPlaybackDevices, SDL_OpenAudioDeviceStream,
        SDL_DestroyAudioStream, SDL_ResumeAudioStreamDevice, SDL_PutAudioStreamData,
        [](SDL_ThreadFunction function, const char* name, void* context) {
            return SDL_CreateThread(function, name, context);
        }};
}

namespace {
GUID DeviceToken(SDL_AudioDeviceID id) {
    // This token is valid only during this process/module lifetime, while the SDL id remains live.
    // SDL physical ids are not reused during the audio subsystem lifetime.
    static const Uint64 nonce = SDL_GetPerformanceCounter();
    constexpr WORD kTokenDomain = 0xa3d0;
    constexpr WORD kTokenVersion = 1;
    GUID token{id, kTokenDomain, kTokenVersion, {}};
    std::memcpy(token.Data4, &nonce, sizeof(nonce));
    return token;
}
}

A3dNativeOutput::A3dNativeOutput(A3dNativeOutputOperations operations) noexcept : operations_(operations) {}

A3dNativeOutput::~A3dNativeOutput() {
    Close();
}

HRESULT A3dNativeOutput::DeviceGuid(DWORD index, GUID* guid) {
    if (!guid) {
        return E_POINTER;
    }
    *guid = {};
    if (!audioOwned_) {
        if (!operations_.init(SDL_INIT_AUDIO)) {
            return E_FAIL;
        }
        audioOwned_ = true;
    }
    int count = 0;
    SDL_AudioDeviceID* devices = operations_.devices(&count);
    const bool found = devices && count > 0 && index < static_cast<DWORD>(count);
    if (found) {
        *guid = DeviceToken(devices[index]);
    }
    SDL_free(devices);
    return found ? S_OK : E_INVALIDARG;
}

HRESULT A3dNativeOutput::Open(const GUID* device, A3dNativeMixer& mixer) {
    if (stream_) {
        return E_INVALIDARG;
    }
    const GUID requested = device ? *device : GUID{};
    status_ = S_OK;
    if (!audioOwned_) {
        if (!operations_.init(SDL_INIT_AUDIO)) {
            return E_FAIL;
        }
        audioOwned_ = true;
    }
    SDL_AudioDeviceID selected = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
    if (device) {
        int count = 0;
        SDL_AudioDeviceID* devices = operations_.devices(&count);
        selected = 0;
        for (int index = 0; devices && index < count; ++index) {
            const GUID candidate = DeviceToken(devices[index]);
            if (!std::memcmp(&candidate, &requested, sizeof(requested))) {
                selected = devices[index];
                break;
            }
        }
        SDL_free(devices);
        if (!selected) {
            Close();
            return E_INVALIDARG;
        }
    }
    mutex_ = SDL_CreateMutex();
    if (!mutex_) {
        Close();
        return E_OUTOFMEMORY;
    }
    constexpr int kSampleRate = 22050;
    const SDL_AudioSpec spec{SDL_AUDIO_S16, static_cast<int>(kChannels), kSampleRate};
    mixer_ = &mixer;
    stopping_ = false;
    stream_ = operations_.open(selected, &spec, Consume, this);
    if (!stream_) {
        Close();
        return E_FAIL;
    }
    producer_ = operations_.thread(Produce, "A3D PCM producer", this);
    if (!producer_) {
        Close();
        return E_FAIL;
    }
    return S_OK;
}

HRESULT A3dNativeOutput::Start() {
    if (!stream_) {
        return E_FAIL;
    }
    const HRESULT status = status_.load();
    if (FAILED(status)) {
        Close();
        return status;
    }
    if (!started_ && !operations_.resume(stream_)) {
        Close();
        return E_FAIL;
    }
    started_ = true;
    return S_OK;
}

void A3dNativeOutput::Close() noexcept {
    stopping_ = true;
    if (producer_) {
        SDL_WaitThread(producer_, nullptr);
        producer_ = nullptr;
    }
    if (stream_) {
        // The callback setter takes SDL's stream lock: fence an in-flight callback and remove
        // future callback admission before destroying the transport or its queue state.
        SDL_SetAudioStreamGetCallback(stream_, nullptr, nullptr);
        operations_.destroy(stream_);
        stream_ = nullptr;
    }
    SDL_DestroyMutex(mutex_);
    mutex_ = nullptr;
    mixer_ = nullptr;
    read_ = 0;
    write_ = 0;
    count_ = 0;
    offset_ = 0;
    started_ = false;
    if (audioOwned_) {
        operations_.quit(SDL_INIT_AUDIO);
        audioOwned_ = false;
    }
}

int A3dNativeOutput::Produce(void* context) {
    auto* output = static_cast<A3dNativeOutput*>(context);
    constexpr Uint32 kProducerPollMilliseconds = 2;
    while (!output->stopping_) {
        SDL_LockMutex(output->mutex_);
        const bool space = output->count_ < kBlockCount;
        SDL_UnlockMutex(output->mutex_);
        if (!space) {
            SDL_Delay(kProducerPollMilliseconds);
            continue;
        }
        Block prepared;
        const HRESULT status = output->mixer_->Render(prepared.data(), kFramesPerBlock);
        if (FAILED(status)) {
            output->status_ = status;
            output->stopping_ = true;
            break;
        }
        SDL_LockMutex(output->mutex_);
        output->queue_[output->write_] = prepared;
        output->write_ = (output->write_ + 1) % kBlockCount;
        ++output->count_;
        SDL_UnlockMutex(output->mutex_);
    }
    return 0;
}

void SDLCALL A3dNativeOutput::Consume(void* context, SDL_AudioStream* stream, int additional, int) {
    auto* output = static_cast<A3dNativeOutput*>(context);
    constexpr DWORD kFrameBytes = kChannels * sizeof(SHORT);
    constexpr DWORD kBlockBytes = kFramesPerBlock * kFrameBytes;
    // No mixing, allocation, source traversal or external event dispatch on SDL's callback.
    SDL_LockMutex(output->mutex_);
    while (!output->stopping_ && output->count_ && additional >= static_cast<int>(kFrameBytes)) {
        const DWORD bytes = std::min(static_cast<DWORD>(additional) / kFrameBytes * kFrameBytes,
            kBlockBytes - output->offset_);
        const auto* data = reinterpret_cast<const BYTE*>(output->queue_[output->read_].data());
        if (!output->operations_.put(stream, data + output->offset_, static_cast<int>(bytes))) {
            output->status_ = E_FAIL;
            output->stopping_ = true;
            break;
        }
        output->offset_ += bytes;
        additional -= static_cast<int>(bytes);
        if (output->offset_ == kBlockBytes) {
            output->offset_ = 0;
            output->read_ = (output->read_ + 1) % kBlockCount;
            --output->count_;
        }
    }
    SDL_UnlockMutex(output->mutex_);
}
