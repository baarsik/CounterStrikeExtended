// Test-only deterministic PCM producer. No production mixer implementation exists in Task 2.
#include "native_mix.h"
#include <SDL3/SDL.h>
#include <atomic>
std::atomic<int> fixtureRenderCount{0};
std::atomic<bool> fixtureBlockRender{false};
std::atomic<bool> fixtureRenderEntered{false};
std::atomic<bool> fixtureFailRender{false};
HRESULT A3dNativeMixer::Render(SHORT* stereo, DWORD frames) {
    fixtureRenderEntered = true;
    while (fixtureBlockRender) {
        SDL_Delay(1);
    }
    if (fixtureFailRender) {
        return E_INVALIDARG;
    }
    ++fixtureRenderCount;
    for (DWORD frame = 0; frame < frames; ++frame) {
        stereo[frame * 2] = 1234;
        stereo[frame * 2 + 1] = -2345;
    }
    return S_OK;
}
