#pragma once

#include "gpu_capture.h"
#include <string>

namespace cse {
namespace render {

struct MetalTextureData {
    std::vector<TextureImage> levels;
    std::array<float, 4> components{{1, 1, 0, 0}};
    bool incomplete = false;
};

struct alignas(16) MetalFragmentConstants {
    GpuFragmentConstants legacy;
    // width, height, rectangle, anisotropy; coordinates remain homogeneous until fragment shading.
    std::array<std::array<float, 4>, kTextureUnits> texture{};
    // wrap S, wrap T, minification, magnification; preserves GL_CLAMP border semantics.
    std::array<std::array<float, 4>, kTextureUnits> sampling{};
};
static_assert(sizeof(MetalFragmentConstants) == 240, "Offline production Metal fragment layout");

struct alignas(16) MetalVertexConstants {
    Matrix4 modelView;
    Matrix4 projection;
    std::array<Matrix4, kTextureUnits> textureMatrix;
    std::array<float, 4> color{{1, 1, 1, 1}};
    std::array<std::array<float, 4>, kTextureUnits> uvOffset{};
};
static_assert(sizeof(MetalVertexConstants) == 384, "Offline production Metal vertex layout");

struct MetalDrawData {
    MetalVertexConstants vertex;
    std::shared_ptr<const SurfaceMesh> mesh;
    MetalFragmentConstants fragment;
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    Primitive topology = Primitive::Triangles;
    RasterState raster;
};

// Clamp a valid GL request without changing the retained sampler state.
float EffectiveMetalAnisotropy(float requested);

// Preparation is transactional: output is unchanged on validation failure.
bool PrepareMetalTexture(const TextureObject& object, const std::shared_ptr<const SharedPalette>& palette,
    MetalTextureData& output, std::string& error);
bool PrepareMetalDraw(const DrawPacket& draw, MetalDrawData& output, std::string& error);
}
}
