#pragma once

#include "legacy_commands.h"
#include <cmath>
#include <string>
#include <utility>

namespace cse {
namespace render {

// Immutable convex world/brush polygon. Current entity color belongs to the draw, not the mesh.
// Keep its original boundary as well as fan indices so wireframe retains GL_POLYGON edges.
class SurfaceMesh {
public:
    static std::shared_ptr<const SurfaceMesh> Create(std::vector<Vertex> vertices, std::string& error) {
        constexpr std::size_t kTriangleVertices = 3;
        if (vertices.size() < kTriangleVertices || vertices.size() > kMaximumVertices ||
            vertices.size() - 2 > kMaximumVertices / kTriangleVertices) {
            error = "Invalid or oversized native surface polygon";
            return nullptr;
        }
        for (auto& vertex : vertices) {
            for (auto value : vertex.position) {
                if (!std::isfinite(value)) {
                    error = "Nonfinite native surface position";
                    return nullptr;
                }
            }
            for (const auto& uv : vertex.texcoords) {
                for (auto value : uv) {
                    if (!std::isfinite(value)) {
                        error = "Nonfinite native surface texture coordinate";
                        return nullptr;
                    }
                }
            }
            vertex.color = {{1, 1, 1, 1}};
        }
        std::vector<std::uint32_t> indices;
        indices.reserve((vertices.size() - 2) * kTriangleVertices);
        for (std::size_t index = 1; index + 1 < vertices.size(); ++index) {
            indices.push_back(0);
            indices.push_back(static_cast<std::uint32_t>(index));
            indices.push_back(static_cast<std::uint32_t>(index + 1));
        }
        error.clear();
        return std::shared_ptr<const SurfaceMesh>(new SurfaceMesh(std::move(vertices), std::move(indices)));
    }

    const std::vector<Vertex>& Vertices() const {
        return vertices_;
    }
    const std::vector<std::uint32_t>& Indices() const {
        return indices_;
    }

private:
    SurfaceMesh(std::vector<Vertex> vertices, std::vector<std::uint32_t> indices)
        : vertices_(std::move(vertices)), indices_(std::move(indices)) {
    }
    const std::vector<Vertex> vertices_;
    const std::vector<std::uint32_t> indices_;
};
}
}
