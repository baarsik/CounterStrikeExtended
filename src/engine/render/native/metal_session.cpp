#include "metal_session.h"
#include "metal_effect_access.h"
#include "radar_renderer_gpu.h"
#include "metal_draw.h"
#include "legacy_runtime_embedded.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <utility>

namespace cse {
namespace render {
namespace {
constexpr std::size_t kMaximumStreamingBytes = 128 * 1024 * 1024;
constexpr std::size_t kMaximumCachedTextureBytes = 256 * 1024 * 1024;
constexpr std::size_t kMaximumCachedTextures = 512;
constexpr std::size_t kMaximumCachedPipelines = 512;
constexpr std::size_t kFramesInFlight = 3;
constexpr std::size_t kTransferAlignment = 512;
constexpr std::size_t kReadbackRowAlignment = 256;
constexpr std::size_t kRgbaComponents = 4;

std::size_t Align(std::size_t value, std::size_t alignment) {
    return (value + alignment - 1) / alignment * alignment;
}

bool Fail(std::string& error, const char* message) {
    error = message;
    return false;
}

bool DeviceFailure(std::string& error, const char* stage) {
    error = std::string(stage) + ": " + SDL_GetError();
    return false;
}

SDL_GPUTexture* CreateTexture(SDL_GPUDevice* device, unsigned width, unsigned height,
    SDL_GPUTextureFormat format, SDL_GPUTextureUsageFlags usage, SDL_GPUSampleCount samples, unsigned levels = 1) {
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = format;
    info.usage = usage;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = levels;
    info.sample_count = samples;
    return SDL_CreateGPUTexture(device, &info);
}

SDL_GPUBlendFactor Blend(BlendFactor factor, bool alpha) {
    constexpr std::array<SDL_GPUBlendFactor, 10> kColor{{SDL_GPU_BLENDFACTOR_ZERO, SDL_GPU_BLENDFACTOR_ONE,
        SDL_GPU_BLENDFACTOR_SRC_COLOR, SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR, SDL_GPU_BLENDFACTOR_SRC_ALPHA,
        SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_GPU_BLENDFACTOR_DST_ALPHA, SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_ALPHA,
        SDL_GPU_BLENDFACTOR_DST_COLOR, SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_COLOR}};
    constexpr std::array<SDL_GPUBlendFactor, 10> kAlpha{{SDL_GPU_BLENDFACTOR_ZERO, SDL_GPU_BLENDFACTOR_ONE,
        SDL_GPU_BLENDFACTOR_SRC_ALPHA, SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_GPU_BLENDFACTOR_SRC_ALPHA,
        SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_GPU_BLENDFACTOR_DST_ALPHA, SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_ALPHA,
        SDL_GPU_BLENDFACTOR_DST_ALPHA, SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_ALPHA}};
    return (alpha ? kAlpha : kColor)[static_cast<std::size_t>(factor)];
}

std::uint32_t Bits(float value) {
    std::uint32_t result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

SDL_Rect Scissor(const RasterState& state, unsigned width, unsigned height) {
    if (!state.enabled[static_cast<std::size_t>(Capability::ScissorTest)]) {
        return {0, 0, static_cast<int>(width), static_cast<int>(height)};
    }
    const auto& rectangle = state.scissor;
    const auto left = std::max<std::int64_t>(0, std::min<std::int64_t>(width, rectangle[0]));
    const auto bottom = std::max<std::int64_t>(0, std::min<std::int64_t>(height, rectangle[1]));
    const auto right = std::max<std::int64_t>(left, std::min<std::int64_t>(width,
        static_cast<std::int64_t>(rectangle[0]) + rectangle[2]));
    const auto top = std::max<std::int64_t>(bottom, std::min<std::int64_t>(height,
        static_cast<std::int64_t>(rectangle[1]) + rectangle[3]));
    return {static_cast<int>(left), static_cast<int>(height - top), static_cast<int>(right - left),
        static_cast<int>(top - bottom)};
}

DrawPacket ClearDraw(const ClearEvent& clear, unsigned width, unsigned height) {
    DrawPacket draw;
    draw.primitive = Primitive::Quads;
    draw.raster.viewport = {{0, 0, static_cast<int>(width), static_cast<int>(height)}};
    draw.raster.scissor = clear.scissor;
    draw.raster.enabled[static_cast<std::size_t>(Capability::ScissorTest)] = clear.scissorEnabled;
    draw.raster.enabled[static_cast<std::size_t>(Capability::DepthTest)] = true;
    draw.raster.depthFunction = Compare::Always;
    draw.raster.depthWrite = (clear.mask & kClearDepth) && clear.depthWrite;
    for (std::size_t component = 0; component < kRgbaComponents; ++component) {
        draw.raster.colorWrite[component] = (clear.mask & kClearColor) && clear.colorWrite[component];
    }
    const auto depth = static_cast<float>(std::max(0.0, std::min(1.0, clear.depth)) * 2 - 1);
    draw.vertices.resize(4);
    draw.vertices[0].position = {{-1, -1, depth, 1}};
    draw.vertices[1].position = {{1, -1, depth, 1}};
    draw.vertices[2].position = {{1, 1, depth, 1}};
    draw.vertices[3].position = {{-1, 1, depth, 1}};
    for (auto& vertex : draw.vertices) {
        vertex.color = clear.color;
    }
    for (auto& matrix : draw.textureMatrices) {
        matrix = Matrix4::Identity();
    }
    return draw;
}
}

struct MetalSession::Impl {
    struct Stream {
        SDL_GPUBuffer* vertices = nullptr;
        SDL_GPUTransferBuffer* upload = nullptr;
        SDL_GPUFence* fence = nullptr;
        std::size_t capacity = 0;
    };
    struct Texture {
        std::weak_ptr<const TextureObject> object;
        std::weak_ptr<const SharedPalette> palette;
        MetalTextureData data;
        SDL_GPUTexture* texture = nullptr;
        std::size_t bytes = 0;
        std::uint64_t lastUse = 0;
        bool uploaded = false;
    };
    struct Draw {
        MetalDrawData data;
        std::array<Texture*, kTextureUnits> textures{};
        std::array<SDL_GPUSampler*, kTextureUnits> samplers{};
        SDL_GPUGraphicsPipeline* pipeline = nullptr;
        Uint32 vertexOffset = 0;
    };
    std::unique_ptr<RadarRendererGpu> effects;
    std::uint64_t effectsEpoch = 0;
    SDL_GPUDevice* device = nullptr;
    SDL_Window* window = nullptr;
    SDL_GPUShader* vertex = nullptr;
    SDL_GPUShader* fragment = nullptr;
    SDL_GPUTexture* color = nullptr;
    SDL_GPUTexture* multisample = nullptr;
    SDL_GPUTexture* depth = nullptr;
    SDL_GPUTexture* presented = nullptr;
    unsigned presentedWidth = 0;
    unsigned presentedHeight = 0;
    bool presentedValid = false;
    unsigned width = 0;
    unsigned height = 0;
    SDL_GPUSampleCount samples = SDL_GPU_SAMPLECOUNT_1;
    bool initializedTarget = false;
    bool vsync = true;
    std::uint64_t serial = 0;
    std::size_t streamIndex = 0;
    std::size_t textureBytes = 0;
    std::array<Stream, kFramesInFlight> streams{};
    using TextureKey = std::pair<const TextureObject*, const SharedPalette*>;
    std::map<TextureKey, Texture> textures;
    std::map<std::vector<std::uint32_t>, SDL_GPUSampler*> samplers;
    std::map<std::vector<std::uint32_t>, SDL_GPUGraphicsPipeline*> pipelines;
    std::shared_ptr<const TextureObject> white;
    std::shared_ptr<const TextureObject> black;

