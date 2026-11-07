#include "../../GoldSrc-SDK/engine/mpg123_music.h"

#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <vector>

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "Check failed at line %d: %s (%s)\n", __LINE__, #condition, SDL_GetError()); \
    return EXIT_FAILURE; \
} } while (false)

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
    // Calibrate one second of known PCM against the same real dummy device.
    // VM scheduling can make its consumption clock slower than wall time.
    const SDL_AudioSpec referenceFormat{SDL_AUDIO_S16, 2, 44100};
    SDL_AudioStream *reference = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &referenceFormat, nullptr, nullptr);
    CHECK(reference);
    std::vector<short> referencePcm(88200, 0);
    CHECK(SDL_PutAudioStreamData(reference, referencePcm.data(), 176400));
    CHECK(SDL_FlushAudioStream(reference));
    const Uint64 referenceStart = SDL_GetTicks();
    CHECK(SDL_ResumeAudioStreamDevice(reference));
    while ((SDL_GetAudioStreamQueued(reference) > 0 || SDL_GetAudioStreamAvailable(reference) > 0) &&
           SDL_GetTicks() - referenceStart < 30000) {
        SDL_Delay(1);
    }
    CHECK(SDL_GetAudioStreamQueued(reference) == 0);
    CHECK(SDL_GetAudioStreamAvailable(reference) == 0);
    const Uint64 referenceDuration = SDL_GetTicks() - referenceStart;
    CHECK(player.Open(argv[1], false));
    const Uint64 startedAt = SDL_GetTicks();
    CHECK(player.Start());
    const Uint64 deadline = SDL_GetTicks() + 30000;
    while (player.IsPlaying() && SDL_GetTicks() < deadline) {
        SDL_Delay(5);
    }
    CHECK(!player.IsPlaying());
    // One second of PCM must reach the device; decoder EOF alone is too early.
    constexpr Uint64 schedulingToleranceMs = 50;
    const Uint64 playerDuration = SDL_GetTicks() - startedAt;
    std::printf("Reference drain %llu ms; music completion %llu ms.\n",
                static_cast<unsigned long long>(referenceDuration), static_cast<unsigned long long>(playerDuration));
    CHECK(playerDuration + schedulingToleranceMs >= referenceDuration);
    SDL_DestroyAudioStream(reference);
    CHECK(player.Open(argv[1], true));
    CHECK(player.Start());
    // The fixture lasts one second; this crosses EOF and verifies an actual loop.
    SDL_Delay(2000);
    CHECK(player.IsPlaying());
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
