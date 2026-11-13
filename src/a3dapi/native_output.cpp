#include "native_output.h"
#include "native_mix.h"
#include "../public/cse_a3d_devices.h"
#include <limits>
#include <new>
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


namespace {
bool ValidDeviceLabel(const char* label, std::size_t bytes) noexcept {
    constexpr unsigned kContinuationMask = 0xc0;
    constexpr unsigned kContinuationTag = 0x80;
    constexpr unsigned kSurrogateFirst = 0xd800;
    constexpr unsigned kSurrogateLast = 0xdfff;
    constexpr unsigned kUnicodeMaximum = 0x10ffff;
    // RFC 3629 lead-byte ranges exclude overlong encodings and values beyond Unicode.
    constexpr unsigned kTwoByteLeadFirst = 0xc2;
    constexpr unsigned kTwoByteLeadLast = 0xdf;
    constexpr unsigned kTwoBytePayload = 0x1f;
    constexpr unsigned kTwoByteMinimum = 0x80;
    constexpr unsigned kThreeByteLeadFirst = 0xe0;
    constexpr unsigned kThreeByteLeadLast = 0xef;
    constexpr unsigned kThreeBytePayload = 0x0f;
    constexpr unsigned kThreeByteMinimum = 0x800;
    constexpr unsigned kFourByteLeadFirst = 0xf0;
    constexpr unsigned kFourByteLeadLast = 0xf4;
    constexpr unsigned kFourBytePayload = 0x07;
    constexpr unsigned kFourByteMinimum = 0x10000;
    std::size_t index = 0;
    while (index < bytes) {
        const unsigned lead = static_cast<unsigned char>(label[index++]);
        if (lead < kContinuationTag) {
            continue;
        }
        unsigned remaining = 0;
        unsigned codepoint = 0;
        unsigned minimum = 0;
        if (lead >= kTwoByteLeadFirst && lead <= kTwoByteLeadLast) {
            remaining = 1;
            codepoint = lead & kTwoBytePayload;
            minimum = kTwoByteMinimum;
        } else if (lead >= kThreeByteLeadFirst && lead <= kThreeByteLeadLast) {
            remaining = 2;
            codepoint = lead & kThreeBytePayload;
            minimum = kThreeByteMinimum;
        } else if (lead >= kFourByteLeadFirst && lead <= kFourByteLeadLast) {
            remaining = 3;
            codepoint = lead & kFourBytePayload;
            minimum = kFourByteMinimum;
        } else {
            return false;
        }
        if (remaining > bytes - index) {
            return false;
        }
        while (remaining--) {
            const unsigned next = static_cast<unsigned char>(label[index++]);
            if ((next & kContinuationMask) != kContinuationTag) {
                return false;
            }
            constexpr unsigned kContinuationBits = 6;
            constexpr unsigned kContinuationPayload = 0x3f;
            codepoint = (codepoint << kContinuationBits) | (next & kContinuationPayload);
        }
        if (codepoint < minimum || codepoint > kUnicodeMaximum ||
            (codepoint >= kSurrogateFirst && codepoint <= kSurrogateLast)) {
            return false;
        }
    }
    return true;
}

class NativeDeviceEnumerator final : public IA3dNativeDeviceEnumeratorV1 {
public:
    HRESULT Snapshot() noexcept {
        int count = 0;
        SDL_AudioDeviceID* ids = SDL_GetAudioPlaybackDevices(&count);
        if (!ids || count < 0) {
            SDL_free(ids);
            return E_FAIL;
        }
        HRESULT status = S_OK;
        if (static_cast<std::size_t>(count) > std::numeric_limits<std::size_t>::max() / sizeof(Entry)) {
            SDL_free(ids);
            return E_FAIL;
        }
        count_ = static_cast<DWORD>(count);
        if (count_) {
            entries_ = static_cast<Entry*>(SDL_malloc(sizeof(Entry) * count_));
            if (!entries_) {
                SDL_free(ids);
                return E_OUTOFMEMORY;
            }
            // All slots must be destructible even when a later name acquisition fails.
            for (DWORD index = 0; index < count_; ++index) {
                new (&entries_[index]) Entry{};
            }
        }
        for (DWORD index = 0; index < count_; ++index) {
            // SDL's name is thread-local scratch storage: consume it immediately on this thread.
            const char* label = SDL_GetAudioDeviceName(ids[index]);
            if (!label) {
                status = E_FAIL;
                break;
            }
            const std::size_t bytes = std::strlen(label);
            if (bytes >= std::numeric_limits<DWORD>::max() || !ValidDeviceLabel(label, bytes)) {
                status = E_FAIL;
                break;
            }
            Entry& entry = entries_[index];
            entry.name = static_cast<char*>(SDL_malloc(bytes + 1));
            if (!entry.name) {
                status = E_OUTOFMEMORY;
                break;
            }
            std::memcpy(entry.name, label, bytes + 1);
            entry.info = {sizeof(A3dNativeDeviceInfoV1), A3D_NATIVE_DEVICE_ENUMERATION_VERSION,
                DeviceToken(ids[index]), static_cast<DWORD>(bytes + 1), 0};
        }
        SDL_free(ids);
        return status;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** result) override {
        if (!result) {
            return E_POINTER;
        }
        *result = nullptr;
        if (iid != IID_IUnknown && iid != IID_IA3dNativeDeviceEnumeratorV1) {
            return E_NOINTERFACE;
        }
        *result = static_cast<IA3dNativeDeviceEnumeratorV1*>(this);
        AddRef();
        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return static_cast<ULONG>(A3dReferenceIncrement(&references_));
    }

    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG remaining = static_cast<ULONG>(A3dReferenceDecrement(&references_));
        if (!remaining) {
            // The caller's main-thread anchor owns this terminal transition.
            this->~NativeDeviceEnumerator();
            SDL_free(this);
        }
        return remaining;
    }

