#include "native_buffer.h"
#include "native_output.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define CHECK(value) \
    do { \
        if (!(value)) { \
            std::fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
            std::exit(1); \
        } \
    } while (0)

namespace {
SDL_AudioStream* stream = nullptr;
bool failPut = false;

SDL_AudioStream* Open(SDL_AudioDeviceID, const SDL_AudioSpec* spec, SDL_AudioStreamCallback callback,
    void* context) {
    stream = SDL_CreateAudioStream(spec, spec);
    CHECK(stream && SDL_SetAudioStreamGetCallback(stream, callback, context));
    return stream;
}

bool Put(SDL_AudioStream* target, const void* data, int bytes) {
    return !failPut && SDL_PutAudioStreamData(target, data, bytes);
}

struct Voice {
    A3dNativeMixer mixer;
    WAVEFORMATEX format{WAVE_FORMAT_PCM, 1, 22050, 44100, 2, 16, 0};
    DSBUFFERDESC description{};
    IDirectSoundBuffer* buffer = nullptr;

    Voice() {
        description.dwSize = sizeof(description);
        description.dwBufferBytes = 16384;
        description.lpwfxFormat = &format;
        CHECK(mixer.CreateBuffer(&description, &buffer) == S_OK);
        void* data = nullptr;
        DWORD bytes = 0;
        CHECK(buffer->Lock(0, description.dwBufferBytes, &data, &bytes, nullptr, nullptr, 0) == S_OK);
        std::fill_n(static_cast<SHORT*>(data), bytes / sizeof(SHORT), SHORT{12000});
        CHECK(buffer->Unlock(data, bytes, nullptr, 0) == S_OK);
        Control(0, 1, 0);
        CHECK(buffer->Play(0, 0, DSBPLAY_LOOPING) == S_OK);
        std::array<SHORT, 2> warmup{};
        CHECK(mixer.Render(warmup.data(), 1) == S_OK);
        Control(1, 0.75f, 0.0005f);
    }

    ~Voice() {
        buffer->Release();
    }

    void Control(float gain, float pitch, float delay) {
        IA3dDalBuffer* controlView = nullptr;
        CHECK(buffer->QueryInterface(IID_A3dNativeRenderBuffer,
            reinterpret_cast<void**>(&controlView)) == S_OK);
        A3DCTRL_SRC_SUPER control{};
        control.flPitch = pitch;
        control.Left.flGain = gain;
        control.Right.flGain = gain;
        control.Left.flDelay = delay;
        CHECK(controlView->SetA3dSuperCtrl(&control, 0) == S_OK);
        controlView->Release();
    }
};

void Read(SHORT* data, int bytes) {
    const Uint64 end = SDL_GetTicks() + 2000;
    int read = 0;
    while (read < bytes && SDL_GetTicks() < end) {
        const int copied = SDL_GetAudioStreamData(stream, reinterpret_cast<BYTE*>(data) + read, bytes - read);
        CHECK(copied >= 0);
        read += copied;
        if (read < bytes) {
            SDL_Delay(1);
        }
    }
    CHECK(read == bytes);
}
}

int main(int argc, char** argv) {
    CHECK(argc == 2);
    Voice voice;
    auto operations = A3dNativeOutputOperations::Default();
    operations.open = Open;
    operations.put = Put;
    A3dNativeOutput output(operations);
    CHECK(output.Open(nullptr, voice.mixer) == S_OK);
    SDL_Delay(50);
    if (!std::strcmp(argv[1], "output_retained_ramp_pcm")) {
        Voice retained;
        std::array<SHORT, 4096> expected{};
        std::array<SHORT, 4096> actual{};
        CHECK(retained.mixer.Render(expected.data(), 2048) == S_OK);
        Read(actual.data(), sizeof(actual));
        CHECK(actual == expected);
        // Constant input and the retained gain attack make a hand-checked slope:
        // halfway through the refill remains quieter than its final quarter.
        CHECK(actual[1024] > 0 && actual[1024] < actual[3072]);
    } else if (!std::strcmp(argv[1], "output_failure_reaches_buffers")) {
        Voice retained;
        std::array<SHORT, 4096> expected{};
        CHECK(retained.mixer.Render(expected.data(), 2048) == S_OK);
        failPut = true;
        std::array<SHORT, 512> data{};
        CHECK(SDL_GetAudioStreamData(stream, data.data(), sizeof(data)) == 0);
        DWORD status = UINT32_MAX;
        CHECK(voice.buffer->GetStatus(&status) == E_FAIL && status == 0);
        CHECK(voice.buffer->Play(0, 0, DSBPLAY_LOOPING) == E_FAIL);
        IDirectSoundBuffer* created = nullptr;
        CHECK(voice.mixer.CreateBuffer(&voice.description, &created) == E_FAIL && !created);
        CHECK(voice.mixer.DuplicateBuffer(voice.buffer, &created) == E_FAIL && !created);
        failPut = false;
        SDL_Delay(25);
        Read(data.data(), sizeof(data));
        CHECK(std::equal(data.begin(), data.end(), expected.begin()));
        CHECK(voice.buffer->GetStatus(&status) == S_OK && (status & DSBSTATUS_PLAYING));
        CHECK(voice.buffer->Play(0, 0, DSBPLAY_LOOPING) == S_OK);
        CHECK(voice.mixer.CreateBuffer(&voice.description, &created) == S_OK && created);
        created->Release();
    } else if (!std::strcmp(argv[1], "output_terminal_render_reaches_buffers")) {
        // A 4410-frame delay cannot be absorbed by a 2048-frame refill. The
        // production mixer's admission returns E_INVALIDARG before kernel entry.
        voice.Control(1, 1, 0.2f);
        std::array<SHORT, 4096> data{};
        Read(data.data(), sizeof(data));
        const Uint64 end = SDL_GetTicks() + 2000;
        while (SUCCEEDED(output.Status()) && SDL_GetTicks() < end) {
            SDL_Delay(1);
        }
        CHECK(output.Status() == E_INVALIDARG);
        DWORD status = UINT32_MAX;
        CHECK(voice.buffer->GetStatus(&status) == E_INVALIDARG && status == 0);
        CHECK(voice.buffer->Play(0, 0, DSBPLAY_LOOPING) == E_INVALIDARG);
        IDirectSoundBuffer* created = nullptr;
        CHECK(voice.mixer.CreateBuffer(&voice.description, &created) == E_INVALIDARG && !created);
    } else {
        CHECK(false);
    }
    output.Close();
    std::printf("PASS %s\n", argv[1]);
}
