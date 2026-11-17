#pragma once

#include "surface_mesh.h"
#include <algorithm>
#include <map>

namespace cse {
namespace render {

// Map-scoped ownership, cleared before hunk addresses can be reused. Queued packets retain old meshes.
class SurfaceMeshCache {
public:
    std::shared_ptr<const SurfaceMesh> Get(const void* identity, const float* source, std::size_t count,
        std::size_t stride, std::string& error) {
        const auto found = meshes_.find(identity);
        if (found != meshes_.end()) {
            return found->second;
        }
        constexpr std::size_t kSurfaceComponents = 7;
        constexpr std::size_t kTriangleVertices = 3;
        if (!identity || !source || count < kTriangleVertices || count > kMaximumVertices ||
            count - 2 > kMaximumVertices / kTriangleVertices || stride != kSurfaceComponents) {
            error = "Invalid native world surface";
            return nullptr;
        }
        const auto bytes = count * sizeof(Vertex) + (count - 2) * kTriangleVertices * sizeof(std::uint32_t);
        if (bytes > kMaximumStorageBytes - bytes_) {
            error.clear(); // Preserve the immediate path when the optional persistent cache is full.
            return nullptr;
        }
        std::vector<Vertex> vertices(count);
        for (std::size_t index = 0; index < count; ++index) {
            const auto* input = source + index * stride;
            for (std::size_t component = 0; component < kSurfaceComponents; ++component) {
                if (!std::isfinite(input[component])) {
                    // Immutable meshes remain finite. Let the immediate path apply the common
                    // per-primitive policy rather than making an optional cache failure fatal.
                    error.clear();
                    return nullptr;
                }
            }
            auto& vertex = vertices[index];
            std::copy_n(input, 3, vertex.position.begin());
            vertex.texcoords[0] = {{input[3], input[4], 0, 1}};
            vertex.texcoords[1] = {{input[5], input[6], 0, 1}};
            vertex.texcoords[2] = vertex.texcoords[0];
        }
        auto mesh = SurfaceMesh::Create(std::move(vertices), error);
        if (mesh) {
            meshes_.emplace(identity, mesh);
            bytes_ += bytes;
        }
        return mesh;
    }

    void Clear() {
        meshes_.clear();
        bytes_ = 0;
    }

private:
    // Bounds CPU residency independently of retained textures; cache pressure never removes a feature.
    static constexpr std::size_t kMaximumStorageBytes = 128 * 1024 * 1024;
    std::map<const void*, std::shared_ptr<const SurfaceMesh>> meshes_;
    std::size_t bytes_ = 0;
};
}
}
