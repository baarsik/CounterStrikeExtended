#pragma once

#include "gpu_capture.h"
#include <string>

namespace cse {
namespace render {

struct MetalTextureData {
    std::vector<std::shared_ptr<const TextureImage>> levels;
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

constexpr std::size_t kQuadCorners = 4;
constexpr std::array<std::uint32_t, 6> kQuadIndices{{0, 1, 2, 0, 2, 3}};

struct MetalDrawData {
    MetalVertexConstants vertex;
    std::shared_ptr<const SurfaceMesh> mesh;
    MetalFragmentConstants fragment;
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    Primitive topology = Primitive::Triangles;
    RasterState raster;
    bool discardedNonfinite = false;
    bool indexedQuads = false;
    // Canonical position and unit-zero UVs can be reconstructed by the compact vertex shader.
    bool compactSingleTexture = true;

    const std::vector<Vertex>& Vertices() const {
        return borrowedVertices ? *borrowedVertices : vertices;
    }
    const std::vector<std::uint32_t>& Indices() const {
        return borrowedIndices ? *borrowedIndices : indices;
    }

    std::size_t IndexCount() const {
        return indexedQuads ? Vertices().size() / kQuadCorners * kQuadIndices.size() : Indices().size();
    }

    // Borrowed only during synchronous encoding while the frame owns the packet.
    const std::vector<Vertex>* borrowedVertices = nullptr;
    const std::vector<std::uint32_t>* borrowedIndices = nullptr;
};

// Clamp a valid GL request without changing the retained sampler state.
float EffectiveMetalAnisotropy(float requested);

// Preparation is transactional: output is unchanged on validation failure.
bool PrepareMetalTexture(const TextureObject& object, const std::shared_ptr<const SharedPalette>& palette,
    MetalTextureData& output, std::string& error);
enum class MetalGeometryOwnership { Own, Borrow };

// Preparation is transactional. Borrow requires the source packet to stay alive and unchanged until encoding ends.
bool PrepareMetalDraw(const DrawPacket& draw, MetalDrawData& output, std::string& error,
    MetalGeometryOwnership ownership = MetalGeometryOwnership::Own);
// The session already owns a new result and discards it on failure.
bool PrepareMetalDrawInto(const DrawPacket& draw, MetalDrawData& fresh, std::string& error,
    MetalGeometryOwnership ownership);
}
}