    bool Wait(Stream& stream, std::string& error) {
        if (stream.fence) {
            if (!SDL_QueryGPUFence(device, stream.fence) &&
                !SDL_WaitForGPUFences(device, true, &stream.fence, 1)) {
                return DeviceFailure(error, "Retire Metal streaming frame");
            }
            SDL_ReleaseGPUFence(device, stream.fence);
            stream.fence = nullptr;
        }
        return true;
    }

    SDL_GPUShader* Shader(bool isVertex) {
        SDL_GPUShaderCreateInfo info{};
        info.code = kLegacyRuntimeMetal;
        info.code_size = sizeof(kLegacyRuntimeMetal);
        info.entrypoint = isVertex ? "LegacyVertex" : "LegacyFragment";
        info.format = SDL_GPU_SHADERFORMAT_METALLIB;
        info.stage = isVertex ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT;
        info.num_samplers = isVertex ? 0 : static_cast<Uint32>(kTextureUnits);
        info.num_uniform_buffers = 1;
        return SDL_CreateGPUShader(device, &info);
    }

    SDL_GPUGraphicsPipeline* Pipeline(const MetalDrawData& draw) {
        const auto& state = draw.raster;
        const bool blend = state.enabled[static_cast<std::size_t>(Capability::Blend)];
        const bool depthTest = state.enabled[static_cast<std::size_t>(Capability::DepthTest)];
        const bool cull = state.enabled[static_cast<std::size_t>(Capability::CullFace)];
        const bool bias = state.enabled.back() && draw.topology == Primitive::Triangles &&
            state.polygonMode[0] == PolygonMode::Fill;
        std::vector<std::uint32_t> key{static_cast<unsigned>(samples), static_cast<unsigned>(draw.topology), blend,
            static_cast<unsigned>(state.blendSource), static_cast<unsigned>(state.blendDestination), depthTest,
            state.depthWrite, static_cast<unsigned>(state.depthFunction), cull, static_cast<unsigned>(state.cull),
            state.frontCounterClockwise, static_cast<unsigned>(state.polygonMode[0]), bias,
            Bits(state.polygonOffset[0]), Bits(state.polygonOffset[1])};
        for (auto write : state.colorWrite) {
            key.push_back(write);
        }
        const auto found = pipelines.find(key);
        if (found != pipelines.end()) {
            return found->second;
        }
        SDL_GPUColorTargetDescription target{};
        target.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        target.blend_state.enable_blend = blend;
        target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        target.blend_state.src_color_blendfactor = Blend(state.blendSource, false);
        target.blend_state.dst_color_blendfactor = Blend(state.blendDestination, false);
        target.blend_state.src_alpha_blendfactor = Blend(state.blendSource, true);
        target.blend_state.dst_alpha_blendfactor = Blend(state.blendDestination, true);
        target.blend_state.enable_color_write_mask = true;
        for (std::size_t component = 0; component < kRgbaComponents; ++component) {
            if (state.colorWrite[component]) {
                target.blend_state.color_write_mask |= static_cast<SDL_GPUColorComponentFlags>(1u << component);
            }
        }
        SDL_GPUVertexBufferDescription buffer{};
        buffer.pitch = sizeof(Vertex);
        buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        std::array<SDL_GPUVertexAttribute, 5> attributes{};
        for (std::size_t index = 0; index < attributes.size(); ++index) {
            attributes[index].location = static_cast<Uint32>(index);
            attributes[index].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
            attributes[index].offset = static_cast<Uint32>(index * sizeof(std::array<float, 4>));
        }
        SDL_GPUGraphicsPipelineCreateInfo info{};
        info.vertex_shader = vertex;
        info.fragment_shader = fragment;
        info.vertex_input_state.vertex_buffer_descriptions = &buffer;
        info.vertex_input_state.num_vertex_buffers = 1;
        info.vertex_input_state.vertex_attributes = attributes.data();
        info.vertex_input_state.num_vertex_attributes = static_cast<Uint32>(attributes.size());
        info.primitive_type = draw.topology == Primitive::Lines ? SDL_GPU_PRIMITIVETYPE_LINELIST :
            SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        info.rasterizer_state.fill_mode = state.polygonMode[0] == PolygonMode::Line ?
            SDL_GPU_FILLMODE_LINE : SDL_GPU_FILLMODE_FILL;
        info.rasterizer_state.cull_mode = !cull ? SDL_GPU_CULLMODE_NONE :
            state.cull == Cull::Front ? SDL_GPU_CULLMODE_FRONT : SDL_GPU_CULLMODE_BACK;
        info.rasterizer_state.front_face = state.frontCounterClockwise ? SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE :
            SDL_GPU_FRONTFACE_CLOCKWISE;
        info.rasterizer_state.enable_depth_clip = true;
        info.rasterizer_state.enable_depth_bias = bias;
        info.rasterizer_state.depth_bias_slope_factor = state.polygonOffset[0];
        info.rasterizer_state.depth_bias_constant_factor = state.polygonOffset[1];
        info.multisample_state.sample_count = samples;
        info.depth_stencil_state.enable_depth_test = depthTest;
        info.depth_stencil_state.enable_depth_write = depthTest && state.depthWrite;
        info.depth_stencil_state.compare_op = static_cast<SDL_GPUCompareOp>(
            static_cast<unsigned>(SDL_GPU_COMPAREOP_NEVER) + static_cast<unsigned>(state.depthFunction));
        info.target_info.num_color_targets = 1;
        info.target_info.color_target_descriptions = &target;
        info.target_info.has_depth_stencil_target = true;
        info.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        if (pipelines.size() >= kMaximumCachedPipelines) {
            SDL_SetError("Frame exceeds cached Metal pipeline budget");
            return nullptr;
        }
        auto* pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
        if (pipeline) {
            pipelines.emplace(std::move(key), pipeline);
        }
        return pipeline;
    }

