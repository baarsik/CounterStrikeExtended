#pragma once

#include "legacy_commands.h"

namespace cse {
namespace render {

enum class GpuStatus { Ok, InvalidValue, Unsupported, DeviceFailure };

struct alignas(16) GpuVertexConstants {
    Matrix4 modelView;
    Matrix4 projection;
    std::array<Matrix4, kTextureUnits> textureMatrix;
};

struct alignas(16) GpuFragmentConstants {
    std::array<std::array<float, 4>, kTextureUnits> unit{};
    std::array<std::array<float, 4>, kTextureUnits> components{};
    std::array<float, 4> fogColor{};
    std::array<float, 4> fog{};
    std::array<float, 4> fogAlpha{};
};

static_assert(kTextureUnits == 3, "Authored first shader's bounded three-unit contract");
static_assert(sizeof(GpuVertexConstants) == 320, "FXC five column-major matrix block");
static_assert(sizeof(GpuFragmentConstants) == 144, "FXC nine float4 fragment state block");
static_assert(offsetof(GpuFragmentConstants, fogColor) == 96, "Reflected fragment state offset");
static_assert(sizeof(Vertex) == 80 && offsetof(Vertex, texcoords) == 32, "Five FLOAT4 vertex attributes");

struct GpuDrawData {
    GpuVertexConstants vertex;
    GpuFragmentConstants fragment;
    std::vector<Vertex> vertices;
    std::array<std::shared_ptr<const TextureImage>, kTextureUnits> images;
    std::array<SamplerState, kTextureUnits> samplers;
    RasterState raster;
};

// Bounded first profile. Unsupported paths fail explicitly; no altered flat colors,
// quad diagonals, palette expansion, border clamp, mip fallback or precision conversion.
GpuStatus PrepareGpuDraw(const DrawPacket& draw, GpuDrawData& output);

}
}
