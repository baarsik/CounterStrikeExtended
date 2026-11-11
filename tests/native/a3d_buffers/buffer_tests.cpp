#include "native_buffer.h"
#include "A3dSource.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <thread>
#include <string>
#include <vector>

#define CHECK(value) \
    do { \
        if (!(value)) { \
            std::fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
            std::exit(1); \
        } \
    } while (0)

WAVEFORMATEX Format(WORD bits = 16, WORD channels = 1) {
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = channels;
    format.nSamplesPerSec = 22050;
    format.wBitsPerSample = bits;
    format.nBlockAlign = channels * bits / 8;
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
    return format;
}

IDirectSoundBuffer* Buffer(A3dNativeMixer& mixer, DWORD bytes = 128, WORD bits = 16, WORD channels = 1) {
    auto format = Format(bits, channels);
    DSBUFFERDESC desc{};
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DSBCAPS_CTRL3D | DSBCAPS_CTRLFREQUENCY | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPAN;
    desc.dwBufferBytes = bytes;
    desc.lpwfxFormat = &format;
    IDirectSoundBuffer* buffer = nullptr;
    CHECK(mixer.CreateBuffer(&desc, &buffer) == S_OK && buffer);
    return buffer;
}

void Fill(IDirectSoundBuffer* buffer, DWORD bytes) {
    void* data = nullptr;
    DWORD size = 0;
    CHECK(buffer->Lock(0, bytes, &data, &size, nullptr, nullptr, 0) == S_OK && size == bytes);
    CHECK(reinterpret_cast<std::uintptr_t>(data) > UINT32_MAX);
    for (DWORD i = 0; i < bytes; ++i) {
        static_cast<BYTE*>(data)[i] = static_cast<BYTE>(i * 37 + 71);
    }
    CHECK(buffer->Unlock(data, size, nullptr, 0) == S_OK);
    IA3dDalBuffer* render = nullptr;
    CHECK(buffer->QueryInterface(IID_A3dNativeRenderBuffer, reinterpret_cast<void**>(&render)) == S_OK);
    A3DCTRL_SRC_SUPER control{};
    control.flPitch = 1;
    control.Left.flGain = 1;
    control.Right.flGain = 0.5f;
    CHECK(render->SetA3dSuperCtrl(&control, 0) == S_OK);
    control = {}; // The submitted record must already be copied.
    render->Release();
}

