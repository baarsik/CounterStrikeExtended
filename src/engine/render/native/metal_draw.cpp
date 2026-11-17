#include "metal_draw.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace cse {
namespace render {
namespace {
constexpr std::size_t kRgbaComponents = 4;
constexpr std::size_t kMaximumLoweredVertices = 128 * 1024 * 1024 / sizeof(Vertex);

bool Fail(std::string& error, const char* message) {
    error = message;
    return false;
}

template<typename Range> bool Finite(const Range& values) {
    for (auto value : values) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    return true;
}

bool HasColor(InternalFormat format) {
    return format != InternalFormat::Alpha && format != InternalFormat::Alpha8;
}

bool HasAlpha(InternalFormat format) {
    return format != InternalFormat::RGB && format != InternalFormat::RGB8 && format != InternalFormat::RGB5 &&
        format != InternalFormat::Luminance && format != InternalFormat::Luminance8 &&
        format != InternalFormat::ColorIndex8;
}

std::uint8_t Quantize(std::uint8_t value, unsigned bits) {
    constexpr unsigned kByteMaximum = 255;
    const unsigned maximum = (1u << bits) - 1;
    const unsigned reduced = (value * maximum + kByteMaximum / 2) / kByteMaximum;
    return static_cast<std::uint8_t>((reduced * kByteMaximum + maximum / 2) / maximum);
}

bool ConvertImage(const TextureImage& source, const std::shared_ptr<const SharedPalette>& palette,
    TextureImage& output, std::string& error) {
    if (!source.defined || source.width <= 0 || source.height <= 0 || source.width > kTextureDimension ||
        source.height > kTextureDimension || source.internal > InternalFormat::Intensity) {
        return Fail(error, "Invalid owned texture image dimensions/format");
    }
    const auto pixels = static_cast<std::size_t>(source.width) * source.height;
    const bool indexed = source.internal == InternalFormat::ColorIndex8;
    if (pixels > kTextureUploadBytes / kRgbaComponents ||
        source.pixels.size() != pixels * (indexed ? 1 : kRgbaComponents)) {
        return Fail(error, "Invalid or oversized owned texture image extent");
    }
    if (indexed && !palette) {
        return Fail(error, "Indexed texture has no owned palette");
    }
    output.width = source.width;
    output.height = source.height;
    output.internal = InternalFormat::RGBA8;
    output.defined = true;
    output.pixels.resize(pixels * kRgbaComponents);
    for (std::size_t index = 0; index < pixels; ++index) {
        auto* destination = output.pixels.data() + index * kRgbaComponents;
        if (indexed) {
            const auto offset = static_cast<std::size_t>(source.pixels[index]) * 3;
            std::copy_n(palette->data() + offset, 3, destination);
            destination[3] = 255;
            continue;
        }
        std::copy_n(source.pixels.data() + index * kRgbaComponents, kRgbaComponents, destination);
        switch (source.internal) {
        case InternalFormat::RGB5:
        case InternalFormat::RGB5A1:
            for (std::size_t component = 0; component < 3; ++component) {
                destination[component] = Quantize(destination[component], 5);
            }
            destination[3] = source.internal == InternalFormat::RGB5 ? 255 :
                static_cast<std::uint8_t>(destination[3] >= 128 ? 255 : 0);
            break;
        case InternalFormat::RGBA4:
            for (std::size_t component = 0; component < kRgbaComponents; ++component) {
                destination[component] = Quantize(destination[component], 4);
            }
            break;
        case InternalFormat::Alpha8:
        case InternalFormat::Alpha:
            destination[0] = 255;
            destination[1] = 255;
            destination[2] = 255;
            break;
        case InternalFormat::Luminance8:
        case InternalFormat::Luminance:
        case InternalFormat::Intensity8:
        case InternalFormat::Intensity:
            destination[1] = destination[0];
            destination[2] = destination[0];
            destination[3] = source.internal == InternalFormat::Intensity ||
                source.internal == InternalFormat::Intensity8 ? destination[0] : 255;
            break;
        default:
            if (!HasAlpha(source.internal)) {
                destination[3] = 255;
            }
            break;
        }
    }
    return true;
}

void Incomplete(MetalTextureData& output) {
    output.incomplete = true;
    output.components = {{1, 1, 0, 0}};
    TextureImage black;
    black.defined = true;
    black.width = 1;
    black.height = 1;
    black.pixels = {0, 0, 0, 255};
    output.levels = {std::move(black)};
}
}

float EffectiveMetalAnisotropy(float requested) {
    // Metal samplers support at most 16 taps; GL permits larger requested values.
    constexpr float kMaximumMetalAnisotropy = 16.0f;
    return std::min(requested, kMaximumMetalAnisotropy);
}

bool PrepareMetalTexture(const TextureObject& object, const std::shared_ptr<const SharedPalette>& palette,
    MetalTextureData& output, std::string& error) {
    const auto& sampler = object.sampler;
    if (object.target > TextureTarget::Rectangle || sampler.minification > Filter::LinearMipmapLinear ||
        sampler.magnification > Filter::Linear || sampler.wrapS > Wrap::ClampToEdge ||
        sampler.wrapT > Wrap::ClampToEdge || sampler.maximumLevel < 0 ||
        !std::isfinite(sampler.requestedAnisotropy) || sampler.requestedAnisotropy < 1) {
        return Fail(error, "Invalid Metal texture/sampler state");
    }
    MetalTextureData prepared;
    const auto base = object.levels.find(0);
    if (base == object.levels.end() || !base->second || !base->second->defined) {
        Incomplete(prepared);
        output = std::move(prepared);
        error.clear();
        return true;
    }
    TextureImage converted;
    if (!ConvertImage(*base->second, palette, converted, error)) {
        return false;
    }
    prepared.components = {{HasColor(base->second->internal) ? 1.0f : 0.0f,
        HasAlpha(base->second->internal) ? 1.0f : 0.0f, 0, 0}};
    prepared.levels.push_back(std::move(converted));
    const bool mipmapped = object.target != TextureTarget::Rectangle && sampler.minification > Filter::Linear;
    int width = base->second->width;
    int height = base->second->height;
    std::size_t totalBytes = prepared.levels[0].pixels.size();
    for (int level = 1; mipmapped && (width > 1 || height > 1) && level <= sampler.maximumLevel; ++level) {
        width = std::max(1, width / 2);
        height = std::max(1, height / 2);
        const auto found = object.levels.find(level);
        if (found == object.levels.end() || !found->second || !found->second->defined ||
            found->second->width != width || found->second->height != height ||
            found->second->internal != base->second->internal) {
            Incomplete(prepared);
            break;
        }
        TextureImage mip;
        if (!ConvertImage(*found->second, palette, mip, error)) {
            return false;
        }
        totalBytes += mip.pixels.size();
        if (totalBytes > kTextureUploadBytes) {
            return Fail(error, "Texture mip chain exceeds Metal upload budget");
        }
        prepared.levels.push_back(std::move(mip));
    }
    output = std::move(prepared);
    error.clear();
    return true;
}

bool PrepareMetalDraw(const DrawPacket& draw, MetalDrawData& output, std::string& error) {
    const auto& raster = draw.raster;
    if (draw.primitive > Primitive::LineLoop || draw.shade > Shade::Smooth || raster.shade > Shade::Smooth ||
        draw.vertices.size() > kMaximumVertices || !Finite(draw.modelView.values) || !Finite(draw.projection.values) ||
        raster.depthFunction > Compare::Always || raster.alphaFunction > Compare::Always ||
        raster.blendSource > BlendFactor::InverseDestinationColor ||
        raster.blendDestination > BlendFactor::InverseDestinationColor || raster.cull > Cull::FrontAndBack ||
        raster.fogMode > FogMode::Linear || raster.viewport[2] < 0 || raster.viewport[3] < 0 ||
        raster.scissor[2] < 0 || raster.scissor[3] < 0 || !Finite(raster.depthRange) ||
        !Finite(raster.fogColor) || !Finite(raster.polygonOffset) || !std::isfinite(raster.alphaReference) ||
        !std::isfinite(raster.fogDensity) || !std::isfinite(raster.fogStart) || !std::isfinite(raster.fogEnd) ||
        raster.depthRange[0] < 0 || raster.depthRange[0] > 1 || raster.depthRange[1] < 0 || raster.depthRange[1] > 1) {
        return Fail(error, "Invalid owned Metal draw state");
    }
    if (raster.polygonMode[0] != raster.polygonMode[1] || raster.polygonMode[0] == PolygonMode::Point ||
        raster.polygonMode[0] > PolygonMode::Fill) {
        return Fail(error, "Metal requires matching fill/line polygon modes; polygon points are unsupported");
    }
    const auto count = draw.vertices.size();
    const bool lines = draw.primitive == Primitive::Lines || draw.primitive == Primitive::LineLoop;
    const bool wire = !lines && raster.polygonMode[0] == PolygonMode::Line;
    std::size_t loweredCount = 0;
    switch (draw.primitive) {
    case Primitive::Triangles: loweredCount = count / 3 * (wire ? 6 : 3); break;
    case Primitive::TriangleFan:
    case Primitive::TriangleStrip: loweredCount = count > 2 ? (count - 2) * (wire ? 6 : 3) : 0; break;
    case Primitive::Quads: loweredCount = count / 4 * (wire ? 8 : 6); break;
    case Primitive::QuadStrip: loweredCount = count >= 4 ? (count / 2 - 1) * (wire ? 8 : 6) : 0; break;
    case Primitive::Polygon: loweredCount = count > 2 ? (wire ? count * 2 : (count - 2) * 3) : 0; break;
    case Primitive::Lines: loweredCount = count / 2 * 2; break;
    case Primitive::LineLoop: loweredCount = count > 1 ? count * 2 : 0; break;
    }
    if (loweredCount > kMaximumLoweredVertices) {
        return Fail(error, "Lowered Metal geometry exceeds streaming budget");
    }
    MetalDrawData prepared;
    prepared.vertex = {draw.modelView, draw.projection, draw.textureMatrices};
    prepared.raster = raster;
    prepared.vertices.reserve(loweredCount);
    prepared.topology = draw.primitive == Primitive::Lines || draw.primitive == Primitive::LineLoop ?
        Primitive::Lines : Primitive::Triangles;
    for (const auto& vertex : draw.vertices) {
        if (!Finite(vertex.position) || !Finite(vertex.color)) {
            return Fail(error, "Nonfinite owned Metal vertex");
        }
        for (const auto& uv : vertex.texcoords) {
            if (!Finite(uv)) {
                return Fail(error, "Nonfinite owned Metal texture coordinate");
            }
        }
    }
    const bool wireframe = raster.polygonMode[0] == PolygonMode::Line &&
        prepared.topology == Primitive::Triangles;
    const auto culled = [&draw, &raster](std::size_t first, std::size_t second, std::size_t third) {
        if (!raster.enabled[static_cast<std::size_t>(Capability::CullFace)]) {
            return false;
        }
        const auto matrix = Multiply(draw.projection, draw.modelView);
        const auto a = Transform(matrix, draw.vertices[first].position);
        const auto b = Transform(matrix, draw.vertices[second].position);
        const auto c = Transform(matrix, draw.vertices[third].position);
        const auto area = a[0] * (b[1] * c[3] - c[1] * b[3]) -
            a[1] * (b[0] * c[3] - c[0] * b[3]) + a[3] * (b[0] * c[1] - c[0] * b[1]);
        const bool front = (area > 0) == raster.frontCounterClockwise;
        return raster.cull == Cull::FrontAndBack || (front && raster.cull == Cull::Front) ||
            (!front && raster.cull == Cull::Back);
    };
    const auto emit = [&draw, &prepared, wireframe, &culled](
        std::initializer_list<std::size_t> indices, std::size_t provoking) {
        std::array<std::size_t, 8> lowered{};
        std::copy(indices.begin(), indices.end(), lowered.begin());
        auto emitted = indices.size();
        if (wireframe && emitted >= 3) {
            if (culled(lowered[0], lowered[1], lowered[2])) {
                return;
            }
            if (emitted == 6) {
                lowered[3] = lowered[5];
                emitted = 4;
            }
            const auto boundary = lowered;
            for (std::size_t edge = 0; edge < emitted; ++edge) {
                lowered[edge * 2] = boundary[edge];
                lowered[edge * 2 + 1] = boundary[(edge + 1) % emitted];
            }
            emitted *= 2;
        }
        for (std::size_t entry = 0; entry < emitted; ++entry) {
            const auto index = lowered[entry];
            auto vertex = draw.vertices[index];
            if (draw.shade == Shade::Flat) {
                vertex.color = draw.vertices[provoking].color;
            }
            for (auto& component : vertex.color) {
                component = std::max(0.0f, std::min(1.0f, component));
            }
            prepared.vertices.push_back(vertex);
        }
    };
    switch (draw.primitive) {
    case Primitive::Triangles:
        // Filled smooth triangles already have the GPU's topology and per-vertex colors.
        // Copy them in one block instead of expanding every triangle through the generic emitter.
        if (!wireframe && draw.shade == Shade::Smooth) {
            prepared.vertices.assign(draw.vertices.begin(), draw.vertices.begin() + loweredCount);
            for (auto& vertex : prepared.vertices) {
                for (auto& component : vertex.color) {
                    component = std::max(0.0f, std::min(1.0f, component));
                }
            }
            break;
        }
        for (std::size_t index = 0; index + 2 < count; index += 3) {
            emit({index, index + 1, index + 2}, index + 2);
        }
        break;
    case Primitive::Polygon:
        if (wireframe) {
            if (count >= 3 && !culled(0, 1, 2)) {
                for (std::size_t index = 0; index < count; ++index) {
                    emit({index, (index + 1) % count}, 0);
                }
            }
            break;
        }
        // Fall through to the convex fan when filled.
    case Primitive::TriangleFan:
        // GL_POLYGON is convex; its first vertex provokes the entire polygon.
        for (std::size_t index = 1; index + 1 < count; ++index) {
            emit({0, index, index + 1}, draw.primitive == Primitive::Polygon ? 0 : index + 1);
        }
        break;
    case Primitive::TriangleStrip:
        for (std::size_t index = 0; index + 2 < count; ++index) {
            emit({index + index % 2, index + 1 - index % 2, index + 2}, index + 2);
        }
        break;
    case Primitive::Quads:
        // Convex GL quads are split along 0--2; preserve the fourth provoking color on both triangles.
        for (std::size_t index = 0; index + 3 < count; index += 4) {
            emit({index, index + 1, index + 2, index, index + 2, index + 3}, index + 3);
        }
        break;
    case Primitive::QuadStrip:
        for (std::size_t index = 0; index + 3 < count; index += 2) {
            emit({index, index + 1, index + 3, index, index + 3, index + 2}, index + 3);
        }
        break;
    case Primitive::Lines:
        for (std::size_t index = 0; index + 1 < count; index += 2) {
            emit({index, index + 1}, index + 1);
        }
        break;
    case Primitive::LineLoop:
        if (count > 1) {
            for (std::size_t index = 0; index < count; ++index) {
                emit({index, (index + 1) % count}, (index + 1) % count);
            }
        }
        break;
    }
    if (wireframe) {
        prepared.topology = Primitive::Lines;
        prepared.raster.enabled[static_cast<std::size_t>(Capability::CullFace)] = false;
    }
    if (prepared.vertices.size() > kMaximumLoweredVertices) {
        return Fail(error, "Lowered Metal draw exceeds vertex budget");
    }
    for (std::size_t unit = 0; unit < kTextureUnits; ++unit) {
        const auto& state = draw.textures[unit];
        if (!Finite(draw.textureMatrices[unit].values) || state.environment > EnvironmentMode::Combine ||
            (state.rgbScale != 1 && state.rgbScale != 2 && state.rgbScale != 4)) {
            return Fail(error, "Invalid owned Metal texture unit state");
        }
        auto& legacy = prepared.fragment.legacy;
        const bool enabled = state.enabled2D || state.enabledRectangle;
        legacy.unit[unit] = {{enabled ? 1.0f : 0.0f, static_cast<float>(state.environment),
            static_cast<float>(state.rgbScale), 0}};
        legacy.components[unit] = {{1, 1, 0, 0}};
        if (!enabled) {
            continue;
        }
        const auto& object = state.enabledRectangle ? draw.resources[unit].rectangle : draw.resources[unit].texture2D;
        if (object) {
            const auto found = object->levels.find(0);
            const auto& sampler = object->sampler;
            if (found != object->levels.end() && found->second && found->second->defined) {
                const auto& image = *found->second;
                prepared.fragment.texture[unit] = {{static_cast<float>(image.width),
                    static_cast<float>(image.height), state.enabledRectangle ? 1.0f : 0.0f,
                    EffectiveMetalAnisotropy(sampler.requestedAnisotropy)}};
                legacy.components[unit] = {{HasColor(image.internal) ? 1.0f : 0.0f,
                    HasAlpha(image.internal) ? 1.0f : 0.0f, 0, 0}};
            }
            prepared.fragment.sampling[unit] = {{static_cast<float>(sampler.wrapS), static_cast<float>(sampler.wrapT),
                static_cast<float>(sampler.minification), static_cast<float>(sampler.magnification)}};
        }
    }
    auto& legacy = prepared.fragment.legacy;
    legacy.fogColor = raster.fogColor;
    legacy.fog = {{raster.enabled[static_cast<std::size_t>(Capability::Fog)] ? 1.0f : 0.0f,
        static_cast<float>(raster.fogMode), raster.fogDensity, raster.fogStart}};
    legacy.fogAlpha = {{raster.fogEnd, raster.enabled[static_cast<std::size_t>(Capability::AlphaTest)] ? 1.0f : 0.0f,
        static_cast<float>(raster.alphaFunction), raster.alphaReference}};
    output = std::move(prepared);
    error.clear();
    return true;
}
}
}
