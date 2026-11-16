#pragma once
#include "NativeSdk.h"
#include <ncl_math/aabb.h>
#include <span>
#include <vector>

// Client geometry/shading stays in the owning map renderers; this adapter only lowers convex fans.
class MapImageService {
public:
    std::vector<CseRadarMapVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<CseRadarMapDraw> draws;
    std::vector<CseRadarMapPass> passes;
    CseRadarResult status = CSE_RADAR_OK;

    void BeginPass(const ncl_math::Aabb2& bounds, float minZ, float maxZ, bool bottom = false) {
        CseRadarMapPass pass{};
        pass.minX = bounds.min.x;
        pass.minY = bounds.min.y;
        pass.maxX = bounds.max.x;
        pass.maxY = bounds.max.y;
        pass.minZ = minZ;
        pass.maxZ = maxZ;
        pass.firstDraw = static_cast<std::uint32_t>(draws.size());
        pass.depthOrder = bottom ? CSE_RADAR_DEPTH_BOTTOMMOST : CSE_RADAR_DEPTH_TOPMOST;
        pass.clearColor = passes.empty() ? 1 : 0;
        passes.push_back(pass);
    }

    void Polygon(std::span<const CseRadarMapVertex> polygon, CseRadarTexture base = {}, CseRadarTexture lightmap = {}) {
        if (polygon.size() < 3) {
            return;
        }
        CseRadarMapDraw draw{};
        draw.firstIndex = static_cast<std::uint32_t>(indices.size());
        draw.base = base;
        draw.lightmap = lightmap;
        draw.textureFlags = (base.serial ? CSE_RADAR_MAP_USE_BASE : 0) |
            (lightmap.serial ? CSE_RADAR_MAP_USE_LIGHTMAP : 0);
        const auto first = static_cast<std::uint32_t>(vertices.size());
        vertices.insert(vertices.end(), polygon.begin(), polygon.end());
        for (std::uint32_t index = 1; index + 1 < polygon.size(); ++index) {
            indices.push_back(first);
            indices.push_back(first + index);
            indices.push_back(first + index + 1);
        }
        draw.indexCount = static_cast<std::uint32_t>(indices.size()) - draw.firstIndex;
        draws.push_back(draw);
        ++passes.back().drawCount;
    }

    CseRadarResult Build(bool textured, CseRadarTexture& image) {
        if (status != CSE_RADAR_OK) {
            return status;
        }
        const auto* service = NativeRadar_Renderer();
        if (!service) {
            return CSE_RADAR_UNAVAILABLE;
        }
        CseRadarMapImageBuild build{};
        build.size = sizeof(build);
        build.rendererEpoch = NativeRadar_RendererState().rendererEpoch;
        build.tone = textured ? CSE_RADAR_TONE_TEXTURED : CSE_RADAR_TONE_FLAT;
        build.vertexCount = static_cast<std::uint32_t>(vertices.size());
        build.indexCount = static_cast<std::uint32_t>(indices.size());
        build.drawCount = static_cast<std::uint32_t>(draws.size());
        build.passCount = static_cast<std::uint32_t>(passes.size());
        build.vertices = vertices.data();
        build.indices = indices.data();
        build.draws = draws.data();
        build.passes = passes.data();
        return service->BuildMapImage(&build, &image);
    }
};
