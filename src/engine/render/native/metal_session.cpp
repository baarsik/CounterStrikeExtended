#include "metal_session.h"
#include "../../native_frame_timings.h"
#include "surface_mesh.h"
#include "metal_effect_access.h"
#include "radar_renderer_gpu.h"
#include "metal_draw.h"
#include "legacy_runtime_embedded.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <map>
#include <unordered_map>
#include <utility>
#include <chrono>
#include <cstdio>

namespace cse {
namespace render {
namespace {
constexpr std::size_t kMaximumStreamingBytes = 128 * 1024 * 1024;
// The measured scene/HUD working set reaches 41 MiB; three in-flight frames need up to 123 MiB.
constexpr std::size_t kMaximumCachedStreamingBytes = 128 * 1024 * 1024;
constexpr std::size_t kMaximumCachedTextureBytes = 256 * 1024 * 1024;
constexpr std::size_t kMaximumCachedTextures = 512;
constexpr std::size_t kMaximumCachedPipelines = 512;
// Three vertex layouts combine with four border/alpha fragment variants.
constexpr std::size_t kMaximumCachedVariantPipelines = 12 * kMaximumCachedPipelines;
constexpr std::size_t kMaximumCachedMeshBytes = 128 * 1024 * 1024;
constexpr std::size_t kMeshVertexPageBytes = 4 * 1024 * 1024;
constexpr std::size_t kMeshIndexPageBytes = 1024 * 1024;
constexpr std::size_t kFramesInFlight = 3;
constexpr std::size_t kTransferAlignment = 512;
constexpr std::size_t kReadbackRowAlignment = 256;
constexpr std::size_t kRgbaComponents = 4;

struct WorldMeshVertex {
    std::array<float, 3> position;
    std::array<float, 2> uv0;
    std::array<float, 2> uv1;
};
static_assert(sizeof(WorldMeshVertex) == 28, "World mesh vertex layout");

// The single-texture stream keeps position, color, and unit zero's entire homogeneous UV.
constexpr std::size_t kSingleTextureVertexBytes = 3 * sizeof(std::array<float, 4>);
static_assert(offsetof(Vertex, texcoords) == 2 * sizeof(std::array<float, 4>), "Legacy vertex attribute layout");

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

template<typename T>
bool SameBits(const T& left, const T& right) {
    return std::memcmp(&left, &right, sizeof(T)) == 0;
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
    bool reportedNonfinite = false;
    bool diagnosticsEnabled = false;
    RenderFrameCounters counters;
    FILE* diagnosticsFile = nullptr;
    using Clock = std::chrono::steady_clock;
    struct Timer {
        double* elapsed;
        Clock::time_point start;
        explicit Timer(double* target) : elapsed(target), start(target ? Clock::now() : Clock::time_point{}) {}
        ~Timer() {
            if (elapsed) {
                *elapsed += std::chrono::duration<double, std::milli>(Clock::now() - start).count();
            }
        }
    };
    struct Stream {
        SDL_GPUBuffer* vertices = nullptr;
        SDL_GPUBuffer* indices = nullptr;
        SDL_GPUTransferBuffer* upload = nullptr;
        std::size_t vertexCapacity = 0;
        std::size_t indexCapacity = 0;
        std::size_t uploadCapacity = 0;
    };
    struct FrameStream {
        std::vector<Stream> chunks;
        std::size_t cursor = 0;
        SDL_GPUFence* fence = nullptr;
    };
    SDL_GPUCommandBuffer* command = nullptr;
    std::size_t pendingUploadBytes = 0;

    struct Texture {
        std::weak_ptr<const TextureObject> object;
        std::weak_ptr<const SharedPalette> palette;
        MetalTextureData data;
        SDL_GPUTexture* texture = nullptr;
        std::size_t bytes = 0;
        std::uint64_t lastUse = 0;
        bool uploaded = false;
    };
    struct MeshPage {
        SDL_GPUDevice* device = nullptr;
        SDL_GPUBuffer* vertices = nullptr;
        SDL_GPUBuffer* indices = nullptr;
        std::size_t vertexCapacity = 0;
        std::size_t indexCapacity = 0;
        std::size_t vertexStride = sizeof(Vertex);
        std::size_t vertexUsed = 0;
        std::size_t indexUsed = 0;
        ~MeshPage() {
            if (vertices) {
                SDL_ReleaseGPUBuffer(device, vertices);
            }
            if (indices) {
                SDL_ReleaseGPUBuffer(device, indices);
            }
        }
    };
    struct Mesh {
        std::weak_ptr<const SurfaceMesh> object;
        std::shared_ptr<MeshPage> page;
        Uint32 vertexOffset = 0;
        Uint32 indexOffset = 0;
        std::uint64_t lastUse = 0;
        bool uploaded = false;
    };
    enum class VertexLayout { Full, World, SingleTexture };
    struct Draw {
        MetalDrawData data;
        VertexLayout layout = VertexLayout::Full;
        Mesh* mesh = nullptr;
        std::array<Texture*, kTextureUnits> textures{};
        std::array<SDL_GPUSampler*, kTextureUnits> samplers{};
        SDL_GPUGraphicsPipeline* pipeline = nullptr;
        Uint32 vertexOffset = 0;
        Uint32 indexOffset = 0;
        std::uint32_t streamedBaseVertex = 0;
        std::size_t batchDraws = 0;
        std::size_t batchIndices = 0;
        bool meshIndicesInStream = false;
        bool softwareBorder = false;
    };
    using PipelineKey = std::array<std::uint32_t, 21>;
    struct ResolvedMaterial {
        const TextureObject* object = nullptr;
        const SharedPalette* palette = nullptr;
        bool sharedPalette = false;
        Texture* texture = nullptr;
        SDL_GPUSampler* sampler = nullptr;
    };
    struct Preparation {
        std::vector<Texture*> textures;
        std::vector<Mesh*> meshes;
        std::array<ResolvedMaterial, kTextureUnits> materials{};
        ResolvedMaterial fallback;
        PipelineKey pipelineKey{};
        SDL_GPUGraphicsPipeline* pipeline = nullptr;
    };

    static bool SameDrawState(const Draw& first, const Draw& second) {
        const auto& left = first.data;
        const auto& right = second.data;
        const bool compatibleGeometry = first.mesh && second.mesh ? first.mesh->page == second.mesh->page :
            !first.mesh && !second.mesh && first.layout == second.layout &&
                (left.IndexCount() == 0) == (right.IndexCount() == 0);
        return compatibleGeometry && first.pipeline == second.pipeline && first.textures == second.textures &&
            first.samplers == second.samplers && left.raster.viewport == right.raster.viewport &&
            SameBits(left.raster.depthRange, right.raster.depthRange) && left.raster.scissor == right.raster.scissor &&
            left.raster.enabled[static_cast<std::size_t>(Capability::ScissorTest)] ==
                right.raster.enabled[static_cast<std::size_t>(Capability::ScissorTest)] &&
            std::memcmp(&left.vertex, &right.vertex, sizeof(left.vertex)) == 0 &&
            std::memcmp(&left.fragment, &right.fragment, sizeof(left.fragment)) == 0;
    }

    std::vector<Draw> preparedDraws;
    std::unique_ptr<RadarRendererGpu> effects;
    std::uint64_t effectsEpoch = 0;
    SDL_GPUDevice* device = nullptr;
    SDL_Window* window = nullptr;
    SDL_GPUShader* vertex = nullptr;
    SDL_GPUShader* worldVertex = nullptr;
    SDL_GPUShader* singleTextureVertex = nullptr;
    std::array<SDL_GPUShader*, 4> fragments{};
    SDL_GPUTexture* color = nullptr;
    SDL_GPUTexture* spareResolve = nullptr;
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
    bool sceneDirty = false;
    bool vsync = true;
    std::uint64_t serial = 0;
    std::size_t streamIndex = 0;
    std::size_t textureBytes = 0;
    std::array<FrameStream, kFramesInFlight> streams{};
    using TextureKey = std::pair<const TextureObject*, const SharedPalette*>;
    struct TextureKeyHash {
        std::size_t operator()(const TextureKey& key) const noexcept {
            const auto object = std::hash<const TextureObject*>{}(key.first);
            const auto palette = std::hash<const SharedPalette*>{}(key.second);
            return object ^ (palette << 1);
        }
    };
    std::unordered_map<TextureKey, Texture, TextureKeyHash> textures;
    std::map<std::array<std::uint32_t, 7>, SDL_GPUSampler*> samplers;
    std::map<PipelineKey, SDL_GPUGraphicsPipeline*> pipelines;
    // Find world surfaces by immutable identity. Rehashing preserves the mesh
    // pointers held by prepared draws and the current upload list.
    std::unordered_map<const SurfaceMesh*, Mesh> meshes;
    std::uint64_t surfaceMeshEpoch = 0;
    std::vector<std::weak_ptr<MeshPage>> meshPages;
    std::array<std::shared_ptr<MeshPage>, 2> activeMeshPages;
    std::shared_ptr<const TextureObject> white;
    std::shared_ptr<const TextureObject> black;

    bool PresentedUsesResolve() const {
        return presented && (presented == color || presented == spareResolve);
    }

    bool CanPresentResolve(unsigned drawableWidth, unsigned drawableHeight) const {
        return multisample && drawableWidth == width && drawableHeight == height;
    }

    SDL_GPUTexture* CreateResolvedColor(unsigned width, unsigned height) {
        return CreateTexture(device, width, height, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
            SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER, SDL_GPU_SAMPLECOUNT_1);
    }

    SDL_GPUTexture* WritableResolve(std::string& error) {
        if (color != presented) {
            return color;
        }
        // A screenshot may still read the previous presentation.
        if (!spareResolve) {
            spareResolve = CreateResolvedColor(width, height);
            if (!spareResolve) {
                DeviceFailure(error, "Create Metal resolve target for next scene");
                return nullptr;
            }
        }
        return spareResolve;
    }

    void CommitResolve(SDL_GPUTexture* target) {
        // Change the readable scene only after successful resolve encoding.
        if (target != color) {
            std::swap(color, spareResolve);
        }
        sceneDirty = false;
        if (diagnosticsEnabled) {
            ++counters.sceneResolves;
        }
    }

    bool Wait(FrameStream& stream, std::string& error) {
        Timer timer(diagnosticsEnabled ? &counters.waitMs : nullptr);
        if (stream.fence) {
            if (!SDL_QueryGPUFence(device, stream.fence)) {
                if (diagnosticsEnabled) {
                    ++counters.fenceWaits;
                }
                if (!SDL_WaitForGPUFences(device, true, &stream.fence, 1)) {
                    return DeviceFailure(error, "Retire Metal streaming frame");
                }
            }
            SDL_ReleaseGPUFence(device, stream.fence);
            stream.fence = nullptr;
        }
        return true;
    }

    SDL_GPUCommandBuffer* Commands(std::string& error) {
        if (!command) {
            if (!Wait(streams[streamIndex], error)) {
                return nullptr;
            }
            streams[streamIndex].cursor = 0;
            command = SDL_AcquireGPUCommandBuffer(device);
            if (!command) {
                DeviceFailure(error, "Acquire Metal frame commands");
            }
        }
        return command;
    }

    SDL_GPUCommandBuffer* UploadCommands(std::size_t bytes, std::string& error) {
        // An exceptional oversized frame can retire a prefix without dropping geometry or growing
        // unbounded storage. Ordinary frames use one submission and one fence for all effects.
        if (bytes > kMaximumStreamingBytes) {
            Fail(error, "Metal upload exceeds pending-command budget");
            return nullptr;
        }
        if (bytes > kMaximumStreamingBytes - pendingUploadBytes && !Submit(error)) {
            return nullptr;
        }
        if (!Commands(error)) {
            return nullptr;
        }
        pendingUploadBytes += bytes;
        return command;
    }

    Stream* NextStream(std::string& error, std::size_t bytes) {
        if (!UploadCommands(bytes, error)) {
            return nullptr;
        }
        auto& frame = streams[streamIndex];
        if (frame.cursor == frame.chunks.size()) {
            frame.chunks.emplace_back();
        }
        return &frame.chunks[frame.cursor++];
    }

    bool Submit(std::string& error) {
        if (!command) {
            return true;
        }
        auto* pending = command;
        command = nullptr;
        auto& frame = streams[streamIndex];
        frame.fence = SDL_SubmitGPUCommandBufferAndAcquireFence(pending);
        if (!frame.fence) {
            return DeviceFailure(error, "Submit Metal frame");
        }
        if (diagnosticsEnabled) {
            ++counters.commandBuffers;
        }
        // Bound retained stream capacity independently of the number of encode boundaries.
        // SDL owns the references already encoded in this submission, including released buffers.
        std::size_t cachedBytes = 0;
        for (const auto& slot : streams) {
            for (const auto& chunk : slot.chunks) {
                cachedBytes += chunk.vertexCapacity + chunk.indexCapacity + chunk.uploadCapacity;
            }
        }
        if (diagnosticsEnabled) {
            counters.streamCacheBytes = cachedBytes;
            counters.streamWorkingBytes = 0;
            for (std::size_t index = 0; index < frame.cursor; ++index) {
                const auto& chunk = frame.chunks[index];
                counters.streamWorkingBytes += chunk.vertexCapacity + chunk.indexCapacity + chunk.uploadCapacity;
            }
        }
        const auto release = [this, &cachedBytes](Stream& chunk) {
            const auto bytes = chunk.vertexCapacity + chunk.indexCapacity + chunk.uploadCapacity;
            if (!bytes) {
                return;
            }
            cachedBytes -= bytes;
            if (chunk.vertices) {
                SDL_ReleaseGPUBuffer(device, chunk.vertices);
            }
            if (chunk.indices) {
                SDL_ReleaseGPUBuffer(device, chunk.indices);
            }
            if (chunk.upload) {
                SDL_ReleaseGPUTransferBuffer(device, chunk.upload);
            }
            chunk = {};
            if (diagnosticsEnabled) {
                ++counters.streamEvictions;
            }
        };
        // A one-off map build can leave thousands of cold chunks behind the normal scene/HUD
        // prefix. Retire unused tails before evicting buffers that an ordinary frame just used.
        for (auto& slot : streams) {
            for (std::size_t index = slot.chunks.size(); index > slot.cursor &&
                cachedBytes > kMaximumCachedStreamingBytes; --index) {
                release(slot.chunks[index - 1]);
            }
            while (slot.chunks.size() > slot.cursor && !slot.chunks.back().uploadCapacity &&
                !slot.chunks.back().vertexCapacity && !slot.chunks.back().indexCapacity) {
                slot.chunks.pop_back();
            }
        }
        // Under actual working-set pressure, evict the least recently submitted slot first.
        for (std::size_t age = 1; age <= streams.size() && cachedBytes > kMaximumCachedStreamingBytes;
            ++age) {
            auto& slot = streams[(streamIndex + age) % streams.size()];
            for (auto iterator = slot.chunks.rbegin(); iterator != slot.chunks.rend() &&
                cachedBytes > kMaximumCachedStreamingBytes; ++iterator) {
                release(*iterator);
            }
        }
        pendingUploadBytes = 0;
        streamIndex = (streamIndex + 1) % streams.size();
        return true;
    }

    SDL_GPUShader* Shader(const char* entrypoint, SDL_GPUShaderStage stage) {
        SDL_GPUShaderCreateInfo info{};
        info.code = kLegacyRuntimeMetal;
        info.code_size = sizeof(kLegacyRuntimeMetal);
        info.entrypoint = entrypoint;
        info.format = SDL_GPU_SHADERFORMAT_METALLIB;
        info.stage = stage;
        info.num_samplers = stage == SDL_GPU_SHADERSTAGE_FRAGMENT ? static_cast<Uint32>(kTextureUnits) : 0;
        info.num_uniform_buffers = 1;
        return SDL_CreateGPUShader(device, &info);
    }

    SDL_GPUGraphicsPipeline* Pipeline(const MetalDrawData& draw, bool softwareBorder, VertexLayout layout,
        Preparation& pending) {
        const bool worldLayout = layout == VertexLayout::World;
        const bool singleTexture = layout == VertexLayout::SingleTexture;
        const auto& state = draw.raster;
        const bool blend = state.enabled[static_cast<std::size_t>(Capability::Blend)];
        const bool depthTest = state.enabled[static_cast<std::size_t>(Capability::DepthTest)];
        const bool cull = state.enabled[static_cast<std::size_t>(Capability::CullFace)];
        const bool alphaTest = state.enabled[static_cast<std::size_t>(Capability::AlphaTest)] &&
            state.alphaFunction != Compare::Always;
        // The low bit selects discard support; the next bit selects software GL_CLAMP.
        const auto fragmentIndex = static_cast<std::uint32_t>(alphaTest) |
            (static_cast<std::uint32_t>(softwareBorder) << 1);
        const bool bias = state.enabled.back() && draw.topology == Primitive::Triangles &&
            state.polygonMode[0] == PolygonMode::Fill;
        const PipelineKey key{{static_cast<unsigned>(samples), static_cast<unsigned>(draw.topology), blend,
            static_cast<unsigned>(state.blendSource), static_cast<unsigned>(state.blendDestination), depthTest,
            state.depthWrite, static_cast<unsigned>(state.depthFunction), cull, static_cast<unsigned>(state.cull),
            state.frontCounterClockwise, static_cast<unsigned>(state.polygonMode[0]), bias,
            Bits(state.polygonOffset[0]), Bits(state.polygonOffset[1]), state.colorWrite[0], state.colorWrite[1],
            state.colorWrite[2], state.colorWrite[3], fragmentIndex, static_cast<std::uint32_t>(layout)}};
        if (pending.pipeline && pending.pipelineKey == key) {
            return pending.pipeline;
        }
        if (diagnosticsEnabled) {
            ++counters.pipelineLookups;
        }
        const auto found = pipelines.find(key);
        if (found != pipelines.end()) {
            pending.pipelineKey = key;
            pending.pipeline = found->second;
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
        buffer.pitch = worldLayout ? sizeof(WorldMeshVertex) :
            singleTexture ? kSingleTextureVertexBytes : sizeof(Vertex);
        buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        constexpr std::array<SDL_GPUVertexAttribute, 3> worldAttributes{{
            {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(WorldMeshVertex, position)},
            {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(WorldMeshVertex, uv0)},
            {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(WorldMeshVertex, uv1)},
        }};
        std::array<SDL_GPUVertexAttribute, 5> attributes{};
        for (std::size_t index = 0; index < attributes.size(); ++index) {
            attributes[index].location = static_cast<Uint32>(index);
            attributes[index].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
            attributes[index].offset = static_cast<Uint32>(index * sizeof(std::array<float, 4>));
        }
        SDL_GPUGraphicsPipelineCreateInfo info{};
        info.vertex_shader = worldLayout ? worldVertex : singleTexture ? singleTextureVertex : vertex;
        info.fragment_shader = fragments[fragmentIndex];
        info.vertex_input_state.vertex_buffer_descriptions = &buffer;
        info.vertex_input_state.num_vertex_buffers = 1;
        info.vertex_input_state.vertex_attributes = worldLayout ? worldAttributes.data() : attributes.data();
        info.vertex_input_state.num_vertex_attributes = static_cast<Uint32>(
            worldLayout ? worldAttributes.size() : singleTexture ? 3 : attributes.size());
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
        if (pipelines.size() >= kMaximumCachedVariantPipelines) {
            SDL_SetError("Frame exceeds cached Metal pipeline budget");
            return nullptr;
        }
        auto* pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
        if (pipeline) {
            pipelines.emplace(key, pipeline);
            pending.pipelineKey = key;
            pending.pipeline = pipeline;
        }
        return pipeline;
    }

    SDL_GPUSampler* Sampler(const SamplerState& state, unsigned levels, bool rectangle) {
        const float anisotropy = EffectiveMetalAnisotropy(state.requestedAnisotropy);
        const std::array<std::uint32_t, 7> key{{static_cast<unsigned>(state.minification),
            static_cast<unsigned>(state.magnification), static_cast<unsigned>(state.wrapS),
            static_cast<unsigned>(state.wrapT), Bits(anisotropy), levels, rectangle}};
        if (diagnosticsEnabled) {
            ++counters.samplerLookups;
        }
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

    Texture* GetTexture(const std::shared_ptr<const TextureObject>& object,
        const std::shared_ptr<const SharedPalette>& palette, Preparation& pending, std::string& error) {
        const TextureKey key{object.get(), palette.get()};
        if (diagnosticsEnabled) {
            ++counters.textureLookups;
        }
        auto found = textures.find(key);
        if (found != textures.end()) {
            // Matching live identities need only an expiration check. The
            // current frame already owns both sources throughout preparation.
            if (!found->second.object.expired() && (!palette || !found->second.palette.expired())) {
                if (!found->second.uploaded && found->second.lastUse != serial) {
                    pending.textures.push_back(&found->second);
                }
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
        auto* texture = &textures.emplace(key, std::move(entry)).first->second;
        pending.textures.push_back(texture);
        return texture;
    }

    Mesh* GetMesh(const std::shared_ptr<const SurfaceMesh>& object, Preparation& pending, std::string& error) {
        auto found = meshes.find(object.get());
        if (found != meshes.end()) {
            if (!found->second.object.expired()) {
                if (!found->second.uploaded && found->second.lastUse != serial) {
                    pending.meshes.push_back(&found->second);
                }
                found->second.lastUse = serial;
                return &found->second;
            }
            meshes.erase(found);
        }
        const auto worldLayout = object->HasWorldVertexLayout();
        const auto vertexStride = worldLayout ? sizeof(WorldMeshVertex) : sizeof(Vertex);
        const auto vertexBytes = object->Vertices().size() * vertexStride;
        const auto indexBytes = object->Indices().size() * sizeof(std::uint32_t);
        auto& activeMeshPage = activeMeshPages[worldLayout];
        if (!activeMeshPage || vertexBytes > activeMeshPage->vertexCapacity - activeMeshPage->vertexUsed ||
            indexBytes > activeMeshPage->indexCapacity - activeMeshPage->indexUsed) {
            std::size_t retainedBytes = 0;
            for (auto iterator = meshPages.begin(); iterator != meshPages.end();) {
                if (const auto page = iterator->lock()) {
                    retainedBytes += page->vertexCapacity + page->indexCapacity;
                    ++iterator;
                } else {
                    iterator = meshPages.erase(iterator);
                }
            }
            const auto vertexCapacity = std::max(vertexBytes, kMeshVertexPageBytes);
            const auto indexCapacity = std::max(indexBytes, kMeshIndexPageBytes);
            if (vertexCapacity + indexCapacity > kMaximumCachedMeshBytes - retainedBytes) {
                // Cache pressure preserves rendering by streaming this polygon through the same shader.
                error.clear();
                return nullptr;
            }
            auto page = std::make_shared<MeshPage>();
            page->device = device;
            page->vertexCapacity = vertexCapacity;
            page->vertexStride = vertexStride;
            page->indexCapacity = indexCapacity;
            SDL_GPUBufferCreateInfo info{};
            info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
            info.size = static_cast<Uint32>(vertexCapacity);
            page->vertices = SDL_CreateGPUBuffer(device, &info);
            info.usage = SDL_GPU_BUFFERUSAGE_INDEX;
            info.size = static_cast<Uint32>(indexCapacity);
            page->indices = SDL_CreateGPUBuffer(device, &info);
            if (!page->vertices || !page->indices) {
                DeviceFailure(error, "Create persistent Metal mesh page");
                return nullptr;
            }
            meshPages.push_back(page);
            activeMeshPage = std::move(page);
        }
        Mesh mesh;
        mesh.object = object;
        mesh.page = activeMeshPage;
        mesh.vertexOffset = static_cast<Uint32>(activeMeshPage->vertexUsed);
        mesh.indexOffset = static_cast<Uint32>(activeMeshPage->indexUsed);
        mesh.lastUse = serial;
        activeMeshPage->vertexUsed += vertexBytes;
        activeMeshPage->indexUsed += indexBytes;
        auto* inserted = &meshes.emplace(object.get(), std::move(mesh)).first->second;
        pending.meshes.push_back(inserted);
        return inserted;
    }

    bool Prepare(const DrawPacket& packet, Draw& draw, Preparation& pending, std::string& error,
        MetalGeometryOwnership ownership) {
        if (!PrepareMetalDraw(packet, draw.data, error, ownership)) {
            return false;
        }
        if (draw.data.discardedNonfinite && !reportedNonfinite) {
            SDL_LogWarn(SDL_LOG_CATEGORY_RENDER,
                "Metal discarded nonfinite geometry; unaffected primitives are preserved (reported once)");
            reportedNonfinite = true;
        }
        if (!draw.data.mesh && draw.data.Vertices().empty()) {
            return true;
        }
        if (draw.data.mesh) {
            draw.mesh = GetMesh(draw.data.mesh, pending, error);
            if (!draw.mesh) {
                if (!error.empty()) {
                    return false;
                }
                draw.data.vertices = draw.data.mesh->Vertices();
                draw.data.indices = draw.data.mesh->Indices();
                draw.data.mesh.reset();
            }
        }
        // The frame owns captured versions, and the cache protects all resources touched
        // by this segment from eviction. Expiration and shader cache eviction run before preparation.
        for (std::size_t unit = 0; unit < kTextureUnits; ++unit) {
            const auto& state = packet.textures[unit];
            const auto& resource = state.enabledRectangle ? packet.resources[unit].rectangle :
                packet.resources[unit].texture2D;
            const bool bound = (state.enabled2D || state.enabledRectangle) && resource;
            const auto& object = bound ? resource : white;
            const auto* palette = bound ? packet.palette.get() : nullptr;
            const bool sharedPalette = bound && packet.sharedTexturePalette;
            auto& material = bound ? pending.materials[unit] : pending.fallback;
            if (material.object != object.get() || material.palette != palette || material.sharedPalette != sharedPalette) {
                const auto base = object->levels.find(0);
                const bool indexed = base != object->levels.end() && base->second &&
                    base->second->internal == InternalFormat::ColorIndex8;
                if (bound && indexed && !sharedPalette) {
                    error = "Indexed texture requires enabled shared texture palette; per-texture palettes are unsupported";
                    return false;
                }
                const std::shared_ptr<const SharedPalette> noPalette;
                auto* texture = GetTexture(object, bound && indexed ? packet.palette : noPalette, pending, error);
                if (!texture) {
                    return false;
                }
                auto* sampler = Sampler(object->sampler, static_cast<unsigned>(texture->data.levels.size()),
                    object->target == TextureTarget::Rectangle);
                if (!sampler) {
                    return DeviceFailure(error, "Create Metal sampler");
                }
                material = {object.get(), palette, sharedPalette, texture, sampler};
            }
            // Fixed-function GL disables incomplete units; sampling black would still alter REPLACE/COMBINE.
            if (!bound || material.texture->data.incomplete) {
                draw.data.fragment.legacy.unit[unit][0] = 0;
            }
            draw.textures[unit] = material.texture;
            draw.samplers[unit] = material.sampler;
            draw.data.fragment.legacy.components[unit] = material.texture->data.components;
            if (draw.data.fragment.legacy.unit[unit][0] != 0) {
                const auto& sampling = draw.data.fragment.sampling[unit];
                constexpr auto clamp = static_cast<float>(Wrap::Clamp);
                draw.softwareBorder |= sampling[0] == clamp || sampling[1] == clamp;
            }
        }
        if (draw.mesh && draw.data.mesh->HasWorldVertexLayout()) {
            draw.layout = VertexLayout::World;
        } else if (!draw.mesh && draw.data.IndexCount() != 0 &&
            draw.data.fragment.legacy.unit[1][0] == 0 && draw.data.fragment.legacy.unit[2][0] == 0) {
            // Higher texture coordinates cannot influence a single-texture draw.
            draw.layout = VertexLayout::SingleTexture;
        }
        draw.pipeline = Pipeline(draw.data, draw.softwareBorder, draw.layout, pending);
        return draw.pipeline || DeviceFailure(error, "Create cached Metal pipeline");
    }

    static std::size_t StreamCapacity(std::size_t size) {
        constexpr std::size_t kInitialStreamingBytes = 4096;
        std::size_t capacity = kInitialStreamingBytes;
        while (capacity < size) {
            capacity *= 2;
        }
        return std::min(capacity, kMaximumStreamingBytes);
    }

    bool EnsureGeometryBuffer(SDL_GPUBuffer*& buffer, std::size_t& capacity, std::size_t bytes,
        SDL_GPUBufferUsageFlags usage, std::string& error) {
        if (capacity >= bytes) {
            return true;
        }
        if (buffer) {
            SDL_ReleaseGPUBuffer(device, buffer);
        }
        buffer = nullptr;
        capacity = 0;
        SDL_GPUBufferCreateInfo info{};
        info.size = static_cast<Uint32>(StreamCapacity(bytes));
        info.usage = usage;
        buffer = SDL_CreateGPUBuffer(device, &info);
        if (!buffer) {
            return DeviceFailure(error, "Grow persistent Metal geometry buffer");
        }
        capacity = info.size;
        if (diagnosticsEnabled) {
            ++counters.streamAllocations;
        }
        return true;
    }

    bool EnsureStream(Stream& stream, std::size_t vertexBytes, std::size_t indexBytes, std::size_t uploadBytes,
        std::string& error) {
        if (!EnsureGeometryBuffer(stream.vertices, stream.vertexCapacity, vertexBytes, SDL_GPU_BUFFERUSAGE_VERTEX, error) ||
            !EnsureGeometryBuffer(stream.indices, stream.indexCapacity, indexBytes, SDL_GPU_BUFFERUSAGE_INDEX, error)) {
            return false;
        }
        if (stream.uploadCapacity >= uploadBytes && stream.upload) {
            return true;
        }
        if (stream.upload) {
            SDL_ReleaseGPUTransferBuffer(device, stream.upload);
        }
        stream.upload = nullptr;
        stream.uploadCapacity = 0;
        SDL_GPUTransferBufferCreateInfo transfer{};
        transfer.size = static_cast<Uint32>(StreamCapacity(uploadBytes));
        transfer.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        stream.upload = SDL_CreateGPUTransferBuffer(device, &transfer);
        if (!stream.upload) {
            return DeviceFailure(error, "Grow persistent Metal upload buffer");
        }
        stream.uploadCapacity = transfer.size;
        if (diagnosticsEnabled) {
            ++counters.streamAllocations;
        }
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

SDL_GPUCommandBuffer* MetalSession::Commands(std::string& error) {
    return impl_->Commands(error);
}

SDL_GPUCommandBuffer* MetalSession::UploadCommands(std::size_t bytes, std::string& error) {
    return impl_->UploadCommands(bytes, error);
}

bool MetalSession::UploadEffectVertices(const void* vertices, std::size_t bytes, SDL_GPUBufferBinding& binding,
    std::string& error) {
    auto& state = *impl_;
    if (!vertices || !bytes || bytes > kMaximumStreamingBytes) {
        return Fail(error, "Invalid Metal effect vertex upload");
    }
    auto* stream = state.NextStream(error, bytes);
    if (!stream || !state.EnsureStream(*stream, bytes, 0, bytes, error)) {
        return false;
    }
    auto* mapped = SDL_MapGPUTransferBuffer(state.device, stream->upload, false);
    if (!mapped) {
        return DeviceFailure(error, "Map Metal effect vertices");
    }
    std::memcpy(mapped, vertices, bytes);
    SDL_UnmapGPUTransferBuffer(state.device, stream->upload);
    auto* copy = SDL_BeginGPUCopyPass(state.command);
    if (!copy) {
        return DeviceFailure(error, "Begin Metal effect upload");
    }
    SDL_GPUTransferBufferLocation source{stream->upload, 0};
    SDL_GPUBufferRegion target{stream->vertices, 0, static_cast<Uint32>(bytes)};
    SDL_UploadToGPUBuffer(copy, &source, &target, false);
    SDL_EndGPUCopyPass(copy);
    binding = {stream->vertices, 0};
    CountCopyPass(bytes);
    return true;
}

SDL_GPUBuffer* MetalSession::CreateEffectMesh(const void* vertices, std::size_t bytes, std::string& error) {
    auto& state = *impl_;
    if (!vertices || !bytes || !state.UploadCommands(bytes, error)) {
        return nullptr;
    }
    SDL_GPUBufferCreateInfo bufferInfo{};
    bufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    bufferInfo.size = static_cast<Uint32>(bytes);
    auto* buffer = SDL_CreateGPUBuffer(state.device, &bufferInfo);
    SDL_GPUTransferBufferCreateInfo uploadInfo{};
    uploadInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    uploadInfo.size = bufferInfo.size;
    auto* upload = SDL_CreateGPUTransferBuffer(state.device, &uploadInfo);
    auto* mapped = upload ? SDL_MapGPUTransferBuffer(state.device, upload, false) : nullptr;
    if (!buffer || !mapped) {
        if (mapped) {
            SDL_UnmapGPUTransferBuffer(state.device, upload);
        }
        if (upload) {
            SDL_ReleaseGPUTransferBuffer(state.device, upload);
        }
        if (buffer) {
            SDL_ReleaseGPUBuffer(state.device, buffer);
        }
        DeviceFailure(error, "Allocate retained Metal effect geometry");
        return nullptr;
    }
    std::memcpy(mapped, vertices, bytes);
    SDL_UnmapGPUTransferBuffer(state.device, upload);
    auto* copy = SDL_BeginGPUCopyPass(state.command);
    if (!copy) {
        SDL_ReleaseGPUTransferBuffer(state.device, upload);
        SDL_ReleaseGPUBuffer(state.device, buffer);
        DeviceFailure(error, "Upload retained Metal effect geometry");
        return nullptr;
    }
    SDL_GPUTransferBufferLocation source{upload, 0};
    SDL_GPUBufferRegion target{buffer, 0, bufferInfo.size};
    SDL_UploadToGPUBuffer(copy, &source, &target, false);
    SDL_EndGPUCopyPass(copy);
    SDL_ReleaseGPUTransferBuffer(state.device, upload);
    CountCopyPass(bytes);
    return buffer;
}

void MetalSession::EnableDiagnostics(bool enabled) {
    impl_->diagnosticsEnabled = enabled;
    impl_->counters = {};
}

const RenderFrameCounters& MetalSession::Diagnostics() const {
    return impl_->counters;
}

std::uint64_t MetalSession::EffectClock() const {
    return impl_->diagnosticsEnabled ? SDL_GetTicksNS() : 0;
}

void MetalSession::CountEffectDraw(std::uint64_t start, bool scene) {
    impl_->sceneDirty = impl_->sceneDirty || scene;
    if (impl_->diagnosticsEnabled) {
        if (start) {
            constexpr double kNanosecondsPerMillisecond = 1000000;
            impl_->counters.encodeMs += (SDL_GetTicksNS() - start) / kNanosecondsPerMillisecond;
        }
        ++impl_->counters.effectDraws;
        ++impl_->counters.gpuDraws;
        ++impl_->counters.renderPasses;
    }
}

bool MetalSession::ResolveScene(std::string& error) {
    auto& state = *impl_;
    if (!state.sceneDirty || !state.multisample) {
        return true;
    }
    auto* command = state.Commands(error);
    if (!command) {
        return false;
    }
    auto* destination = state.WritableResolve(error);
    if (!destination) {
        return false;
    }
    // Preserve multisample storage between draws; resolve only when a consumer samples the scene.
    // A load/resolve pass keeps the exact same MSAA samples and resolve operation as the draw passes.
    SDL_GPUColorTargetInfo target{};
    target.texture = state.multisample;
    target.resolve_texture = destination;
    target.load_op = SDL_GPU_LOADOP_LOAD;
    target.store_op = SDL_GPU_STOREOP_RESOLVE_AND_STORE;
    auto* pass = SDL_BeginGPURenderPass(command, &target, 1, nullptr);
    if (!pass) {
        return DeviceFailure(error, "Resolve Metal scene");
    }
    SDL_EndGPURenderPass(pass);
    state.CommitResolve(destination);
    CountRenderPass();
    return true;
}

void MetalSession::CountRenderPass() {
    if (impl_->diagnosticsEnabled) {
        ++impl_->counters.renderPasses;
    }
}

void MetalSession::CountCopyPass(std::size_t bytes) {
    if (impl_->diagnosticsEnabled) {
        ++impl_->counters.copyPasses;
        impl_->counters.uploadBytes += bytes;
        impl_->counters.effectUploadBytes += bytes;
    }
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
    state.vertex = state.Shader("LegacyVertex", SDL_GPU_SHADERSTAGE_VERTEX);
    state.worldVertex = state.Shader("LegacyWorldVertex", SDL_GPU_SHADERSTAGE_VERTEX);
    state.singleTextureVertex = state.Shader("LegacySingleTextureVertex", SDL_GPU_SHADERSTAGE_VERTEX);
    constexpr std::array<const char*, 4> entrypoints{{"LegacyFragmentHardwareSampling",
        "LegacyFragmentAlphaTestHardwareSampling", "LegacyFragment", "LegacyFragmentAlphaTest"}};
    for (std::size_t index = 0; index < entrypoints.size(); ++index) {
        state.fragments[index] = state.Shader(entrypoints[index], SDL_GPU_SHADERSTAGE_FRAGMENT);
    }
    if (!state.vertex || !state.worldVertex || !state.singleTextureVertex ||
        std::any_of(state.fragments.begin(), state.fragments.end(), [](const auto* fragment) {
            return fragment == nullptr;
        })) {
        Shutdown();
        return DeviceFailure(error, "Load embedded offline Metal shaders");
    }
    if (const char* path = SDL_getenv("CSE_METAL_FRAME_STATS")) {
        state.diagnosticsFile = std::fopen(path, "w");
        state.diagnosticsEnabled = state.diagnosticsFile != nullptr;
        if (state.diagnosticsFile) {
            std::fprintf(state.diagnosticsFile,
                "frame,packets,draws,effects,submissions,render_passes,scene_resolves,copy_passes,upload_bytes,"
                "fence_waits,preparation_allocations,stream_allocations,stream_evictions,cache_bytes,working_bytes,"
                "prepare_ms,encode_ms,wait_ms,present_ms,texture_lookups,sampler_lookups,pipeline_lookups,"
                "disconnected_mesh_ranges,empty_upload_segments,mesh_owner_checks,unused_texture_units,"
                "alpha_discard_draws,border_sampling_draws,leading_unscissored_clears,drawable_wait_ms,vsync,"
                "stream_vertex_bytes,stream_index_bytes,resident_mesh_bytes,legacy_texture_bytes,effect_upload_bytes,"
                "quad_vertex_bytes,borrowed_geometry_bytes\n");
        }
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
    auto* color = state.CreateResolvedColor(width, height);
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
    // Retire the old target's ordered commands before replacing it; SDL owns in-flight references.
    if (!state.Submit(error)) {
        SDL_ReleaseGPUTexture(state.device, color);
        SDL_ReleaseGPUTexture(state.device, depth);
        if (multisample) {
            SDL_ReleaseGPUTexture(state.device, multisample);
        }
        return false;
    }
    // If the retained presentation uses one of the old resolves, transfer
    // ownership to the presented slot instead of discarding that screenshot.
    if (state.color && state.color != state.presented) {
        SDL_ReleaseGPUTexture(state.device, state.color);
    }
    if (state.spareResolve && state.spareResolve != state.presented) {
        SDL_ReleaseGPUTexture(state.device, state.spareResolve);
    }
    state.spareResolve = nullptr;
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
    state.sceneDirty = false;
    error.clear();
    return true;
}

bool MetalSession::Render(const FrameSnapshot& frame, std::string& error, SceneOutput output) {
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
    if (frame.info.surfaceMeshEpoch != state.surfaceMeshEpoch) {
        // Pending GPU commands retain the pages already bound by earlier map packets.
        state.meshes.clear();
        state.surfaceMeshEpoch = frame.info.surfaceMeshEpoch;
    }
    if (!state.surfaceMeshEpoch) {
        // Standalone callers can retire individual mesh owners between segments.
        if (state.diagnosticsEnabled) {
            state.counters.meshOwnerChecks += state.meshes.size();
        }
        for (auto iterator = state.meshes.begin(); iterator != state.meshes.end();) {
            if (iterator->second.object.expired()) {
                iterator = state.meshes.erase(iterator);
            } else {
                ++iterator;
            }
        }
    }
    if (state.meshes.empty()) {
        state.activeMeshPages = {};
        state.meshPages.clear();
    }
    // Never evict a pipeline/sampler after this frame has retained its pointer and before submission.
    // SDL keeps prior submitted command references alive when the owning cache releases its reference.
    if (state.pipelines.size() >= kMaximumCachedPipelines) {
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
    const auto prepareStart = state.diagnosticsEnabled ? Impl::Clock::now() : Impl::Clock::time_point{};
    if (state.diagnosticsEnabled) {
        state.counters.drawPackets += frame.draws.size();
        if (!frame.events.empty() && frame.events.front().kind == FrameEventKind::Clear &&
            !frame.events.front().clear.scissorEnabled) {
            ++state.counters.leadingUnscissoredClears;
        }
    }
    auto& draws = state.preparedDraws;
    // Keep only allocation capacity between segments. Source mesh ownership and cache pointers
    // must end on every return, including preparation/encoding failure and map retirement.
    struct PreparedScope {
        std::vector<Impl::Draw>& draws;
        ~PreparedScope() {
            draws.clear();
        }
    } preparedScope{draws};
    if (state.diagnosticsEnabled && frame.events.size() > draws.capacity()) {
        ++state.counters.preparationAllocations;
    }
    draws.reserve(frame.events.size());
    Impl::Preparation pending;
    std::size_t firstDraw = 0;
    SDL_FColor initialColor{0, 0, 0, 0};
    float initialDepth = 1;
    bool clearColor = !state.initializedTarget;
    bool clearDepth = !state.initializedTarget;
    // Only whole-attachment clears with exact endpoint values can replace the
    // corresponding load. Masked/scissored and other numeric clears remain draws.
    for (; firstDraw < frame.events.size(); ++firstDraw) {
        const auto& event = frame.events[firstDraw];
        const auto& clear = event.clear;
        if (event.kind != FrameEventKind::Clear || !clear.mask || clear.scissorEnabled) {
            break;
        }
        const bool clearsColor = (clear.mask & kClearColor) != 0;
        const bool clearsDepth = (clear.mask & kClearDepth) != 0;
        if (clearsDepth && (!clear.depthWrite || (clear.depth != 0 && clear.depth != 1))) {
            break;
        }
        const bool validColor = std::all_of(clear.color.begin(), clear.color.end(),
            [clearsColor](float component) {
                return clearsColor ? component == 0 || component == 1 : std::isfinite(component);
            });
        if (!validColor || (clearsColor && !std::all_of(clear.colorWrite.begin(), clear.colorWrite.end(),
            [](bool write) { return write; }))) {
            break;
        }
        if (clearsColor) {
            initialColor = {clear.color[0], clear.color[1], clear.color[2], clear.color[3]};
            clearColor = true;
        }
        if (clearsDepth) {
            initialDepth = static_cast<float>(clear.depth);
            clearDepth = true;
        }
    }
    std::size_t geometryBytes = 0;
    std::size_t vertexBytes = 0;
    std::size_t indexBytes = 0;
    for (std::size_t index = firstDraw; index < frame.events.size(); ++index) {
        const auto& event = frame.events[index];
        auto& draw = draws.emplace_back();
        if (event.kind == FrameEventKind::Clear) {
            if (!state.Prepare(ClearDraw(event.clear, state.width, state.height), draw, pending, error,
                MetalGeometryOwnership::Own)) {
                return false;
            }
        } else if (!state.Prepare(frame.draws[event.drawIndex], draw, pending, error, MetalGeometryOwnership::Borrow)) {
            return false;
        }
        if ((!draw.mesh && draw.data.Vertices().empty()) ||
            draw.data.raster.viewport[2] == 0 || draw.data.raster.viewport[3] == 0 ||
            (draw.data.topology == Primitive::Triangles &&
                draw.data.raster.enabled[static_cast<std::size_t>(Capability::CullFace)] &&
                draw.data.raster.cull == Cull::FrontAndBack)) {
            draws.pop_back();
            continue;
        }
        const auto vertexStride = draw.layout == Impl::VertexLayout::SingleTexture ?
            kSingleTextureVertexBytes : sizeof(Vertex);
        const auto drawVertexBytes = draw.data.Vertices().size() * vertexStride;
        const auto drawIndexBytes = draw.data.IndexCount() * sizeof(std::uint32_t);
        const auto bytes = drawVertexBytes + drawIndexBytes;
        if (bytes > kMaximumStreamingBytes - geometryBytes) {
            return Fail(error, "Frame geometry exceeds Metal streaming budget");
        }
        draw.vertexOffset = static_cast<Uint32>(vertexBytes);
        draw.indexOffset = static_cast<Uint32>(indexBytes);
        vertexBytes += drawVertexBytes;
        indexBytes += drawIndexBytes;
        geometryBytes += bytes;
        if (state.diagnosticsEnabled) {
            if (event.kind == FrameEventKind::Draw && frame.draws[event.drawIndex].primitive == Primitive::Quads) {
                state.counters.quadVertexBytes += drawVertexBytes;
            }
            if (draw.data.borrowedVertices) {
                state.counters.borrowedGeometryBytes += draw.data.Vertices().size() * sizeof(Vertex) + drawIndexBytes;
            }
        }
    }
    struct Upload {
        Impl::Texture* texture;
        unsigned level;
        Uint32 offset;
    };
    std::vector<Upload> uploads;
    auto uploadBytes = geometryBytes;
    struct MeshUpload {
        Impl::Mesh* mesh;
        std::shared_ptr<const SurfaceMesh> object;
        Uint32 vertexOffset;
        Uint32 indexOffset;
    };
    std::vector<MeshUpload> meshUploads;
    // Record first use while preparing draws. Existing residents need no upload planning,
    // and resources touched by this segment are protected from eviction until it ends.
    for (auto* mesh : pending.meshes) {
        const auto object = mesh->object.lock();
        const auto vertexBytes = object->Vertices().size() * mesh->page->vertexStride;
        const auto indexBytes = object->Indices().size() * sizeof(std::uint32_t);
        if (vertexBytes + indexBytes > kMaximumStreamingBytes - uploadBytes) {
            return Fail(error, "Frame meshes exceed Metal upload budget");
        }
        meshUploads.push_back({mesh, object, static_cast<Uint32>(uploadBytes),
            static_cast<Uint32>(uploadBytes + vertexBytes)});
        uploadBytes += vertexBytes + indexBytes;
    }
    const auto meshUploadBytes = uploadBytes - geometryBytes;
    for (auto* texture : pending.textures) {
        for (std::size_t level = 0; level < texture->data.levels.size(); ++level) {
            uploadBytes = Align(uploadBytes, kTransferAlignment);
            const auto bytes = texture->data.levels[level].pixels.size();
            if (uploadBytes > kMaximumStreamingBytes || bytes > kMaximumStreamingBytes - uploadBytes) {
                return Fail(error, "Frame textures exceed Metal streaming budget");
            }
            uploads.push_back({texture, static_cast<unsigned>(level), static_cast<Uint32>(uploadBytes)});
            uploadBytes += bytes;
        }
    }
    const auto textureUploadBytes = uploadBytes - geometryBytes - meshUploadBytes;
    SDL_GPUTexture* resolve = nullptr;
    if (state.multisample && output == SceneOutput::Resolved) {
        resolve = state.WritableResolve(error);
        if (!resolve) {
            return false;
        }
    }
    // Existing vertex pages can also share a draw when their original index ranges
    // are disjoint. Append only the reordered index ranges to the transfer buffer.
    // Never displace a required vertex, index, or texture upload.
    const auto meshIndexUploadOffset = Align(uploadBytes, alignof(std::uint32_t));
    std::size_t meshIndexBytes = 0;
    for (std::size_t first = 0; first < draws.size();) {
        auto& draw = draws[first];
        if (!draw.mesh) {
            ++first;
            continue;
        }
        auto count = draw.data.mesh->Indices().size();
        auto nextOffset = draw.mesh->indexOffset + count * sizeof(std::uint32_t);
        bool disjoint = false;
        auto end = first + 1;
        while (end < draws.size() && Impl::SameDrawState(draw, draws[end])) {
            const auto& next = draws[end];
            if (next.mesh->indexOffset != nextOffset) {
                disjoint = true;
                if (state.diagnosticsEnabled) {
                    ++state.counters.disconnectedMeshRanges;
                }
            }
            const auto nextCount = next.data.mesh->Indices().size();
            count += nextCount;
            nextOffset = next.mesh->indexOffset + nextCount * sizeof(std::uint32_t);
            ++end;
        }
        if (!disjoint) {
            draw.batchDraws = end - first;
            draw.batchIndices = count;
        } else {
            const auto bytes = count * sizeof(std::uint32_t);
            if (meshIndexUploadOffset <= kMaximumStreamingBytes &&
                bytes <= kMaximumStreamingBytes - meshIndexUploadOffset - meshIndexBytes) {
                draw.batchDraws = end - first;
                draw.batchIndices = count;
                draw.meshIndicesInStream = true;
                draw.indexOffset = static_cast<Uint32>(indexBytes + meshIndexBytes);
                meshIndexBytes += bytes;
            }
        }
        first = end;
    }
    if (meshIndexBytes) {
        uploadBytes = meshIndexUploadOffset + meshIndexBytes;
    }
    // Consecutive streamed indexed draws can address one vertex binding with
    // their existing index slices. Keep every primitive in its original order.
    for (std::size_t first = 0; first < draws.size();) {
        auto& draw = draws[first];
        if (draw.mesh || draw.data.IndexCount() == 0) {
            ++first;
            continue;
        }
        const auto vertexStride = draw.layout == Impl::VertexLayout::SingleTexture ?
            kSingleTextureVertexBytes : sizeof(Vertex);
        auto count = draw.data.IndexCount();
        auto end = first + 1;
        while (end < draws.size() && Impl::SameDrawState(draw, draws[end])) {
            auto& next = draws[end];
            next.streamedBaseVertex = static_cast<std::uint32_t>((next.vertexOffset - draw.vertexOffset) / vertexStride);
            count += next.data.IndexCount();
            ++end;
        }
        if (end > first + 1) {
            draw.batchDraws = end - first;
            draw.batchIndices = count;
        }
        first = end;
    }
    auto* stream = uploadBytes ? state.NextStream(error, uploadBytes) : nullptr;
    if (uploadBytes && !stream) {
        return false;
    }
    if (stream) {
        if (!state.EnsureStream(*stream, vertexBytes, indexBytes + meshIndexBytes, uploadBytes, error)) {
            return false;
        }
        auto* mapped = static_cast<std::uint8_t*>(SDL_MapGPUTransferBuffer(state.device, stream->upload, false));
        if (!mapped) {
            return DeviceFailure(error, "Map persistent Metal upload");
        }
        for (const auto& draw : draws) {
            const auto& vertices = draw.data.Vertices();
            if (draw.layout == Impl::VertexLayout::SingleTexture) {
                auto* destination = mapped + draw.vertexOffset;
                for (const auto& vertex : vertices) {
                    std::memcpy(destination, &vertex, kSingleTextureVertexBytes);
                    destination += kSingleTextureVertexBytes;
                }
            } else if (!vertices.empty()) {
                std::memcpy(mapped + draw.vertexOffset, vertices.data(), vertices.size() * sizeof(Vertex));
            }
            const auto& indices = draw.data.Indices();
            if (draw.data.indexedQuads) {
                auto* destination = reinterpret_cast<std::uint32_t*>(mapped + vertexBytes + draw.indexOffset);
                for (std::size_t corner = 0; corner < vertices.size(); corner += kQuadCorners) {
                    const auto base = draw.streamedBaseVertex + static_cast<std::uint32_t>(corner);
                    for (auto index : kQuadIndices) {
                        *destination++ = base + index;
                    }
                }
            } else if (!indices.empty()) {
                auto* destination = mapped + vertexBytes + draw.indexOffset;
                if (draw.streamedBaseVertex) {
                    const auto base = draw.streamedBaseVertex;
                    auto* rebased = reinterpret_cast<std::uint32_t*>(destination);
                    std::transform(indices.begin(), indices.end(), rebased,
                        [base](std::uint32_t value) { return value + base; });
                } else {
                    std::memcpy(destination, indices.data(), indices.size() * sizeof(std::uint32_t));
                }
            }
        }
        for (const auto& upload : meshUploads) {
            const auto& vertices = upload.object->Vertices();
            auto* destination = mapped + upload.vertexOffset;
            if (upload.object->HasWorldVertexLayout()) {
                for (const auto& vertex : vertices) {
                    const WorldMeshVertex packed{{{vertex.position[0], vertex.position[1], vertex.position[2]}},
                        {{vertex.texcoords[0][0], vertex.texcoords[0][1]}},
                        {{vertex.texcoords[1][0], vertex.texcoords[1][1]}}};
                    std::memcpy(destination, &packed, sizeof(packed));
                    destination += sizeof(packed);
                }
            } else {
                std::memcpy(destination, vertices.data(), vertices.size() * sizeof(Vertex));
            }
            // Page-relative indices let adjacent surfaces share a draw without rewriting their vertices.
            auto* indices = reinterpret_cast<std::uint32_t*>(mapped + upload.indexOffset);
            const auto baseVertex = static_cast<std::uint32_t>(upload.mesh->vertexOffset / upload.mesh->page->vertexStride);
            for (std::size_t index = 0; index < upload.object->Indices().size(); ++index) {
                indices[index] = upload.object->Indices()[index] + baseVertex;
            }
        }
        for (const auto& upload : uploads) {
            const auto& image = upload.texture->data.levels[upload.level];
            std::memcpy(mapped + upload.offset, image.pixels.data(), image.pixels.size());
        }
        if (meshIndexBytes) {
            auto* indices = reinterpret_cast<std::uint32_t*>(mapped + meshIndexUploadOffset);
            for (std::size_t index = 0; index < draws.size(); ++index) {
                const auto& draw = draws[index];
                if (!draw.meshIndicesInStream) {
                    continue;
                }
                for (auto offset = index; offset < index + draw.batchDraws; ++offset) {
                    const auto& piece = draws[offset];
                    const auto base = static_cast<std::uint32_t>(piece.mesh->vertexOffset / piece.mesh->page->vertexStride);
                    const auto& original = piece.data.mesh->Indices();
                    indices = std::transform(original.begin(), original.end(), indices,
                        [base](std::uint32_t value) { return value + base; });
                }
                index += draw.batchDraws - 1;
            }
        }
        SDL_UnmapGPUTransferBuffer(state.device, stream->upload);
    }
    auto* command = state.Commands(error);
    if (!command) {
        return false;
    }
    if (state.diagnosticsEnabled) {
        state.counters.prepareMs += std::chrono::duration<double, std::milli>(Impl::Clock::now() - prepareStart).count();
        state.counters.uploadBytes += uploadBytes;
        state.counters.streamedVertexBytes += vertexBytes;
        state.counters.streamedIndexBytes += indexBytes + meshIndexBytes;
        state.counters.residentMeshBytes += meshUploadBytes;
        state.counters.legacyTextureBytes += textureUploadBytes;
        if (stream) {
            ++state.counters.copyPasses;
        } else {
            ++state.counters.emptyUploadSegments;
        }
        ++state.counters.renderPasses;
    }
    Impl::Timer encodeTimer(state.diagnosticsEnabled ? &state.counters.encodeMs : nullptr);
    SDL_PushGPUDebugGroup(command, "Legacy scene / HUD");
    if (stream) {
        auto* copy = SDL_BeginGPUCopyPass(command);
        if (!copy) {
            SDL_PopGPUDebugGroup(command);
            return DeviceFailure(error, "Begin Metal upload pass");
        }
        if (vertexBytes) {
            SDL_GPUTransferBufferLocation source{stream->upload, 0};
            SDL_GPUBufferRegion destination{stream->vertices, 0, static_cast<Uint32>(vertexBytes)};
            SDL_UploadToGPUBuffer(copy, &source, &destination, false);
        }
        if (indexBytes) {
            SDL_GPUTransferBufferLocation source{stream->upload, static_cast<Uint32>(vertexBytes)};
            SDL_GPUBufferRegion destination{stream->indices, 0, static_cast<Uint32>(indexBytes)};
            SDL_UploadToGPUBuffer(copy, &source, &destination, false);
        }
        if (meshIndexBytes) {
            SDL_GPUTransferBufferLocation source{stream->upload, static_cast<Uint32>(meshIndexUploadOffset)};
            SDL_GPUBufferRegion destination{stream->indices, static_cast<Uint32>(indexBytes),
                static_cast<Uint32>(meshIndexBytes)};
            SDL_UploadToGPUBuffer(copy, &source, &destination, false);
        }
        for (const auto& upload : meshUploads) {
            SDL_GPUTransferBufferLocation source{stream->upload, upload.vertexOffset};
            SDL_GPUBufferRegion destination{upload.mesh->page->vertices, upload.mesh->vertexOffset,
                static_cast<Uint32>(upload.object->Vertices().size() * upload.mesh->page->vertexStride)};
            SDL_UploadToGPUBuffer(copy, &source, &destination, false);
            source.offset = upload.indexOffset;
            destination.buffer = upload.mesh->page->indices;
            destination.offset = upload.mesh->indexOffset;
            destination.size = static_cast<Uint32>(upload.object->Indices().size() * sizeof(std::uint32_t));
            SDL_UploadToGPUBuffer(copy, &source, &destination, false);
        }
        for (const auto& upload : uploads) {
            const auto& image = upload.texture->data.levels[upload.level];
            SDL_GPUTextureTransferInfo source{};
            source.transfer_buffer = stream->upload;
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
    }
    SDL_GPUColorTargetInfo color{};
    color.texture = state.multisample ? state.multisample : state.color;
    color.resolve_texture = resolve;
    color.clear_color = initialColor;
    color.load_op = clearColor ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
    // Keep the samples for later HUD/effect segments while delivering the resolved image.
    color.store_op = resolve ? SDL_GPU_STOREOP_RESOLVE_AND_STORE : SDL_GPU_STOREOP_STORE;
    SDL_GPUDepthStencilTargetInfo depth{};
    depth.texture = state.depth;
    depth.clear_depth = initialDepth;
    depth.load_op = clearDepth ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
    depth.store_op = SDL_GPU_STOREOP_STORE;
    depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    auto* pass = SDL_BeginGPURenderPass(command, &color, 1, &depth);
    if (!pass) {
        SDL_PopGPUDebugGroup(command);
        return DeviceFailure(error, "Begin Metal scene pass");
    }
    // Each pass starts with fresh bindings. Within the pass all legacy pipelines use
    // the same shader layout, so unchanged resources and uniforms remain valid.
    const Impl::Draw* previous = nullptr;
    SDL_Rect previousScissor{};
    SDL_GPUBufferBinding previousVertexBinding{};
    SDL_GPUBuffer* previousIndexBuffer = nullptr;
    for (std::size_t drawIndex = 0; drawIndex < draws.size(); ++drawIndex) {
        const auto& draw = draws[drawIndex];
        const auto& data = draw.data;
        auto vertexCount = data.Vertices().size();
        auto indexCount = draw.mesh ? data.mesh->Indices().size() : data.IndexCount();
        // Preserve order while combining adjacent stream ranges or resident ranges on the same page.
        // Material, uniforms, transforms, scissor and all dynamic raster state must match exactly.
        if (draw.batchDraws) {
            drawIndex += draw.batchDraws - 1;
            indexCount = draw.batchIndices;
        }
        while (!draw.batchDraws && (draw.mesh || (data.IndexCount() == 0)) && drawIndex + 1 < draws.size()) {
            const auto& next = draws[drawIndex + 1];
            if (!Impl::SameDrawState(draw, next)) {
                break;
            }
            if (draw.mesh) {
                if (next.mesh->indexOffset != draw.mesh->indexOffset + indexCount * sizeof(std::uint32_t)) {
                    break;
                }
                indexCount += next.data.mesh->Indices().size();
            } else {
                if (next.vertexOffset != draw.vertexOffset + vertexCount * sizeof(Vertex)) {
                    break;
                }
                vertexCount += next.data.Vertices().size();
            }
            ++drawIndex;
        }
        const auto& viewport = data.raster.viewport;
        const auto scissor = Scissor(data.raster, state.width, state.height);
        if (scissor.w == 0 || scissor.h == 0) {
            continue;
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
        if (!previous || previous->pipeline != draw.pipeline) {
            SDL_BindGPUGraphicsPipeline(pass, draw.pipeline);
        }
        const bool newViewport = !previous || previous->data.raster.viewport != viewport ||
            !SameBits(previous->data.raster.depthRange, data.raster.depthRange);
        if (newViewport || !SameBits(previous->data.vertex, data.vertex)) {
            auto nearDepth = static_cast<float>(data.raster.depthRange[0]);
            auto farDepth = static_cast<float>(data.raster.depthRange[1]);
            const bool reverseDepth = nearDepth > farDepth;
            if (reverseDepth) {
                std::swap(nearDepth, farDepth);
            }
            const auto width = static_cast<float>(right - left);
            const auto height = static_cast<float>(top - bottom);
            if (newViewport) {
                const SDL_GPUViewport gpuViewport{static_cast<float>(left), static_cast<float>(state.height - top),
                    width, height, nearDepth, farDepth};
                SDL_SetGPUViewport(pass, &gpuViewport);
            }
            auto constants = data.vertex;
            constexpr float kNdcExtent = 2.0f;
            const float scaleX = viewport[2] / width;
            const float scaleY = viewport[3] / height;
            const float offsetX = (kNdcExtent * (viewport[0] - left) + viewport[2] - width) / width;
            const float offsetY = (kNdcExtent * (viewport[1] - bottom) + viewport[3] - height) / height;
            for (std::size_t column = 0; column < 4; ++column) {
                auto* row = constants.projection.values.data() + column * 4;
                row[0] = scaleX * row[0] + offsetX * row[3];
                row[1] = scaleY * row[1] + offsetY * row[3];
                if (reverseDepth) {
                    row[2] *= -1;
                }
            }
            SDL_PushGPUVertexUniformData(command, 0, &constants, sizeof(constants));
        }
        if (!previous || previousScissor.x != scissor.x || previousScissor.y != scissor.y ||
            previousScissor.w != scissor.w || previousScissor.h != scissor.h) {
            SDL_SetGPUScissor(pass, &scissor);
            previousScissor = scissor;
        }
        SDL_GPUBufferBinding binding{draw.mesh ? draw.mesh->page->vertices : stream->vertices,
            draw.mesh ? 0 : draw.vertexOffset};
        if (previousVertexBinding.buffer != binding.buffer || previousVertexBinding.offset != binding.offset) {
            SDL_BindGPUVertexBuffers(pass, 0, &binding, 1);
            previousVertexBinding = binding;
        }
        if (!previous || previous->textures != draw.textures || previous->samplers != draw.samplers) {
            std::array<SDL_GPUTextureSamplerBinding, kTextureUnits> textures{};
            for (std::size_t unit = 0; unit < kTextureUnits; ++unit) {
                textures[unit] = {draw.textures[unit]->texture, draw.samplers[unit]};
            }
            SDL_BindGPUFragmentSamplers(pass, 0, textures.data(), static_cast<Uint32>(textures.size()));
        }
        if (!previous || !SameBits(previous->data.fragment, data.fragment)) {
            SDL_PushGPUFragmentUniformData(command, 0, &data.fragment, sizeof(data.fragment));
        }
        previous = &draw;
        if (state.diagnosticsEnabled) {
            ++state.counters.gpuDraws;
            if (data.fragment.legacy.fogAlpha[1] != 0 && data.raster.alphaFunction != Compare::Always) {
                ++state.counters.alphaDiscardDraws;
            }
            for (std::size_t unit = 0; unit < kTextureUnits; ++unit) {
                state.counters.unusedTextureUnits += data.fragment.legacy.unit[unit][0] == 0;
            }
            state.counters.borderSamplingDraws += draw.softwareBorder;
        }
        if (!draw.mesh && (data.IndexCount() == 0)) {
            SDL_DrawGPUPrimitives(pass, static_cast<Uint32>(vertexCount), 1, 0, 0);
        } else {
            const bool residentIndices = draw.mesh && !draw.meshIndicesInStream;
            auto* indexBuffer = residentIndices ? draw.mesh->page->indices : stream->indices;
            if (indexBuffer != previousIndexBuffer) {
                const SDL_GPUBufferBinding indices{indexBuffer, 0};
                SDL_BindGPUIndexBuffer(pass, &indices, SDL_GPU_INDEXELEMENTSIZE_32BIT);
                previousIndexBuffer = indexBuffer;
            }
            // All offsets are byte positions in uint32 index streams. Use firstIndex
            // for each range so repeated draws do not rebind the same buffer.
            const auto firstIndex = (residentIndices ? draw.mesh->indexOffset : draw.indexOffset) /
                sizeof(std::uint32_t);
            SDL_DrawGPUIndexedPrimitives(pass, static_cast<Uint32>(indexCount), 1,
                static_cast<Uint32>(firstIndex), 0, 0);
        }
    }
    SDL_EndGPURenderPass(pass);
    SDL_PopGPUDebugGroup(command);
    for (const auto& upload : meshUploads) {
        upload.mesh->uploaded = true;
    }
    for (const auto& upload : uploads) {
        upload.texture->uploaded = true;
        // The GPU transfer already holds these bytes. The mip dimensions and
        // retained texture byte charge still describe the sampled GPU resource.
        auto& pixels = upload.texture->data.levels[upload.level].pixels;
        std::vector<std::uint8_t>().swap(pixels);
    }
    state.initializedTarget = true;
    state.sceneDirty = true;
    if (resolve) {
        state.CommitResolve(resolve);
    }
    error.clear();
    return true;
}

bool MetalSession::Present(bool vsync, bool preserveAspect, std::string& error) {
    auto& state = *impl_;
    if (!state.device || !state.window || !state.color || !state.initializedTarget) {
        return Fail(error, "Metal presentation needs a window and rendered scene");
    }
    if (!ResolveScene(error)) {
        return false;
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
        if (state.CanPresentResolve(width, height)) {
            return true;
        }
        if (!state.presented || state.PresentedUsesResolve() ||
            state.presentedWidth != width || state.presentedHeight != height) {
            auto* presented = state.CreateResolvedColor(width, height);
            if (!presented) {
                return DeviceFailure(error, "Create retained Metal final-color target");
            }
            if (state.presented && !state.PresentedUsesResolve()) {
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
    const auto presentStart = state.diagnosticsEnabled ? Impl::Clock::now() : Impl::Clock::time_point{};
    auto* command = state.Commands(error);
    if (!command) {
        return false;
    }
    SDL_GPUTexture* swapchain = nullptr;
    const auto retireFailure = [&state, &error]() {
        const auto diagnostic = error;
        state.Submit(error); // An acquired drawable must be submitted; earlier scene work is retained too.
        error = diagnostic;
        return false;
    };
    Uint32 width = 0;
    Uint32 height = 0;
    const auto previousDrawableWait = state.counters.drawableWaitMs;
    {
        Impl::Timer drawableTimer(state.diagnosticsEnabled ? &state.counters.drawableWaitMs : nullptr);
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(command, state.window, &swapchain, &width, &height)) {
            DeviceFailure(error, "Acquire Metal swapchain image");
            return retireFailure();
        }
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
    // At 1:1 the resolve is already the final image. Keep it for readback and
    // copy straight to the drawable. Scaling and single-sample scenes keep a
    // separate retained image, since the next scene may load their color.
    const bool presentResolve = state.CanPresentResolve(width, height);
    if (!presentResolve) {
        SDL_BlitGPUTexture(command, &info);
        CountRenderPass();
    }
    if (swapchain) {
        info.source.texture = presentResolve ? state.color : state.presented;
        info.source.w = width;
        info.source.h = height;
        info.destination.texture = swapchain;
        info.destination.x = 0;
        info.destination.y = 0;
        info.destination.w = width;
        info.destination.h = height;
        SDL_BlitGPUTexture(command, &info);
        CountRenderPass();
    }
    if (!state.Submit(error)) {
        return false;
    }
    if (state.diagnosticsEnabled) {
        state.counters.presentMs += std::chrono::duration<double, std::milli>(Impl::Clock::now() - presentStart).count();
    }
    if (state.diagnosticsFile) {
        const auto& c = state.counters;
        cse_diagnostics::NativeFrameTimings().RecordMetalDrawableWait(c.drawableWaitMs - previousDrawableWait);
        std::fprintf(state.diagnosticsFile,
            "%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,"
            "%.6f,%.6f,%.6f,%.6f,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%.6f,%d,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n",
            static_cast<unsigned long long>(state.serial), static_cast<unsigned long long>(c.drawPackets),
            static_cast<unsigned long long>(c.gpuDraws), static_cast<unsigned long long>(c.effectDraws),
            static_cast<unsigned long long>(c.commandBuffers), static_cast<unsigned long long>(c.renderPasses),
            static_cast<unsigned long long>(c.sceneResolves), static_cast<unsigned long long>(c.copyPasses), static_cast<unsigned long long>(c.uploadBytes),
            static_cast<unsigned long long>(c.fenceWaits), static_cast<unsigned long long>(c.preparationAllocations),
            static_cast<unsigned long long>(c.streamAllocations),
            static_cast<unsigned long long>(c.streamEvictions), static_cast<unsigned long long>(c.streamCacheBytes),
            static_cast<unsigned long long>(c.streamWorkingBytes), c.prepareMs, c.encodeMs, c.waitMs, c.presentMs,
            static_cast<unsigned long long>(c.textureLookups), static_cast<unsigned long long>(c.samplerLookups),
            static_cast<unsigned long long>(c.pipelineLookups),
            static_cast<unsigned long long>(c.disconnectedMeshRanges),
            static_cast<unsigned long long>(c.emptyUploadSegments),
            static_cast<unsigned long long>(c.meshOwnerChecks),
            static_cast<unsigned long long>(c.unusedTextureUnits),
            static_cast<unsigned long long>(c.alphaDiscardDraws),
            static_cast<unsigned long long>(c.borderSamplingDraws),
            static_cast<unsigned long long>(c.leadingUnscissoredClears), c.drawableWaitMs, static_cast<int>(state.vsync),
            static_cast<unsigned long long>(c.streamedVertexBytes),
            static_cast<unsigned long long>(c.streamedIndexBytes),
            static_cast<unsigned long long>(c.residentMeshBytes),
            static_cast<unsigned long long>(c.legacyTextureBytes),
            static_cast<unsigned long long>(c.effectUploadBytes),
            static_cast<unsigned long long>(c.quadVertexBytes),
            static_cast<unsigned long long>(c.borrowedGeometryBytes));
        // Counters are cumulative, so callers can difference consecutive rows without per-frame reset races.
    }
    if (presentResolve) {
        if (state.presented && !state.PresentedUsesResolve()) {
            SDL_ReleaseGPUTexture(state.device, state.presented);
        }
        state.presented = state.color;
        state.presentedWidth = width;
        state.presentedHeight = height;
    } else if (state.spareResolve) {
        // A separate scaled presentation owns the retained pixels now.
        SDL_ReleaseGPUTexture(state.device, state.spareResolve);
        state.spareResolve = nullptr;
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
    if (!ResolveScene(error)) {
        return false;
    }
    if (!state.Submit(error)) {
        return false;
    }
    if (state.diagnosticsEnabled && width && height) {
        ++state.counters.commandBuffers;
        ++state.counters.copyPasses;
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
    if (!state.Submit(error)) {
        return false;
    }
    if (state.diagnosticsEnabled && width && height) {
        ++state.counters.commandBuffers;
        ++state.counters.copyPasses;
    }
    return ReadTarget(state.device, state.presented, state.presentedWidth, state.presentedHeight,
        x, y, width, height, components, alignment, output, error);
}

bool MetalSession::Finish(std::string& error) {
    auto& state = *impl_;
    if (!state.device) {
        return Fail(error, "Metal session is not initialized");
    }
    if (!state.Submit(error)) {
        return false;
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
    std::string error;
    state.Submit(error);
    SDL_WaitForGPUIdle(state.device);
    if (state.diagnosticsFile) {
        std::fclose(state.diagnosticsFile);
        state.diagnosticsFile = nullptr;
    }
    state.effects.reset();
    state.meshes.clear();
    state.activeMeshPages = {};
    state.meshPages.clear();
    for (auto& frame : state.streams) {
        if (frame.fence) {
            SDL_ReleaseGPUFence(state.device, frame.fence);
        }
        for (auto& stream : frame.chunks) {
            if (stream.vertices) {
                SDL_ReleaseGPUBuffer(state.device, stream.vertices);
            }
            if (stream.indices) {
                SDL_ReleaseGPUBuffer(state.device, stream.indices);
            }
            if (stream.upload) {
                SDL_ReleaseGPUTransferBuffer(state.device, stream.upload);
            }
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
    if (state.presented && !state.PresentedUsesResolve()) {
        SDL_ReleaseGPUTexture(state.device, state.presented);
    }
    if (state.spareResolve) {
        SDL_ReleaseGPUTexture(state.device, state.spareResolve);
    }
    if (state.color) {
        SDL_ReleaseGPUTexture(state.device, state.color);
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
    if (state.worldVertex) {
        SDL_ReleaseGPUShader(state.device, state.worldVertex);
    }
    if (state.singleTextureVertex) {
        SDL_ReleaseGPUShader(state.device, state.singleTextureVertex);
    }
    for (auto* fragment : state.fragments) {
        if (fragment) {
            SDL_ReleaseGPUShader(state.device, fragment);
        }
    }
    if (state.window) {
        SDL_ReleaseWindowFromGPUDevice(state.device, state.window);
    }
    SDL_DestroyGPUDevice(state.device);
    impl_.reset(new Impl);
}
}
}