    SDL_GPUSampler* Sampler(const SamplerState& state, unsigned levels, bool rectangle) {
        const float anisotropy = EffectiveMetalAnisotropy(state.requestedAnisotropy);
        const std::vector<std::uint32_t> key{static_cast<unsigned>(state.minification),
            static_cast<unsigned>(state.magnification), static_cast<unsigned>(state.wrapS),
            static_cast<unsigned>(state.wrapT), Bits(anisotropy), levels, rectangle};
        const auto found = samplers.find(key);
        if (found != samplers.end()) {
            return found->second;
        }
        SDL_GPUSamplerCreateInfo info{};
        info.min_filter = state.minification == Filter::Linear || state.minification == Filter::LinearMipmapNearest ||
            state.minification == Filter::LinearMipmapLinear ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
        info.mag_filter = state.magnification == Filter::Linear ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
        info.mipmap_mode = state.minification == Filter::NearestMipmapLinear ||
            state.minification == Filter::LinearMipmapLinear ? SDL_GPU_SAMPLERMIPMAPMODE_LINEAR :
            SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        info.address_mode_u = state.wrapS == Wrap::Repeat ? SDL_GPU_SAMPLERADDRESSMODE_REPEAT :
            SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        info.address_mode_v = state.wrapT == Wrap::Repeat ? SDL_GPU_SAMPLERADDRESSMODE_REPEAT :
            SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        // A max LOD of zero forces Metal's magnification filter even when derivatives minify.
        // The physical texture contains only admitted levels, so its last level still bounds sampling.
        constexpr float kMinimumSamplerLodRange = 1.0f;
        info.max_lod = std::max(kMinimumSamplerLodRange,
            state.minification > Filter::Linear && !rectangle ? static_cast<float>(levels - 1) : 0.0f);
        info.enable_anisotropy = anisotropy > 1;
        info.max_anisotropy = anisotropy;
        if (samplers.size() >= kMaximumCachedPipelines * 2) {
            SDL_SetError("Frame exceeds cached Metal sampler budget");
            return nullptr;
        }
        auto* sampler = SDL_CreateGPUSampler(device, &info);
        if (sampler) {
            samplers.emplace(key, sampler);
        }
        return sampler;
    }

    Texture* GetTexture(std::shared_ptr<const TextureObject> object,
        std::shared_ptr<const SharedPalette> palette, std::string& error) {
        const TextureKey key{object.get(), palette.get()};
        auto found = textures.find(key);
        if (found != textures.end()) {
            // Raw addresses are only an index: expired owners may have been allocated at these addresses again.
            if (found->second.object.lock() == object && found->second.palette.lock() == palette) {
                found->second.lastUse = serial;
                return &found->second;
            }
            textureBytes -= found->second.bytes;
            SDL_ReleaseGPUTexture(device, found->second.texture);
            textures.erase(found);
        }
        Texture entry;
        entry.object = object;
        entry.palette = palette;
        if (!PrepareMetalTexture(*object, palette, entry.data, error)) {
            return nullptr;
        }
        for (const auto& image : entry.data.levels) {
            entry.bytes += image.pixels.size();
        }
        // Evict immutable versions only between frames. SDL retains submitted GPU references after release.
        while ((textureBytes + entry.bytes > kMaximumCachedTextureBytes || textures.size() >= kMaximumCachedTextures) &&
            !textures.empty()) {
            auto oldest = textures.end();
            for (auto iterator = textures.begin(); iterator != textures.end(); ++iterator) {
                if (iterator->second.lastUse != serial &&
                    (oldest == textures.end() || iterator->second.lastUse < oldest->second.lastUse)) {
                    oldest = iterator;
                }
            }
            if (oldest == textures.end()) {
                Fail(error, "Frame sampled image set exceeds Metal cache budget");
                return nullptr;
            }
            textureBytes -= oldest->second.bytes;
            SDL_ReleaseGPUTexture(device, oldest->second.texture);
            textures.erase(oldest);
        }
        const auto& image = entry.data.levels.front();
        entry.texture = CreateTexture(device, image.width, image.height, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
            SDL_GPU_TEXTUREUSAGE_SAMPLER, SDL_GPU_SAMPLECOUNT_1, static_cast<unsigned>(entry.data.levels.size()));
        if (!entry.texture) {
            DeviceFailure(error, "Create immutable Metal sampled texture");
            return nullptr;
        }
        entry.lastUse = serial;
        textureBytes += entry.bytes;
        return &textures.emplace(key, std::move(entry)).first->second;
    }