int main(int argc, char** argv) {
    CHECK(argc == 2);
    const std::string name = argv[1];
    A3dNativeMixer mixer;
    auto* buffer = Buffer(mixer);
    if (name == "public_buffer_roundtrip" || name == "high_address_storage") {
        Fill(buffer, 128);
        DSBCAPS caps{};
        caps.dwSize = sizeof(caps);
        CHECK(buffer->GetCaps(&caps) == S_OK && caps.dwBufferBytes == 128);
        WAVEFORMATEX format{};
        DWORD written = 0;
        CHECK(buffer->GetFormat(&format, sizeof(format), &written) == S_OK);
        CHECK(written == sizeof(format) && format.nSamplesPerSec == 22050 && format.nBlockAlign == 2);
        CHECK(buffer->SetVolume(-100) == S_OK);
        CHECK(buffer->SetPan(200) == S_OK);
        CHECK(buffer->SetFrequency(11025) == S_OK);
        LONG value = 0;
        DWORD frequency = 0;
        CHECK(buffer->GetVolume(&value) == S_OK && value == -100);
        CHECK(buffer->GetPan(&value) == S_OK && value == 200);
        CHECK(buffer->GetFrequency(&frequency) == S_OK && frequency == 11025);
        IDirectSound3DBuffer* spatial = nullptr;
        CHECK(buffer->QueryInterface(IID_IDirectSound3DBuffer, reinterpret_cast<void**>(&spatial)) == S_OK);
        CHECK(spatial->SetPosition(1, 2, 3, DS3D_IMMEDIATE) == S_OK);
        D3DVECTOR position{};
        CHECK(spatial->GetPosition(&position) == S_OK && position.x == 1 && position.y == 2 && position.z == 3);
        IUnknown* identity = nullptr;
        CHECK(spatial->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&identity)) == S_OK);
        CHECK(identity == static_cast<IUnknown*>(buffer));
        identity->Release();
        spatial->Release();
        void* unsupported = reinterpret_cast<void*>(1);
        CHECK(buffer->QueryInterface(IID_IDirectSoundNotify, &unsupported) == E_NOINTERFACE && !unsupported);
        CHECK(buffer->Initialize(nullptr, nullptr) != S_OK);
        CHECK(buffer->Play(0, 0, DSBPLAY_LOOPING) == S_OK);
        std::array<SHORT, 64> output{};
        CHECK(mixer.Render(output.data(), 32) == S_OK);
        bool nonzero = false;
        for (SHORT sample : output) {
            nonzero |= sample != 0;
        }
        CHECK(nonzero);
    } else if (name == "split_lock_wrap") {
        void* first = nullptr;
        void* second = nullptr;
        DWORD firstBytes = 0;
        DWORD secondBytes = 0;
        CHECK(buffer->Lock(120, 16, &first, &firstBytes, &second, &secondBytes, 0) == S_OK);
        CHECK(firstBytes == 8 && secondBytes == 8);
        std::memset(first, 11, firstBytes);
        std::memset(second, 22, secondBytes);
        CHECK(buffer->Unlock(first, firstBytes, second, secondBytes) == S_OK);
        CHECK(buffer->Lock(0, 0, &first, &firstBytes, &second, &secondBytes, DSBLOCK_ENTIREBUFFER) == S_OK);
        CHECK(firstBytes == 128 && !second && secondBytes == 0);
        CHECK(static_cast<BYTE*>(first)[0] == 22 && static_cast<BYTE*>(first)[120] == 11);
        CHECK(buffer->Unlock(first, firstBytes, second, secondBytes) == S_OK);
        CHECK(buffer->Lock(UINT32_MAX, 16, &first, &firstBytes, &second, &secondBytes, 0) != S_OK);
    } else if (name == "duplicate_lifetime") {
        Fill(buffer, 128);
        IDirectSoundBuffer* duplicate = nullptr;
        CHECK(mixer.DuplicateBuffer(buffer, &duplicate) == S_OK);
        buffer->Release();
        buffer = duplicate;
        CHECK(buffer->Play(0, 0, 0) == S_OK);
        std::array<SHORT, 128> output{};
        CHECK(mixer.Render(output.data(), 64) == S_OK);
        bool nonzero = false;
        for (SHORT sample : output) {
            nonzero |= sample != 0;
        }
        CHECK(nonzero);
    } else if (name == "stopped_loop_tail_cursor") {
        Fill(buffer, 128);
        CHECK(buffer->Play(0, 0, 0) == S_OK);
        std::array<SHORT, 512> output{};
        CHECK(mixer.Render(output.data(), 256) == S_OK);
        DWORD status = 99;
        DWORD cursor = 99;
        CHECK(buffer->GetStatus(&status) == S_OK && !(status & DSBSTATUS_PLAYING));
        CHECK(buffer->GetCurrentPosition(&cursor, nullptr) == S_OK && cursor == 128);
        CHECK(buffer->Play(0, 0, DSBPLAY_LOOPING) == S_OK);
        CHECK(mixer.Render(output.data(), 256) == S_OK);
        CHECK(buffer->GetStatus(&status) == S_OK && (status & DSBSTATUS_LOOPING));
        CHECK(buffer->GetCurrentPosition(&cursor, nullptr) == S_OK && cursor < 128);
        CHECK(buffer->Stop() == S_OK);
        output.fill(1);
        CHECK(mixer.Render(output.data(), 256) == S_OK);
        for (SHORT sample : output) {
            CHECK(sample == 0);
        }
    } else if (name == "duplicate_control_failure") {
        Fill(buffer, 128);
        IA3dDalBuffer* render = nullptr;
        CHECK(buffer->QueryInterface(IID_A3dNativeRenderBuffer, reinterpret_cast<void**>(&render)) == S_OK);
        A3DCTRL_SRC_SUPER control{};
        control.flPitch = 2;
        control.Left.flGain = 1;
        control.Right.flGain = 0.5f;
        CHECK(render->SetA3dSuperCtrl(&control, 0) == S_OK);
        render->Release();
        CHECK(buffer->SetFrequency(UINT32_MAX) == S_OK);
        IDirectSoundBuffer* duplicate = reinterpret_cast<IDirectSoundBuffer*>(1);
        CHECK(mixer.DuplicateBuffer(buffer, &duplicate) == E_INVALIDARG && !duplicate);
        CHECK(buffer->Play(0, 0, DSBPLAY_LOOPING) == S_OK);
        std::array<SHORT, 128> output{};
        CHECK(mixer.Render(output.data(), 64) == S_OK);
        bool nonzero = false;
        for (SHORT sample : output) {
            nonzero |= sample != 0;
        }
        CHECK(nonzero);
    } else if (name == "parent_lifetime") {
        auto* parent = new A3dNativeMixer;
        auto* child = Buffer(*parent);
        Fill(child, 128);
        CHECK(child->Play(0, 0, DSBPLAY_LOOPING) == S_OK);
        delete parent;
        DWORD status = UINT32_MAX;
        CHECK(child->GetStatus(&status) == S_OK && !(status & DSBSTATUS_PLAYING));
        CHECK(child->Play(0, 0, DSBPLAY_LOOPING) == E_ABORT);
        WAVEFORMATEX format{};
        CHECK(child->GetFormat(&format, sizeof(format), nullptr) == S_OK && format.nBlockAlign == 2);
        child->Release();
    } else if (name == "spatial_parameters") {
        IDirectSound3DBuffer* spatial = nullptr;
        CHECK(buffer->QueryInterface(IID_IDirectSound3DBuffer, reinterpret_cast<void**>(&spatial)) == S_OK);
        DS3DBUFFER parameters{};
        parameters.dwSize = sizeof(parameters);
        CHECK(spatial->GetAllParameters(&parameters) == S_OK);
        parameters.vPosition = {1, 2, 3};
        parameters.vVelocity = {4, 5, 6};
        parameters.vConeOrientation = {0, 1, 0};
        parameters.dwInsideConeAngle = 30;
        parameters.dwOutsideConeAngle = 90;
        parameters.lConeOutsideVolume = -1000;
        parameters.flMinDistance = 2;
        parameters.flMaxDistance = 100;
        parameters.dwMode = DS3DMODE_HEADRELATIVE;
        CHECK(spatial->SetAllParameters(&parameters, DS3D_IMMEDIATE) == S_OK);
        DS3DBUFFER result{};
        result.dwSize = sizeof(result);
        CHECK(spatial->GetAllParameters(&result) == S_OK);
        CHECK(std::memcmp(&parameters, &result, sizeof(result)) == 0);
        CHECK(spatial->SetMode(DS3DMODE_DISABLE, DS3D_DEFERRED) == E_NOTIMPL);
        DWORD mode = 99;
        CHECK(spatial->GetMode(&mode) == S_OK && mode == DS3DMODE_HEADRELATIVE);
        CHECK(spatial->SetMinDistance(200, DS3D_IMMEDIATE) == E_INVALIDARG);
        CHECK(spatial->SetVelocity(7, 8, 9, DS3D_IMMEDIATE) == S_OK);
        D3DVECTOR velocity{};
        CHECK(spatial->GetVelocity(&velocity) == S_OK && velocity.x == 7 && velocity.y == 8 && velocity.z == 9);
        CHECK(spatial->SetConeAngles(10, 20, DS3D_IMMEDIATE) == S_OK);
        DWORD inside = 0;
        DWORD outside = 0;
        CHECK(spatial->GetConeAngles(&inside, &outside) == S_OK && inside == 10 && outside == 20);
        spatial->Release();
    } else if (name == "duplicate_shared_lock") {
        IDirectSoundBuffer* duplicate = nullptr;
        CHECK(mixer.DuplicateBuffer(buffer, &duplicate) == S_OK);
        void* first = nullptr;
        DWORD count = 0;
        CHECK(buffer->Lock(0, 128, &first, &count, nullptr, nullptr, 0) == S_OK);
        static_cast<BYTE*>(first)[0] = 123;
        void* second = nullptr;
        DWORD otherCount = 0;
        CHECK(duplicate->Lock(0, 128, &second, &otherCount, nullptr, nullptr, 0) == E_INVALIDARG);
        CHECK(buffer->Unlock(first, count, nullptr, 0) == S_OK);
        CHECK(duplicate->Lock(0, 128, &second, &otherCount, nullptr, nullptr, 0) == S_OK);
        CHECK(second == first && static_cast<BYTE*>(second)[0] == 123);
        CHECK(duplicate->Unlock(second, otherCount, nullptr, 0) == S_OK);
        CHECK(buffer->SetCurrentPosition(126) == S_OK);
        DWORD cursor = 0;
        CHECK(buffer->GetCurrentPosition(&cursor, nullptr) == S_OK && cursor == 126);
        CHECK(duplicate->GetCurrentPosition(&cursor, nullptr) == S_OK && cursor == 0);
        CHECK(buffer->SetCurrentPosition(127) == E_INVALIDARG);
        duplicate->Release();
    } else if (name == "format_size_overflow") {
        auto format = Format();
        DSBUFFERDESC desc{};
        desc.dwSize = sizeof(desc);
        desc.dwBufferBytes = UINT32_MAX;
        desc.lpwfxFormat = &format;
        IDirectSoundBuffer* bad = reinterpret_cast<IDirectSoundBuffer*>(1);
        CHECK(mixer.CreateBuffer(&desc, &bad) == E_INVALIDARG && !bad);
        desc.dwBufferBytes = 128;
        constexpr DWORD unsupportedBufferFlag = 0x80000000u;
        desc.dwFlags = unsupportedBufferFlag;
        CHECK(mixer.CreateBuffer(&desc, &bad) == E_NOTIMPL && !bad);
        desc.dwFlags = 0;
        format.nBlockAlign = 0;
        CHECK(mixer.CreateBuffer(&desc, &bad) == E_INVALIDARG && !bad);
        format = Format();
        format.nAvgBytesPerSec = UINT32_MAX;
        CHECK(mixer.CreateBuffer(&desc, &bad) == E_INVALIDARG && !bad);
    } else if (name == "release_while_playing") {
        Fill(buffer, 128);
        CHECK(buffer->Play(0, 0, DSBPLAY_LOOPING) == S_OK);
        std::thread render([&] {
            std::array<SHORT, 512> output{};
            for (int i = 0; i < 200; ++i) {
                CHECK(mixer.Render(output.data(), 256) == S_OK);
            }
        });
        buffer->Release();
        buffer = nullptr;
        render.join();
        std::array<SHORT, 8> output{};
        CHECK(mixer.Render(output.data(), 4) == S_OK);
        for (SHORT sample : output) {
            CHECK(sample == 0);
        }
    } else {
        CHECK(false);
    }
    if (buffer) {
        buffer->Release();
    }
    std::printf("PASS %s\n", argv[1]);
}
