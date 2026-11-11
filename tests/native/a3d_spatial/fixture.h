#pragma once
#include "A3d3.h"
#include "A3dSource.h"
#include "native_provider.h"
#include "native_buffer.h"
#include "resman.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#define CHECK(value) \
    do { \
        if (!(value)) { \
            std::fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
            std::exit(1); \
        } \
    } while (0)

constexpr DWORD kSampleRate = 22050;
constexpr DWORD kSourceFrames = 256;
constexpr DWORD kRenderFrames = 128;
constexpr DWORD kStereoChannels = 2;

struct PcmOwner {
    A3dNativeMixer mixer;
    IDirectSoundBuffer* buffer = nullptr;
    A3dNativeBuffer* native = nullptr;
    PcmOwner() {
        WAVEFORMATEX format{WAVE_FORMAT_PCM, 1, kSampleRate, kSampleRate * sizeof(SHORT),
            sizeof(SHORT), 16, 0};
        DSBUFFERDESC description{};
        description.dwSize = sizeof(description);
        description.dwBufferBytes = kSourceFrames * sizeof(SHORT);
        description.lpwfxFormat = &format;
        CHECK(mixer.CreateBuffer(&description, &buffer) == S_OK);
        native = dynamic_cast<A3dNativeBuffer*>(buffer);
        CHECK(native);
        void* data = nullptr;
        DWORD bytes = 0;
        CHECK(buffer->Lock(0, description.dwBufferBytes, &data, &bytes, nullptr, nullptr, 0) == S_OK);
        constexpr int kAmplitude = 12000;
        constexpr int kPeriod = 31;
        for (DWORD i = 0; i < kSourceFrames; ++i) {
            const int sample = static_cast<int>(i % kPeriod) * (2 * kAmplitude / kPeriod) - kAmplitude;
            static_cast<SHORT*>(data)[i] = static_cast<SHORT>(sample);
        }
        CHECK(buffer->Unlock(data, bytes, nullptr, 0) == S_OK);
    }

    ~PcmOwner() {
        buffer->Release();
    }

    void Control(const A3DCTRL_SRC_SUPER& control) {
        CHECK(native->SetA3dSuperCtrl(const_cast<A3DCTRL_SRC_SUPER*>(&control), sizeof(control)) == S_OK);
        CHECK(std::memcmp(&native->voice_.control_, &control, sizeof(control)) == 0);
    }

    std::vector<SHORT> Render() {
        CHECK(buffer->Play(0, 0, DSBPLAY_LOOPING) == S_OK);
        std::vector<SHORT> result(kRenderFrames * kStereoChannels);
        CHECK(mixer.Render(result.data(), kRenderFrames) == S_OK);
        return result;
    }
};

inline CA3d* CreateParent() {
    IUnknown* provider = nullptr;
    CHECK(A3dCreateNativeProvider(&provider) == S_OK);
    IDirectSound* sound = nullptr;
    IA3d2* resource = nullptr;
    CHECK(provider->QueryInterface(IID_IDirectSound, reinterpret_cast<void**>(&sound)) == S_OK);
    CHECK(provider->QueryInterface(IID_IA3d2, reinterpret_cast<void**>(&resource)) == S_OK);
    auto* parent = new CA3d(FALSE);
    CHECK(parent->InitializeInterfaces(sound, resource, nullptr) == S_OK);
    parent->AddRef();
    provider->Release();
    return parent;
}

inline A3DCTRL_SRC_SUPER DirectControl() {
    A3DCTRL_SRC_SUPER result{};
    result.dwEnabled = 1;
    result.dwMode = 1;
    result.flPitch = 1;
    result.Left.flGain = 0.75f;
    result.Right.flGain = 0.5f;
    return result;
}