    bool Prepare(const DrawPacket& packet, Draw& draw, std::string& error) {
        if (!PrepareMetalDraw(packet, draw.data, error)) {
            return false;
        }
        for (std::size_t unit = 0; unit < kTextureUnits; ++unit) {
            const auto& state = packet.textures[unit];
            const bool enabled = state.enabled2D || state.enabledRectangle;
            auto object = enabled ? (state.enabledRectangle ? packet.resources[unit].rectangle :
                packet.resources[unit].texture2D) : white;
            if (!object) {
                object = white;
                draw.data.fragment.legacy.unit[unit][0] = 0;
            }
            const auto& sampler = object->sampler;
            const auto base = object->levels.find(0);
            const bool indexed = base != object->levels.end() && base->second &&
                base->second->internal == InternalFormat::ColorIndex8;
            if (enabled && indexed && !packet.sharedTexturePalette) {
                error = "Indexed texture requires enabled shared texture palette; per-texture palettes are unsupported";
                return false;
            }
            auto* texture = GetTexture(object, enabled && indexed ? packet.palette : nullptr, error);
            if (!texture) {
                return false;
            }
            // Fixed-function GL disables incomplete units; sampling black would still alter REPLACE/COMBINE.
            if (texture->data.incomplete) {
                draw.data.fragment.legacy.unit[unit][0] = 0;
            }
            draw.textures[unit] = texture;
            draw.samplers[unit] = Sampler(sampler, static_cast<unsigned>(texture->data.levels.size()),
                object->target == TextureTarget::Rectangle);
            draw.data.fragment.legacy.components[unit] = texture->data.components;
            if (!draw.samplers[unit]) {
                return DeviceFailure(error, "Create Metal sampler");
            }
        }
        draw.pipeline = Pipeline(draw.data);
        return draw.pipeline || DeviceFailure(error, "Create cached Metal pipeline");
    }

