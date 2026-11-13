#include "../../src/engine/mpg123_music.h"

#include <SDL3/SDL.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "Check failed at line %d: %s (%s)\n", __LINE__, #condition, SDL_GetError()); \
    return EXIT_FAILURE; \
} } while (false)

struct DeviceClock {
    std::atomic<Uint64> frames{0};
    std::atomic<Uint64> maximumBlockFrames{0};
    std::atomic<bool> formatChanged{false};
    int frequency = 0;
};

static void SDLCALL CountDeviceFrames(void *context, const SDL_AudioSpec *format, float *, int bytes) {
    auto &clock = *static_cast<DeviceClock *>(context);
    if (format->freq != clock.frequency || format->channels <= 0 || bytes < 0) {
        clock.formatChanged = true;
        return;
    }
    const auto frameBytes = static_cast<std::size_t>(format->channels) * sizeof(float);
    const auto frames = static_cast<Uint64>(static_cast<std::size_t>(bytes) / frameBytes);
    if (frames > clock.maximumBlockFrames) {
        clock.maximumBlockFrames = frames;
    }
    clock.frames += frames;
}

int main(int argc, char **argv) {
    CHECK(argc == 3);
    CHECK(SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy"));
    CHECK(SDL_InitSubSystem(SDL_INIT_AUDIO));
    cse::Mpg123MusicPlayer player;
    CHECK(player.Init());
    CHECK(player.Init());
    CHECK(!player.Open("cse-missing-track.mp3", false));
    CHECK(!player.IsPlaying());
    CHECK(!player.Open(argv[2], true));
    CHECK(!player.IsPlaying());
    for (int count = 0; count < 8; ++count) {
        CHECK(player.Open(argv[1], true));
        CHECK(!player.IsPlaying()); // Primed streams must stay silent until Start.
        player.SetVolume(0.25f);
        CHECK(player.Start());
        CHECK(player.IsPlaying());
        player.Pause(true);
        CHECK(!player.IsPlaying());
        SDL_Delay(15);
        player.Pause(false);
        CHECK(player.IsPlaying());
        player.Stop();
        player.Stop();
        CHECK(!player.IsPlaying());
    }
    // Observe the real device's sample clock, which keeps running even when
    // this logical stream is silent. Sequential wall-time calibrations vary
    // with VM scheduling and cannot establish early completion reliably.
    const SDL_AudioSpec referenceFormat{SDL_AUDIO_S16, 2, 44100};
    DeviceClock clock;
    std::unique_ptr<SDL_AudioStream, decltype(&SDL_DestroyAudioStream)> referenceOwner(
        SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &referenceFormat, nullptr, nullptr),
        SDL_DestroyAudioStream);
    SDL_AudioStream *reference = referenceOwner.get();
    CHECK(reference);
    SDL_AudioSpec deviceFormat{};
    CHECK(SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(reference), &deviceFormat, nullptr));
    clock.frequency = deviceFormat.freq;
    CHECK(SDL_SetAudioPostmixCallback(SDL_GetAudioStreamDevice(reference), CountDeviceFrames, &clock));
    const auto referenceSamples = static_cast<std::size_t>(referenceFormat.freq) * referenceFormat.channels;
    std::vector<short> referencePcm(referenceSamples, 0);
    CHECK(SDL_PutAudioStreamData(reference, referencePcm.data(),
                                static_cast<int>(referencePcm.size() * sizeof(short))));
    CHECK(SDL_FlushAudioStream(reference));
    const Uint64 referenceStart = SDL_GetTicks();
    CHECK(SDL_ResumeAudioStreamDevice(reference));
    while ((SDL_GetAudioStreamQueued(reference) > 0 || SDL_GetAudioStreamAvailable(reference) > 0) &&
           SDL_GetTicks() - referenceStart < 30000) {
        SDL_Delay(1);
    }
    CHECK(SDL_GetAudioStreamQueued(reference) == 0);
    CHECK(SDL_GetAudioStreamAvailable(reference) == 0);
    CHECK(player.Open(argv[1], false));
    const Uint64 startedAtFrame = clock.frames;
    CHECK(player.Start());
    const Uint64 deadline = SDL_GetTicks() + 30000;
    while (player.IsPlaying() && SDL_GetTicks() < deadline) {
        SDL_Delay(5);
    }
    CHECK(!player.IsPlaying());
    // The fixture contains exactly one second of PCM. Allow one device block
    // for the phase between Start and the first mixer callback; decoder EOF
    // with the producer's remaining 200 ms still queued must fail this check.
    const Uint64 consumedFrames = clock.frames - startedAtFrame;
    const Uint64 maximumBlockFrames = clock.maximumBlockFrames;
    std::printf("Music completion after %llu device frames at %d Hz (block <= %llu frames).\n",
                static_cast<unsigned long long>(consumedFrames), clock.frequency,
                static_cast<unsigned long long>(maximumBlockFrames));
    CHECK(!clock.formatChanged);
    CHECK(maximumBlockFrames > 0);
    constexpr Uint64 producerQueueMilliseconds = 200;
    constexpr Uint64 millisecondsPerSecond = 1000;
    CHECK(maximumBlockFrames * millisecondsPerSecond
          < static_cast<Uint64>(clock.frequency) * producerQueueMilliseconds);
    CHECK(consumedFrames + maximumBlockFrames >= static_cast<Uint64>(clock.frequency));
    CHECK(player.Open(argv[1], true));
    const Uint64 loopStartFrame = clock.frames;
    CHECK(player.Start());
    // Cross two seconds of output-device time, independently of VM wall time.
    const Uint64 loopDeadline = SDL_GetTicks() + 30000;
    const Uint64 loopFrames = static_cast<Uint64>(clock.frequency) * 2;
    while (clock.frames - loopStartFrame < loopFrames && SDL_GetTicks() < loopDeadline) {
        SDL_Delay(5);
    }
    CHECK(clock.frames - loopStartFrame >= loopFrames);
    CHECK(player.IsPlaying());
    CHECK(SDL_SetAudioPostmixCallback(SDL_GetAudioStreamDevice(reference), nullptr, nullptr));
    referenceOwner.reset();
    CHECK(player.Open(argv[1], false)); // Replace a live stream without reusing its queued PCM.
    CHECK(!player.IsPlaying());
    CHECK(player.Start());
    player.SetVolume(-1.0f);
    player.SetVolume(2.0f);
    player.Shutdown();
    player.Shutdown();
    CHECK(SDL_WasInit(SDL_INIT_AUDIO) == SDL_INIT_AUDIO);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    CHECK(SDL_WasInit(SDL_INIT_AUDIO) == 0);
    CHECK(player.Open(argv[1], true));
    CHECK(player.Start());
    player.Shutdown();
    CHECK(SDL_WasInit(SDL_INIT_AUDIO) == 0);
    std::puts("Music priming, replacement, pause, EOF and shutdown passed.");
    return EXIT_SUCCESS;
}
