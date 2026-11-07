#include "../../GoldSrc-SDK/engine/sdl3_audio_backend.h"
#include <cassert>

int main() {
    cse::SdlAudioBackend backend;
    assert(SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "cse-unavailable-driver"));
    assert(!backend.Open(22050, 2));
    assert(!backend.IsOpen());
    assert(SDL_WasInit(SDL_INIT_AUDIO) == 0);
    assert(SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy"));
    // Failure must not leave an owned subsystem reference or a live stream.
    assert(!backend.Open(0, 2));
    assert(!backend.IsOpen());
    assert(SDL_WasInit(SDL_INIT_AUDIO) == 0);
    // Force stream creation failure after successful subsystem acquisition.
    constexpr int invalidChannelCount = 257;
    assert(!backend.Open(22050, invalidChannelCount));
    assert(!backend.IsOpen());
    assert(SDL_WasInit(SDL_INIT_AUDIO) == 0);
    // Acquire our own reference even when another consumer already owns audio.
    assert(SDL_InitSubSystem(SDL_INIT_AUDIO));
    for (int iteration = 0; iteration < 3; ++iteration) {
        assert(backend.Open(22050, 2));
        assert(backend.Open(22050, 2));
        assert(backend.IsOpen());
        assert(backend.IsPaused());
        assert(backend.SetPaused(false));
        SDL_Delay(50);
        assert(!backend.IsPaused());
        assert(backend.MonoSamplePosition() > 0);
        assert(backend.SetPaused(true));
        const int position = backend.MonoSamplePosition();
        SDL_Delay(30);
        assert(backend.MonoSamplePosition() == position);
        backend.Close();
        backend.Close();
        assert(!backend.IsOpen());
        assert(SDL_WasInit(SDL_INIT_AUDIO) == SDL_INIT_AUDIO);
    }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    assert(SDL_WasInit(SDL_INIT_AUDIO) == 0);
    assert(backend.Open(22050, 2));
    backend.Close();
    assert(SDL_WasInit(SDL_INIT_AUDIO) == 0);
}