    bool EnsureStream(Stream& stream, std::size_t size, std::string& error) {
        if (!Wait(stream, error)) {
            return false;
        }
        if (stream.capacity >= size) {
            return true;
        }
        if (stream.vertices) {
            SDL_ReleaseGPUBuffer(device, stream.vertices);
        }
        if (stream.upload) {
            SDL_ReleaseGPUTransferBuffer(device, stream.upload);
        }
        stream.vertices = nullptr;
        stream.upload = nullptr;
        stream.capacity = 0;
        // Geometric growth, capped before allocating; each of the three slots is reused after its fence.
        std::size_t capacity = 4096;
        while (capacity < size) {
            capacity *= 2;
        }
        capacity = std::min(capacity, kMaximumStreamingBytes);
        SDL_GPUBufferCreateInfo buffer{};
        buffer.size = static_cast<Uint32>(capacity);
        buffer.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        stream.vertices = SDL_CreateGPUBuffer(device, &buffer);
        SDL_GPUTransferBufferCreateInfo transfer{};
        transfer.size = static_cast<Uint32>(capacity);
        transfer.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        stream.upload = SDL_CreateGPUTransferBuffer(device, &transfer);
        if (!stream.vertices || !stream.upload) {
            return DeviceFailure(error, "Grow persistent Metal streaming buffers");
        }
        stream.capacity = capacity;
        return true;
    }
};

RadarRendererGpu& MetalSession::RadarEffects(std::uint64_t epoch) {
    auto& state = *impl_;
    if (!state.effects || state.effectsEpoch != epoch) {
        state.effects.reset(new RadarRendererGpu(*this, epoch));
        state.effectsEpoch = epoch;
    }
    return *state.effects;
}

MetalEffectAccess MetalSession::EffectAccess() const {
    const auto& state = *impl_;
    return {state.device, state.color, state.multisample, state.width, state.height, state.samples};
}

MetalSession::MetalSession() : impl_(new Impl) {
}

MetalSession::~MetalSession() {
    Shutdown();
}

bool MetalSession::Initialize(SDL_Window* window, bool debug, std::string& error) {
    Shutdown();
    auto& state = *impl_;
    state.device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_METALLIB, debug, "metal");
    if (!state.device) {
        return DeviceFailure(error, "Create persistent SDL Metal device");
    }
    if (std::strcmp(SDL_GetGPUDeviceDriver(state.device), "metal") != 0) {
        Shutdown();
        return Fail(error, "SDL selected a non-Metal device");
    }
    if (window) {
        if (!SDL_ClaimWindowForGPUDevice(state.device, window)) {
            Shutdown();
            return DeviceFailure(error, "Claim Metal presentation window");
        }
        state.window = window;
    }
    state.vertex = state.Shader(true);
    state.fragment = state.Shader(false);
    if (!state.vertex || !state.fragment) {
        Shutdown();
        return DeviceFailure(error, "Load embedded offline Metal shaders");
    }
    const auto fallback = [](std::array<std::uint8_t, 4> rgba) {
        auto image = std::make_shared<TextureImage>();
        image->width = 1;
        image->height = 1;
        image->defined = true;
        image->pixels.assign(rgba.begin(), rgba.end());
        auto object = std::make_shared<TextureObject>();
        object->levels[0] = image;
        object->sampler.minification = Filter::Nearest;
        object->sampler.magnification = Filter::Nearest;
        return object;
    };
    state.white = fallback({{255, 255, 255, 255}});
    state.black = fallback({{0, 0, 0, 255}});
    error.clear();
    return true;
}

bool MetalSession::Resize(std::uint32_t width, std::uint32_t height, std::uint32_t samples, std::string& error) {
    auto& state = *impl_;
    if (!state.device || width == 0 || height == 0 || width > kTextureDimension || height > kTextureDimension ||
        static_cast<std::uint64_t>(width) * height * kRgbaComponents > kMaximumStreamingBytes) {
        return Fail(error, "Invalid Metal target dimensions or uninitialized session");
    }
    SDL_GPUSampleCount count;
    switch (samples) {
    case 1:
        count = SDL_GPU_SAMPLECOUNT_1;
        break;
    case 2:
        count = SDL_GPU_SAMPLECOUNT_2;
        break;
    case 4:
        count = SDL_GPU_SAMPLECOUNT_4;
        break;
    case 8:
        count = SDL_GPU_SAMPLECOUNT_8;
        break;
    default:
        return Fail(error, "Unsupported Metal sample count");
    }
    if (!SDL_GPUTextureSupportsSampleCount(state.device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, count) ||
        !SDL_GPUTextureSupportsSampleCount(state.device, SDL_GPU_TEXTUREFORMAT_D32_FLOAT, count)) {
        return Fail(error, "Metal device does not support requested color/depth sample count");
    }
    if (state.width == width && state.height == height && state.samples == count) {
        error.clear();
        return true;
    }
    auto* color = CreateTexture(state.device, width, height, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER, SDL_GPU_SAMPLECOUNT_1);
    auto* depth = CreateTexture(state.device, width, height, SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET, count);
    auto* multisample = count == SDL_GPU_SAMPLECOUNT_1 ? nullptr :
        CreateTexture(state.device, width, height, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
            SDL_GPU_TEXTUREUSAGE_COLOR_TARGET, count);
    if (!color || !depth || (count != SDL_GPU_SAMPLECOUNT_1 && !multisample)) {
        if (color) {
            SDL_ReleaseGPUTexture(state.device, color);
        }
        if (depth) {
            SDL_ReleaseGPUTexture(state.device, depth);
        }
        if (multisample) {
            SDL_ReleaseGPUTexture(state.device, multisample);
        }
        return DeviceFailure(error, "Create Metal color/depth targets");
    }
    // SDL resources retain in-flight references, so resizing needs no global device stall.
    if (state.color) {
        SDL_ReleaseGPUTexture(state.device, state.color);
    }
    if (state.depth) {
        SDL_ReleaseGPUTexture(state.device, state.depth);
    }
    if (state.multisample) {
        SDL_ReleaseGPUTexture(state.device, state.multisample);
    }
    state.color = color;
    state.depth = depth;
    state.multisample = multisample;
    state.width = width;
    state.height = height;
    state.samples = count;
    state.initializedTarget = false;
    error.clear();
    return true;
}

bool MetalSession::Render(const FrameSnapshot& frame, std::string& error) {
    auto& state = *impl_;
    if (!state.device || !state.color || frame.info.width != state.width || frame.info.height != state.height ||
        frame.events.size() > kMaximumDrawPackets * 2 || frame.draws.size() > kMaximumDrawPackets) {
        return Fail(error, "Invalid Metal frame or missing target");
    }
    // Validate all event references before issuing GPU commands. Readback uses incremental Render/ReadPixels.
    for (const auto& event : frame.events) {
        if (event.kind == FrameEventKind::Readback) {
            return Fail(error, "ReadbackEvent has no delivery sink; flush Render then call ReadPixels");
        }
        if (event.kind == FrameEventKind::Draw && event.drawIndex >= frame.draws.size()) {
            return Fail(error, "Metal frame references an absent draw");
        }
        if (event.kind == FrameEventKind::Clear && ((event.clear.mask & kClearStencil) ||
            (event.clear.mask & ~(kClearColor | kClearDepth | kClearStencil)) ||
            !std::isfinite(event.clear.depth) || event.clear.scissor[2] < 0 || event.clear.scissor[3] < 0)) {
            return Fail(error, "Invalid clear or unsupported stencil clear on Metal D32 target");
        }
        if (event.kind != FrameEventKind::Draw && event.kind != FrameEventKind::Clear) {
            return Fail(error, "Invalid Metal frame event");
        }
    }
    ++state.serial;
    // Captured packets own source versions until submission; the persistent GPU cache only borrows them.
    // Expired entries can be released without waiting because SDL retains submitted GPU references.
    for (auto iterator = state.textures.begin(); iterator != state.textures.end();) {
        if (iterator->second.object.expired()) {
            state.textureBytes -= iterator->second.bytes;
            SDL_ReleaseGPUTexture(state.device, iterator->second.texture);
            iterator = state.textures.erase(iterator);
        } else {
            ++iterator;
        }
    }
    // Never evict a pipeline/sampler after this frame has retained its pointer and before submission.
    // SDL keeps prior submitted command references alive when the owning cache releases its reference.
    if (state.pipelines.size() >= kMaximumCachedPipelines / 2) {
        for (const auto& pair : state.pipelines) {
            SDL_ReleaseGPUGraphicsPipeline(state.device, pair.second);
        }
        state.pipelines.clear();
    }
    if (state.samplers.size() >= kMaximumCachedPipelines) {
        for (const auto& pair : state.samplers) {
            SDL_ReleaseGPUSampler(state.device, pair.second);
        }
        state.samplers.clear();
    }
    std::vector<Impl::Draw> draws;
    std::size_t vertexBytes = 0;
    for (const auto& event : frame.events) {
        Impl::Draw draw;
        if (event.kind == FrameEventKind::Clear) {
            if (!state.Prepare(ClearDraw(event.clear, state.width, state.height), draw, error)) {
                return false;
            }
        } else if (!state.Prepare(frame.draws[event.drawIndex], draw, error)) {
            return false;
        }
        if (draw.data.vertices.empty() || draw.data.raster.viewport[2] == 0 || draw.data.raster.viewport[3] == 0 ||
            (draw.data.topology == Primitive::Triangles &&
                draw.data.raster.enabled[static_cast<std::size_t>(Capability::CullFace)] &&
                draw.data.raster.cull == Cull::FrontAndBack)) {
            continue;
        }
        const auto bytes = draw.data.vertices.size() * sizeof(Vertex);
        if (bytes > kMaximumStreamingBytes - vertexBytes) {
            return Fail(error, "Frame geometry exceeds Metal streaming budget");
        }
        draw.vertexOffset = static_cast<Uint32>(vertexBytes);
        vertexBytes += bytes;
        draws.push_back(std::move(draw));
    }
    struct Upload {
        Impl::Texture* texture;
        unsigned level;
        Uint32 offset;
    };
    std::vector<Upload> uploads;
    auto uploadBytes = vertexBytes;
    for (auto& pair : state.textures) {
        auto& texture = pair.second;
        if (texture.uploaded || texture.lastUse != state.serial) {
            continue;
        }
        for (std::size_t level = 0; level < texture.data.levels.size(); ++level) {
            uploadBytes = Align(uploadBytes, kTransferAlignment);
            const auto bytes = texture.data.levels[level].pixels.size();
            if (uploadBytes > kMaximumStreamingBytes || bytes > kMaximumStreamingBytes - uploadBytes) {
                return Fail(error, "Frame textures exceed Metal streaming budget");
            }
            uploads.push_back({&texture, static_cast<unsigned>(level), static_cast<Uint32>(uploadBytes)});
            uploadBytes += bytes;
        }
    }
    auto& stream = state.streams[state.streamIndex];
    if (!state.EnsureStream(stream, std::max<std::size_t>(1, uploadBytes), error)) {
        return false;
    }
    auto* mapped = static_cast<std::uint8_t*>(SDL_MapGPUTransferBuffer(state.device, stream.upload, false));
    if (!mapped) {
        return DeviceFailure(error, "Map persistent Metal upload");
    }
    for (const auto& draw : draws) {
        std::memcpy(mapped + draw.vertexOffset, draw.data.vertices.data(), draw.data.vertices.size() * sizeof(Vertex));
    }
    for (const auto& upload : uploads) {
        const auto& image = upload.texture->data.levels[upload.level];
        std::memcpy(mapped + upload.offset, image.pixels.data(), image.pixels.size());
    }
    SDL_UnmapGPUTransferBuffer(state.device, stream.upload);
    auto* command = SDL_AcquireGPUCommandBuffer(state.device);
    if (!command) {
        return DeviceFailure(error, "Acquire Metal render command buffer");
    }
    auto* copy = SDL_BeginGPUCopyPass(command);
    if (!copy) {
        SDL_CancelGPUCommandBuffer(command);
        return DeviceFailure(error, "Begin Metal upload pass");
    }
    if (vertexBytes) {
        SDL_GPUTransferBufferLocation source{stream.upload, 0};
        SDL_GPUBufferRegion destination{stream.vertices, 0, static_cast<Uint32>(vertexBytes)};
        SDL_UploadToGPUBuffer(copy, &source, &destination, false);
    }
    for (const auto& upload : uploads) {
        const auto& image = upload.texture->data.levels[upload.level];
        SDL_GPUTextureTransferInfo source{};
        source.transfer_buffer = stream.upload;
        source.offset = upload.offset;
        // Pinned SDL Metal uses destination width; every uploaded mip is deliberately tightly packed.
        source.pixels_per_row = image.width;
        source.rows_per_layer = image.height;
        SDL_GPUTextureRegion destination{};
        destination.texture = upload.texture->texture;
        destination.mip_level = upload.level;
        destination.w = image.width;
        destination.h = image.height;
        destination.d = 1;
        SDL_UploadToGPUTexture(copy, &source, &destination, false);
    }
    SDL_EndGPUCopyPass(copy);
    SDL_GPUColorTargetInfo color{};
    color.texture = state.multisample ? state.multisample : state.color;
    color.resolve_texture = state.multisample ? state.color : nullptr;
    color.clear_color = {0, 0, 0, 0};
    color.load_op = state.initializedTarget ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_CLEAR;
    color.store_op = state.multisample ? SDL_GPU_STOREOP_RESOLVE_AND_STORE : SDL_GPU_STOREOP_STORE;
    SDL_GPUDepthStencilTargetInfo depth{};
    depth.texture = state.depth;
    depth.clear_depth = 1;
    depth.load_op = state.initializedTarget ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_CLEAR;
    depth.store_op = SDL_GPU_STOREOP_STORE;
    depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    auto* pass = SDL_BeginGPURenderPass(command, &color, 1, &depth);
    if (!pass) {
        SDL_CancelGPUCommandBuffer(command);
        return DeviceFailure(error, "Begin Metal scene pass");
    }
    for (const auto& draw : draws) {
        const auto& data = draw.data;
        const auto& viewport = data.raster.viewport;
        const auto scissor = Scissor(data.raster, state.width, state.height);
        if (scissor.w == 0 || scissor.h == 0) {
            continue;
        }
        auto constants = data.vertex;
        auto nearDepth = static_cast<float>(data.raster.depthRange[0]);
        auto farDepth = static_cast<float>(data.raster.depthRange[1]);
        if (nearDepth > farDepth) {
            // Reverse clip Z and swap viewport endpoints for GL's reversed depth range.
            for (std::size_t column = 0; column < 4; ++column) {
                constants.projection.values[column * 4 + 2] *= -1;
            }
            std::swap(nearDepth, farDepth);
        }
        // GL permits viewports extending beyond the drawable (including R_SetupGL's rounding margin).
        // SDL_GPU requires a contained viewport. Crop it and remap clip X/Y so screen coordinates,
        // interpolation and texture derivatives remain those of the original GL viewport.
        const auto left = std::max<std::int64_t>(0, viewport[0]);
        const auto bottom = std::max<std::int64_t>(0, viewport[1]);
        const auto right = std::min<std::int64_t>(state.width,
            static_cast<std::int64_t>(viewport[0]) + viewport[2]);
        const auto top = std::min<std::int64_t>(state.height,
            static_cast<std::int64_t>(viewport[1]) + viewport[3]);
        if (right <= left || top <= bottom) {
            continue;
        }
        const auto width = static_cast<float>(right - left);
        const auto height = static_cast<float>(top - bottom);
        constexpr float kNdcExtent = 2.0f;
        const float scaleX = viewport[2] / width;
        const float scaleY = viewport[3] / height;
        const float offsetX = (kNdcExtent * (viewport[0] - left) + viewport[2] - width) / width;
        const float offsetY = (kNdcExtent * (viewport[1] - bottom) + viewport[3] - height) / height;
        for (std::size_t column = 0; column < 4; ++column) {
            auto* row = constants.projection.values.data() + column * 4;
            row[0] = scaleX * row[0] + offsetX * row[3];
            row[1] = scaleY * row[1] + offsetY * row[3];
        }
        SDL_GPUViewport gpuViewport{static_cast<float>(left), static_cast<float>(state.height - top),
            width, height, nearDepth, farDepth};
        SDL_BindGPUGraphicsPipeline(pass, draw.pipeline);
        SDL_SetGPUViewport(pass, &gpuViewport);
        SDL_SetGPUScissor(pass, &scissor);
        SDL_GPUBufferBinding binding{stream.vertices, draw.vertexOffset};
        SDL_BindGPUVertexBuffers(pass, 0, &binding, 1);
        std::array<SDL_GPUTextureSamplerBinding, kTextureUnits> textures{};
        for (std::size_t unit = 0; unit < kTextureUnits; ++unit) {
            textures[unit] = {draw.textures[unit]->texture, draw.samplers[unit]};
        }
        SDL_BindGPUFragmentSamplers(pass, 0, textures.data(), static_cast<Uint32>(textures.size()));
        SDL_PushGPUVertexUniformData(command, 0, &constants, sizeof(constants));
        SDL_PushGPUFragmentUniformData(command, 0, &data.fragment, sizeof(data.fragment));
        SDL_DrawGPUPrimitives(pass, static_cast<Uint32>(data.vertices.size()), 1, 0, 0);
    }
    SDL_EndGPURenderPass(pass);
    stream.fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command);
    if (!stream.fence) {
        return DeviceFailure(error, "Submit Metal scene");
    }
    for (const auto& upload : uploads) {
        upload.texture->uploaded = true;
        // Converted CPU bytes are retained for cache accounting, while immutable capture owns source lifetime.
    }
    state.initializedTarget = true;
    state.streamIndex = (state.streamIndex + 1) % kFramesInFlight;
    error.clear();
    return true;
}

bool MetalSession::Present(bool vsync, bool preserveAspect, std::string& error) {
    auto& state = *impl_;
    if (!state.device || !state.window || !state.color || !state.initializedTarget) {
        return Fail(error, "Metal presentation needs a window and rendered scene");
    }
    if (state.vsync != vsync) {
        const auto mode = vsync ? SDL_GPU_PRESENTMODE_VSYNC : SDL_GPU_PRESENTMODE_IMMEDIATE;
        if (!SDL_SetGPUSwapchainParameters(state.device, state.window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode)) {
            return DeviceFailure(error, "Set Metal presentation mode");
        }
        state.vsync = vsync;
    }
    int drawableWidth = 0;
    int drawableHeight = 0;
    if (!SDL_GetWindowSizeInPixels(state.window, &drawableWidth, &drawableHeight) ||
        drawableWidth <= 0 || drawableHeight <= 0) {
        return DeviceFailure(error, "Query Metal drawable size");
    }
    const auto ensureFinalColor = [&state, &error](Uint32 width, Uint32 height) {
        if (width > kTextureDimension || height > kTextureDimension ||
            static_cast<std::uint64_t>(width) * height * kRgbaComponents > kMaximumStreamingBytes) {
            return Fail(error, "Metal drawable exceeds retained final-color budget");
        }
        if (!state.presented || state.presentedWidth != width || state.presentedHeight != height) {
            auto* presented = CreateTexture(state.device, width, height, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER, SDL_GPU_SAMPLECOUNT_1);
            if (!presented) {
                return DeviceFailure(error, "Create retained Metal final-color target");
            }
            if (state.presented) {
                SDL_ReleaseGPUTexture(state.device, state.presented);
            }
            state.presented = presented;
            state.presentedWidth = width;
            state.presentedHeight = height;
            state.presentedValid = false;
        }
        return true;
    };
    // Validate and allocate the expected drawable before acquiring a swapchain command.
    if (!ensureFinalColor(static_cast<Uint32>(drawableWidth), static_cast<Uint32>(drawableHeight))) {
        return false;
    }
    auto* command = SDL_AcquireGPUCommandBuffer(state.device);
    if (!command) {
        return DeviceFailure(error, "Acquire Metal presentation command");
    }
    SDL_GPUTexture* swapchain = nullptr;
    const auto retireFailure = [command, &swapchain, &error]() {
        const auto diagnostic = error;
        // SDL forbids cancellation after a non-null acquisition, even if no drawing was encoded.
        if (swapchain) {
            SDL_SubmitGPUCommandBuffer(command);
        } else {
            SDL_CancelGPUCommandBuffer(command);
        }
        error = diagnostic;
        return false;
    };
    Uint32 width = 0;
    Uint32 height = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(command, state.window, &swapchain, &width, &height)) {
        DeviceFailure(error, "Acquire Metal swapchain image");
        return retireFailure();
    }
    if (!width || !height) {
        width = static_cast<Uint32>(drawableWidth);
        height = static_cast<Uint32>(drawableHeight);
    }
    // A resize may race preflight. A failed reallocation must still retire an acquired swapchain by submission.
    if (!ensureFinalColor(width, height)) {
        return retireFailure();
    }
    unsigned destinationWidth = width;
    unsigned destinationHeight = height;
    if (preserveAspect) {
        const double scale = std::min(static_cast<double>(width) / state.width,
            static_cast<double>(height) / state.height);
        destinationWidth = std::max(1u, static_cast<unsigned>(state.width * scale));
        destinationHeight = std::max(1u, static_cast<unsigned>(state.height * scale));
    }
    SDL_GPUBlitInfo info{};
    info.source.texture = state.color;
    info.source.w = state.width;
    info.source.h = state.height;
    info.destination.texture = state.presented;
    info.destination.x = (width - destinationWidth) / 2;
    info.destination.y = (height - destinationHeight) / 2;
    info.destination.w = destinationWidth;
    info.destination.h = destinationHeight;
    info.load_op = SDL_GPU_LOADOP_CLEAR;
    info.clear_color = {0, 0, 0, 1};
    info.filter = SDL_GPU_FILTER_LINEAR;
    SDL_BlitGPUTexture(command, &info);
    if (swapchain) {
        info.source.texture = state.presented;
        info.source.w = width;
        info.source.h = height;
        info.destination.texture = swapchain;
        info.destination.x = 0;
        info.destination.y = 0;
        info.destination.w = width;
        info.destination.h = height;
        SDL_BlitGPUTexture(command, &info);
    }
    if (!SDL_SubmitGPUCommandBuffer(command)) {
        return DeviceFailure(error, "Submit Metal presentation");
    }
    state.presentedValid = true;
    error.clear();
    return true;
}

