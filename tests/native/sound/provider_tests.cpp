#include <atomic>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <thread>
#include "../../../GoldSrc-SDK/engine/sdl3_audio_backend.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "Sound provider check failed at line %d: %s (%s)\n", __LINE__, #condition, SDL_GetError()); \
        return 1; \
    } \
} while (false)

namespace {
constexpr int Frequency = 22050;
constexpr int Channels = 2;
constexpr std::int16_t SourceSample = 12345;
constexpr float SignedPcmScale = 32768.0f;
constexpr float ExpectedSample = static_cast<float>(SourceSample) / SignedPcmScale;

struct Capture {
    std::atomic<unsigned> samples{0};
    std::atomic<unsigned> incorrect{0};
    std::atomic<bool> incompatibleFormat{false};
    int frequency = Frequency;
};

void SDLCALL Observe(void *context, const SDL_AudioSpec *format, float *buffer, int bytes) {
    auto &capture = *static_cast<Capture *>(context);
    if (format->freq != capture.frequency || format->channels != Channels || bytes < 0
        || bytes % static_cast<int>(sizeof(float)) != 0) {
        capture.incompatibleFormat = true;
        return;
    }
    const auto count = static_cast<unsigned>(bytes / static_cast<int>(sizeof(float)));
    for (unsigned index = 0; index < count; ++index) {
        if (buffer[index] != ExpectedSample) {
            ++capture.incorrect;
        }
    }
    capture.samples += count;
}
} // namespace

int main() {
    CHECK(SDL_GetVersion() == SDL_VERSIONNUM(3, 4, 20));
    CHECK(SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy"));
    CHECK(SDL_SetHint(SDL_HINT_AUDIO_FREQUENCY, "22050"));
    CHECK(SDL_SetHint(SDL_HINT_AUDIO_CHANNELS, "2"));
    CHECK(SDL_SetHint(SDL_HINT_AUDIO_FORMAT, "F32"));
    CHECK(SDL_InitSubSystem(SDL_INIT_AUDIO));
    cse::SdlAudioBackend backend;
    CHECK(!backend.LockBuffer());
    CHECK(backend.Open(Frequency, Channels, cse::AudioRing::PlaybackPolicy::PreserveSamples));
    SDL_AudioSpec nativeFormat{};
    CHECK(SDL_GetAudioDeviceFormat(backend.Device(), &nativeFormat, nullptr));
    CHECK(nativeFormat.freq > 0 && nativeFormat.channels == Channels);
    backend.Close();
    // Compare exact samples with matching stream/device rates. Hardware-rate
    // conversion is a separate operation from preserving the mixer's input bytes.
    for (int cycle = 0; cycle < 3; ++cycle) {
        CHECK(backend.Open(nativeFormat.freq, Channels, cse::AudioRing::PlaybackPolicy::PreserveSamples));
        CHECK(backend.IsOpen() && backend.IsPaused());
        CHECK(backend.LockBuffer());
        for (int offset = 0; offset < cse::AudioRing::BufferBytes; offset += sizeof(SourceSample)) {
            std::memcpy(backend.Buffer() + offset, &SourceSample, sizeof(SourceSample));
        }
        backend.UnlockBuffer();
        const auto device = backend.Device();
        CHECK(device != 0);
        SDL_AudioSpec actual{};
        CHECK(SDL_GetAudioDeviceFormat(device, &actual, nullptr));
        CHECK(actual.freq == nativeFormat.freq && actual.channels == Channels);
        Capture capture;
        capture.frequency = nativeFormat.freq;
        CHECK(SDL_SetAudioPostmixCallback(device, Observe, &capture));
        CHECK(backend.SetPaused(false));
        const auto deadline = SDL_GetTicks() + 3000;
        while (capture.samples < 4096 && SDL_GetTicks() < deadline) {
            SDL_Delay(1);
        }
        CHECK(backend.SetPaused(true));
        CHECK(SDL_SetAudioPostmixCallback(device, nullptr, nullptr));
        if (capture.samples < 4096 || capture.incorrect != 0 || capture.incompatibleFormat) {
            std::fprintf(stderr, "Captured samples=%u incorrect=%u incompatible=%d device-frequency=%d\n",
                capture.samples.load(), capture.incorrect.load(), capture.incompatibleFormat.load(), actual.freq);
        }
        CHECK(capture.samples >= 4096 && capture.incorrect == 0 && !capture.incompatibleFormat);
        CHECK(backend.Buffer()[0] == static_cast<unsigned char>(SourceSample & UINT8_MAX));
        const int pausedPosition = backend.MonoSamplePosition();
        SDL_Delay(20);
        CHECK(backend.MonoSamplePosition() == pausedPosition);
        CHECK(backend.LockBuffer());
        std::atomic<bool> attempting{false};
        std::atomic<bool> acquired{false};
        std::thread writer([&] {
            attempting = true;
            if (backend.LockBuffer()) {
                acquired = true;
                backend.UnlockBuffer();
            }
        });
        while (!attempting) {
            SDL_Delay(1);
        }
        SDL_Delay(20);
        const bool blocked = !acquired;
        backend.UnlockBuffer();
        writer.join();
        CHECK(blocked && acquired);
        backend.Close();
        backend.Close();
        CHECK(!backend.IsOpen() && !backend.LockBuffer());
        CHECK(SDL_WasInit(SDL_INIT_AUDIO) == SDL_INIT_AUDIO);
    }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    CHECK(SDL_WasInit(SDL_INIT_AUDIO) == 0);
    std::puts("Native SDL effects preserve exact postmix samples, write exclusion, pause and owned shutdown");
    return 0;
}