    HRESULT STDMETHODCALLTYPE GetCount(DWORD* count) override {
        if (!count) {
            return E_POINTER;
        }
        *count = count_;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetInfo(DWORD index, DWORD recordVersion, DWORD recordBytes,
        A3dNativeDeviceInfoV1* info, char* name, DWORD nameCapacity, DWORD* requiredNameBytes) override {
        constexpr DWORD kRecordBytes = sizeof(A3dNativeDeviceInfoV1);
        if (requiredNameBytes) {
            *requiredNameBytes = 0;
        }
        if (info) {
            std::memset(info, 0, std::min(recordBytes, kRecordBytes));
        }
        if (name && nameCapacity) {
            name[0] = '\0';
        }
        if (!info || !requiredNameBytes || (!name && nameCapacity)) {
            return E_POINTER;
        }
        if (recordVersion != A3D_NATIVE_DEVICE_ENUMERATION_VERSION || recordBytes != kRecordBytes ||
            index >= count_) {
            return E_INVALIDARG;
        }
        const Entry& entry = entries_[index];
        *requiredNameBytes = entry.info.nameBytes;
        if (nameCapacity < entry.info.nameBytes) {
            return S_FALSE;
        }
        std::memcpy(name, entry.name, entry.info.nameBytes);
        *info = entry.info;
        return S_OK;
    }

private:
    struct Entry {
        A3dNativeDeviceInfoV1 info;
        char* name;
    };
    ~NativeDeviceEnumerator() {
        if (entries_) {
            for (DWORD index = 0; index < count_; ++index) {
                SDL_free(entries_[index].name);
                entries_[index].~Entry();
            }
            SDL_free(entries_);
        }
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
    A3dNativeOwnerLease owner_{A3dNativeOwnerKind::Output};
    LONG references_ = 1;
    DWORD count_ = 0;
    Entry* entries_ = nullptr;
};
}

extern "C" HRESULT CSE_ABI_CALL CseCreateA3dNativeDeviceEnumeratorV1(
    IA3dNativeDeviceEnumeratorV1** enumerator) {
    if (!enumerator) {
        return E_POINTER;
    }
    *enumerator = nullptr;
    A3dNativeOwnerLease acquisition(A3dNativeOwnerKind::Output);
    // SDL has no cold-host main-thread oracle; SDL_SetMainReady is a host precondition.
    if (!SDL_IsMainThread() || !SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        return E_FAIL;
    }
    void* storage = SDL_malloc(sizeof(NativeDeviceEnumerator));
    if (!storage) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return E_OUTOFMEMORY;
    }
    auto* snapshot = new (storage) NativeDeviceEnumerator;
    const HRESULT status = snapshot->Snapshot();
    if (FAILED(status)) {
        snapshot->Release();
        return status;
    }
    *enumerator = snapshot;
    return S_OK;
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
    mixer_->outputStatus_ = S_OK;
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
    A3dNativeTrackOwner(A3dNativeOwnerKind::OutputWorker, 1);
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
        A3dNativeTrackOwner(A3dNativeOwnerKind::OutputWorker, -1);
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
    queuedBytes_ = 0;
    retryAfter_ = 0;
    started_ = false;
    if (audioOwned_) {
        operations_.quit(SDL_INIT_AUDIO);
        audioOwned_ = false;
    }
}