namespace {
bool ReadTarget(SDL_GPUDevice* device, SDL_GPUTexture* target, unsigned targetWidth, unsigned targetHeight,
    int x, int y, int width, int height, unsigned components, unsigned alignment,
    void* output, std::string& error) {
    if (!device || !target || x < 0 || y < 0 || width < 0 || height < 0 ||
        static_cast<std::uint64_t>(x) + width > targetWidth || static_cast<std::uint64_t>(y) + height > targetHeight ||
        (components != 3 && components != 4) ||
        (alignment != 1 && alignment != 2 && alignment != 4 && alignment != 8) ||
        ((width && height) && !output)) {
        return Fail(error, "Invalid Metal ReadPixels rectangle, components, packing or destination");
    }
    if (!width || !height) {
        error.clear();
        return true;
    }
    const auto rowBytes = Align(static_cast<std::size_t>(width) * kRgbaComponents, kReadbackRowAlignment);
    const auto bytes = rowBytes * height;
    if (bytes > kMaximumStreamingBytes) {
        return Fail(error, "Metal ReadPixels exceeds transfer budget");
    }
    SDL_GPUTransferBufferCreateInfo transfer{};
    transfer.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    transfer.size = static_cast<Uint32>(bytes);
    auto* download = SDL_CreateGPUTransferBuffer(device, &transfer);
    if (!download) {
        return DeviceFailure(error, "Create Metal readback transfer");
    }
    auto* command = SDL_AcquireGPUCommandBuffer(device);
    if (!command) {
        SDL_ReleaseGPUTransferBuffer(device, download);
        return DeviceFailure(error, "Acquire Metal readback commands");
    }
    auto* copy = SDL_BeginGPUCopyPass(command);
    if (!copy) {
        SDL_CancelGPUCommandBuffer(command);
        SDL_ReleaseGPUTransferBuffer(device, download);
        return DeviceFailure(error, "Begin Metal readback copy");
    }
    SDL_GPUTextureRegion source{};
    source.texture = target;
    source.x = x;
    source.y = targetHeight - y - height;
    source.w = width;
    source.h = height;
    source.d = 1;
    SDL_GPUTextureTransferInfo destination{};
    destination.transfer_buffer = download;
    destination.pixels_per_row = static_cast<Uint32>(rowBytes / kRgbaComponents);
    destination.rows_per_layer = height;
    SDL_DownloadFromGPUTexture(copy, &source, &destination);
    SDL_EndGPUCopyPass(copy);
    auto* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command);
    if (!fence || !SDL_WaitForGPUFences(device, true, &fence, 1)) {
        if (fence) {
            SDL_ReleaseGPUFence(device, fence);
        }
        SDL_ReleaseGPUTransferBuffer(device, download);
        return DeviceFailure(error, "Wait Metal synchronous readback");
    }
    const auto* mapped = static_cast<const std::uint8_t*>(SDL_MapGPUTransferBuffer(device, download, false));
    if (!mapped) {
        SDL_ReleaseGPUFence(device, fence);
        SDL_ReleaseGPUTransferBuffer(device, download);
        return DeviceFailure(error, "Map Metal readback");
    }
    const auto pitch = Align(static_cast<std::size_t>(width) * components, alignment);
    auto* pixels = static_cast<std::uint8_t*>(output);
    for (int row = 0; row < height; ++row) {
        const auto* sourceRow = mapped + static_cast<std::size_t>(height - 1 - row) * rowBytes;
        for (int column = 0; column < width; ++column) {
            std::memcpy(pixels + static_cast<std::size_t>(row) * pitch + column * components,
                sourceRow + column * kRgbaComponents, components);
        }
    }
    SDL_UnmapGPUTransferBuffer(device, download);
    SDL_ReleaseGPUFence(device, fence);
    SDL_ReleaseGPUTransferBuffer(device, download);
    error.clear();
    return true;
}

}

