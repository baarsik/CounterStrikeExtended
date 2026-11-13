// Test-only deterministic producer for output ownership tests; never linked with the real mixer.
#include "native_mix.h"
#include <SDL3/SDL.h>
#include <atomic>
std::atomic<int> fixtureRenderCount{0};
std::atomic<DWORD> fixtureRenderFrames[32]{};
std::atomic<Uint64> fixtureRenderTicks[32]{};
std::atomic<bool> fixtureBlockRender{false};
std::atomic<bool> fixtureRenderEntered{false};
std::atomic<bool> fixtureFailRender{false};
// This output-only fixture never creates buffers or owns a render list.
A3dNativeMixer::~A3dNativeMixer() = default;

HRESULT A3dNativeMixer::Render(SHORT* stereo, DWORD frames) {
    fixtureRenderEntered = true;
    while (fixtureBlockRender) {
        SDL_Delay(1);
    }
    if (fixtureFailRender) {
        return E_INVALIDARG;
    }
    const int call = fixtureRenderCount.load();
    if (call < 32) {
        fixtureRenderFrames[call] = frames;
        fixtureRenderTicks[call] = SDL_GetTicks();
    }
    ++fixtureRenderCount;
    for (DWORD frame = 0; frame < frames; ++frame) {
        stereo[frame * 2] = 1234;
        stereo[frame * 2 + 1] = -2345;
    }
    return S_OK;
}
