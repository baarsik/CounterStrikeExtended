#include <cstdio>
#include <cstring>
#include "../../../GoldSrc-SDK/engine/sound.h"
#include <SDL3/SDL.h>

volatile dma_t *shm = nullptr;
volatile dma_t sn{};
qboolean fakedma = 0;
int snd_blocked = 0;
int starts = 0;
int stops = 0;
int clears = 0;
int startupFailures = 0;

extern "C" qboolean SNDDMA_Init();
extern "C" void SNDDMA_Shutdown();
extern "C" int SNDDMA_GetDMAPos();
extern "C" void Snd_AcquireBuffer();
extern "C" void Snd_ReleaseBuffer();
extern "C" void S_BlockSound();
extern "C" void S_UnblockSound();

// Owned engine callbacks drive the production DMA adapter's focus/lifecycle entry points.
extern "C" void S_Startup() {
    ++starts;
    if (!SNDDMA_Init()) {
        ++startupFailures;
    }
}

extern "C" void S_Shutdown() {
    ++stops;
    SNDDMA_Shutdown();
    shm = nullptr;
}

extern "C" void S_ClearBuffer() {
    ++clears;
    if (shm && shm->buffer && SNDDMA_LockBuffer()) {
        std::memset(shm->buffer, 0, static_cast<std::size_t>(shm->samples) * sizeof(short));
        SNDDMA_UnlockBuffer();
    }
}

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "DMA provider check failed at line %d: %s (%s)\n", __LINE__, #condition, SDL_GetError()); \
        return 1; \
    } \
} while (false)

int main() {
    CHECK(SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy"));
    CHECK(SDL_InitSubSystem(SDL_INIT_AUDIO));
    for (int cycle = 0; cycle < 3; ++cycle) {
        S_BlockSound();
        CHECK(snd_blocked == 1);
        Snd_AcquireBuffer();
        CHECK(starts == cycle + 1 && startupFailures == 0);
        CHECK(shm == &sn && shm->soundalive && shm->buffer && shm->samplebits == 16 && shm->channels == 2);
        CHECK(shm->speed == 11025 && shm->dmaspeed == 22050 && shm->samples == 32768);
        CHECK(SNDDMA_GetDMAPos() == 0);
        SDL_Delay(20);
        CHECK(SNDDMA_GetDMAPos() == 0);
        Snd_AcquireBuffer();
        CHECK(starts == cycle + 1);
        S_UnblockSound();
        const auto deadline = SDL_GetTicks() + 3000;
        while (SNDDMA_GetDMAPos() == 0 && SDL_GetTicks() < deadline) {
            SDL_Delay(1);
        }
        CHECK(SNDDMA_GetDMAPos() > 0);
        S_BlockSound();
        const int pausedPosition = SNDDMA_GetDMAPos();
        SDL_Delay(20);
        CHECK(SNDDMA_GetDMAPos() == pausedPosition);
        Snd_ReleaseBuffer();
        CHECK(stops == cycle && shm != nullptr);
        Snd_ReleaseBuffer();
        CHECK(stops == cycle + 1 && clears == cycle + 1 && shm == nullptr);
        CHECK(!sn.soundalive && sn.buffer == nullptr && SDL_WasInit(SDL_INIT_AUDIO) == SDL_INIT_AUDIO);
        S_UnblockSound();
        CHECK(snd_blocked == 0);
        SNDDMA_Shutdown();
    }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    CHECK(SDL_WasInit(SDL_INIT_AUDIO) == 0);
    std::puts("Production native DMA adapter preserves format, nested ownership, focus pause and shutdown");
    return 0;
}