bool MetalSession::ReadPixels(int x, int y, int width, int height, unsigned components, unsigned alignment,
    void* output, std::string& error) {
    auto& state = *impl_;
    if (!state.initializedTarget) {
        return Fail(error, "Metal scene has not been rendered");
    }
    return ReadTarget(state.device, state.color, state.width, state.height,
        x, y, width, height, components, alignment, output, error);
}

bool MetalSession::PresentedSize(int& width, int& height) const {
    const auto& state = *impl_;
    width = state.presentedValid ? static_cast<int>(state.presentedWidth) : 0;
    height = state.presentedValid ? static_cast<int>(state.presentedHeight) : 0;
    return state.presentedValid;
}

bool MetalSession::ReadPresentedPixels(int x, int y, int width, int height, unsigned components, unsigned alignment,
    void* output, std::string& error) {
    auto& state = *impl_;
    if (!state.presentedValid) {
        return Fail(error, "Metal final color has not been presented");
    }
    return ReadTarget(state.device, state.presented, state.presentedWidth, state.presentedHeight,
        x, y, width, height, components, alignment, output, error);
}

bool MetalSession::Finish(std::string& error) {
    auto& state = *impl_;
    if (!state.device) {
        return Fail(error, "Metal session is not initialized");
    }
    for (auto& stream : state.streams) {
        if (!state.Wait(stream, error)) {
            return false;
        }
    }
    if (!SDL_WaitForGPUIdle(state.device)) {
        return DeviceFailure(error, "Finish Metal queue");
    }
    error.clear();
    return true;
}

