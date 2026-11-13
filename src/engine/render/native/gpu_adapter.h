#pragma once

#include "gpu_capture.h"
#include <SDL3/SDL.h>
#include <string>

namespace cse {
namespace render {

struct GpuOfflineShaders {
    std::vector<std::uint8_t> vertex;
    std::vector<std::uint8_t> fragment;
    SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_DXBC;
};

// The caller owns the device for this synchronous call. All transfer/pipeline/image
// resources are private and released after fence completion. No window/presentation.
GpuStatus RenderGpuOffscreen(SDL_GPUDevice* device, const GpuOfflineShaders& shaders,
    const DrawPacket& draw, std::uint32_t width, std::uint32_t height,
    std::vector<std::uint8_t>& rgbaTopFirst, std::string& error);

}
}
