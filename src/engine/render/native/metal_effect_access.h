#pragma once
#include <SDL3/SDL_gpu.h>

namespace cse {
namespace render {
// Borrowed for a single rendering-thread operation; never retain across Resize or Shutdown.
struct MetalEffectAccess {
    SDL_GPUDevice* device = nullptr;
    SDL_GPUTexture* color = nullptr;
    SDL_GPUTexture* multisample = nullptr;
    unsigned width = 0;
    unsigned height = 0;
    SDL_GPUSampleCount samples = SDL_GPU_SAMPLECOUNT_1;
};
}
}