void MetalSession::Shutdown() {
    auto& state = *impl_;
    if (!state.device) {
        return;
    }
    SDL_WaitForGPUIdle(state.device);
    state.effects.reset();
    for (auto& stream : state.streams) {
        if (stream.fence) {
            SDL_ReleaseGPUFence(state.device, stream.fence);
        }
        if (stream.vertices) {
            SDL_ReleaseGPUBuffer(state.device, stream.vertices);
        }
        if (stream.upload) {
            SDL_ReleaseGPUTransferBuffer(state.device, stream.upload);
        }
    }
    for (const auto& pair : state.pipelines) {
        SDL_ReleaseGPUGraphicsPipeline(state.device, pair.second);
    }
    for (const auto& pair : state.samplers) {
        SDL_ReleaseGPUSampler(state.device, pair.second);
    }
    for (const auto& pair : state.textures) {
        SDL_ReleaseGPUTexture(state.device, pair.second.texture);
    }
    if (state.color) {
        SDL_ReleaseGPUTexture(state.device, state.color);
    }
    if (state.presented) {
        SDL_ReleaseGPUTexture(state.device, state.presented);
    }
    if (state.depth) {
        SDL_ReleaseGPUTexture(state.device, state.depth);
    }
    if (state.multisample) {
        SDL_ReleaseGPUTexture(state.device, state.multisample);
    }
    if (state.vertex) {
        SDL_ReleaseGPUShader(state.device, state.vertex);
    }
    if (state.fragment) {
        SDL_ReleaseGPUShader(state.device, state.fragment);
    }
    if (state.window) {
        SDL_ReleaseWindowFromGPUDevice(state.device, state.window);
    }
    SDL_DestroyGPUDevice(state.device);
    impl_.reset(new Impl);
}
}
}
