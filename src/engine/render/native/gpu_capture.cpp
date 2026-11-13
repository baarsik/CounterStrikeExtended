#include "gpu_capture.h"
#include <cmath>

namespace cse {
namespace render {
namespace {
bool Finite(const Matrix4& matrix) {
    for (auto value : matrix.values) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    return true;
}

bool AffineTexture(const Matrix4& matrix) {
    return Finite(matrix) && matrix.values[3] == 0 && matrix.values[7] == 0 &&
        matrix.values[11] == 0 && matrix.values[15] == 1;
}
}

GpuStatus PrepareGpuDraw(const DrawPacket& draw, GpuDrawData& output) {
    const auto& rasterInput = draw.raster;
    if (rasterInput.depthFunction > Compare::Always || rasterInput.alphaFunction > Compare::Always ||
        rasterInput.blendSource > BlendFactor::InverseDestinationColor ||
        rasterInput.blendDestination > BlendFactor::InverseDestinationColor || rasterInput.cull > Cull::FrontAndBack ||
        rasterInput.fogMode > FogMode::Linear || rasterInput.scissor[2] < 0 || rasterInput.scissor[3] < 0) {
        return GpuStatus::InvalidValue;
    }
    if (draw.shade != Shade::Smooth || draw.raster.shade != Shade::Smooth ||
        draw.primitive != Primitive::Triangles || draw.raster.polygonMode[0] != PolygonMode::Fill ||
        draw.raster.polygonMode[1] != PolygonMode::Fill || draw.raster.enabled.back()) {
        return GpuStatus::Unsupported;
    }
    if (!Finite(draw.modelView) || !Finite(draw.projection) || draw.raster.viewport[2] <= 0 ||
        draw.raster.viewport[3] <= 0 || !std::isfinite(draw.raster.depthRange[0]) ||
        !std::isfinite(draw.raster.depthRange[1]) || draw.raster.depthRange[0] < 0 ||
        draw.raster.depthRange[1] > 1 || draw.raster.depthRange[0] > draw.raster.depthRange[1]) {
        return GpuStatus::InvalidValue;
    }
    if (draw.raster.enabled[static_cast<std::size_t>(Capability::CullFace)] && draw.raster.cull == Cull::FrontAndBack) {
        return GpuStatus::Unsupported;
    }
    GpuDrawData prepared;
    Primitive topology = Primitive::Triangles;
    if (ExpandTopology(draw, prepared.vertices, topology) != Status::Ok) {
        return GpuStatus::InvalidValue;
    }
    for (const auto& vertex : prepared.vertices) {
        for (auto value : vertex.position) {
            if (!std::isfinite(value)) {
                return GpuStatus::InvalidValue;
            }
        }
        for (auto value : vertex.color) {
            if (!std::isfinite(value) || value < 0 || value > 1) {
                return GpuStatus::Unsupported;
            }
        }
    }
    prepared.vertex.modelView = draw.modelView;
    prepared.vertex.projection = draw.projection;
    prepared.vertex.textureMatrix = draw.textureMatrices;
    prepared.raster = draw.raster;
    for (std::size_t unit = 0; unit < kTextureUnits; ++unit) {
        const auto& state = draw.textures[unit];
        if (state.environment > EnvironmentMode::Combine ||
            (state.rgbScale != 1 && state.rgbScale != 2 && state.rgbScale != 4)) {
            return GpuStatus::InvalidValue;
        }
        prepared.fragment.unit[unit] = {{state.enabled2D ? 1.0f : 0.0f, static_cast<float>(state.environment),
            static_cast<float>(state.rgbScale), 0}};
        prepared.fragment.components[unit] = {{1, 1, 0, 0}};
        if (state.enabledRectangle) {
            return GpuStatus::Unsupported;
        }
        if (!state.enabled2D) {
            continue;
        }
        if (!AffineTexture(draw.textureMatrices[unit])) {
            return GpuStatus::Unsupported;
        }
        const auto& object = draw.resources[unit].texture2D;
        if (!object || object->levels.count(0) == 0) {
            return GpuStatus::Unsupported;
        }
        const auto& image = object->levels.at(0);
        const auto& sampler = object->sampler;
        if (!image->defined || image->width <= 0 || image->height <= 0 ||
            image->width > kTextureDimension || image->height > kTextureDimension ||
            (image->internal != InternalFormat::RGBA8 && image->internal != InternalFormat::RGB8) ||
            sampler.minification > Filter::Linear || sampler.wrapS == Wrap::Clamp || sampler.wrapT == Wrap::Clamp ||
            sampler.requestedAnisotropy != 1) {
            return GpuStatus::Unsupported;
        }
        if (image->pixels.size() != static_cast<std::uint64_t>(image->width) *
            static_cast<std::uint64_t>(image->height) * sizeof(std::array<std::uint8_t, 4>)) {
            return GpuStatus::InvalidValue;
        }
        prepared.images[unit] = image;
        prepared.samplers[unit] = sampler;
        prepared.fragment.components[unit][1] = image->internal == InternalFormat::RGB8 ? 0.0f : 1.0f;
    }
    const auto& raster = draw.raster;
    prepared.fragment.fogColor = raster.fogColor;
    prepared.fragment.fog = {{raster.enabled[static_cast<std::size_t>(Capability::Fog)] ? 1.0f : 0.0f,
        static_cast<float>(raster.fogMode), raster.fogDensity, raster.fogStart}};
    prepared.fragment.fogAlpha = {{raster.fogEnd,
        raster.enabled[static_cast<std::size_t>(Capability::AlphaTest)] ? 1.0f : 0.0f,
        static_cast<float>(raster.alphaFunction), raster.alphaReference}};
    output = std::move(prepared);
    return GpuStatus::Ok;
}

}
}