int A3dNativeOutput::Produce(void* context) {
    auto* output = static_cast<A3dNativeOutput*>(context);
    while (!output->stopping_) {
        // Retain DAL_A2D's polling interval, including the first refill.
        SDL_Delay(kPollMilliseconds);
        if (output->stopping_) {
            break;
        }
        // Match callback lock order. Count both prepared PCM and SDL-owned input
        // under the stream lock so transferring bytes cannot look like playback.
        SDL_LockAudioStream(output->stream_);
        SDL_LockMutex(output->mutex_);
        const int transported = SDL_GetAudioStreamQueued(output->stream_);
        const DWORD queued = output->queuedBytes_ + static_cast<DWORD>(std::max(transported, 0));
        if (transported < 0) {
            output->status_ = E_FAIL;
            output->mixer_->outputStatus_ = E_FAIL;
            output->stopping_ = true;
        }
        SDL_UnlockMutex(output->mutex_);
        SDL_UnlockAudioStream(output->stream_);
        if (output->stopping_ || queued >= kLowWaterBytes) {
            continue;
        }
        const DWORD bytes = (kBufferBytes - queued) / kFrameBytes * kFrameBytes;
        Prepared prepared;
        const HRESULT status = output->mixer_->Render(prepared.data(), bytes / kFrameBytes);
        SDL_LockMutex(output->mutex_);
        if (FAILED(status)) {
            output->status_ = status;
            output->mixer_->outputStatus_ = status;
            output->stopping_ = true;
        } else {
            const DWORD first = std::min(bytes, kBufferBytes - output->write_);
            auto* queue = reinterpret_cast<BYTE*>(output->queue_.data());
            const auto* data = reinterpret_cast<const BYTE*>(prepared.data());
            std::memcpy(queue + output->write_, data, first);
            std::memcpy(queue, data + first, bytes - first);
            output->write_ = (output->write_ + bytes) % kBufferBytes;
            output->queuedBytes_ += bytes;
        }
        SDL_UnlockMutex(output->mutex_);
    }
    return 0;
}

void SDLCALL A3dNativeOutput::Consume(void* context, SDL_AudioStream* stream, int additional, int) {
    A3dNativeOwnerLease callback(A3dNativeOwnerKind::Callback);
    auto* output = static_cast<A3dNativeOutput*>(context);
    // No mixing, allocation, source traversal or external event dispatch on SDL's callback.
    SDL_LockMutex(output->mutex_);
    while (!output->stopping_ && output->queuedBytes_ && additional >= static_cast<int>(kFrameBytes)) {
        if (SDL_GetTicks() < output->retryAfter_) {
            break;
        }
        const DWORD bytes = std::min({static_cast<DWORD>(additional) / kFrameBytes * kFrameBytes,
            kTransportBytes, output->queuedBytes_, kBufferBytes - output->read_});
        const auto* data = reinterpret_cast<const BYTE*>(output->queue_.data());
        if (!output->operations_.put(stream, data + output->read_, static_cast<int>(bytes))) {
            // Preserve prepared PCM for the next callback. DAL_A2D also retries
            // failed output copies; pacing bounds repeated SDL demand callbacks.
            output->status_ = E_FAIL;
            output->mixer_->outputStatus_ = E_FAIL;
            output->retryAfter_ = SDL_GetTicks() + kPollMilliseconds;
            break;
        }
        output->status_ = S_OK;
        output->mixer_->outputStatus_ = S_OK;
        output->retryAfter_ = 0;
        output->read_ = (output->read_ + bytes) % kBufferBytes;
        output->queuedBytes_ -= bytes;
        additional -= static_cast<int>(bytes);
    }
    SDL_UnlockMutex(output->mutex_);
}
