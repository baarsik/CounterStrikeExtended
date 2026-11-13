#include "gpu_adapter.h"
#include <algorithm>
#include <cstring>
#include <limits>

namespace cse {
namespace render {
namespace {

constexpr std::uint64_t kMaximumStagingBytes = 128 * 1024 * 1024;
constexpr std::uint64_t kTextureRowAlignment = 256;
constexpr std::uint64_t kTextureOffsetAlignment = 512;
constexpr std::uint32_t kPixelComponents = 4;

std::uint64_t Aligned(std::uint64_t value, std::uint64_t alignment) {
    return (value + alignment - 1) / alignment * alignment;
}

GpuStatus Failure(std::string& error, const char* stage) {
    error = std::string(stage) + ": " + SDL_GetError();
    return GpuStatus::DeviceFailure;
}

struct Resources {
    explicit Resources(SDL_GPUDevice* value) : device(value) {
    }
    Resources(const Resources&) = delete;
    Resources& operator=(const Resources&) = delete;
    SDL_GPUDevice* device;
    SDL_GPUTexture* color = nullptr;
    SDL_GPUTexture* depth = nullptr;
    std::array<SDL_GPUTexture*, kTextureUnits> textures{};
    std::array<SDL_GPUSampler*, kTextureUnits> samplers{};
    SDL_GPUTransferBuffer* upload = nullptr;
    SDL_GPUTransferBuffer* download = nullptr;
    SDL_GPUBuffer* vertices = nullptr;
    SDL_GPUShader* vertex = nullptr;
    SDL_GPUShader* fragment = nullptr;
    SDL_GPUGraphicsPipeline* pipeline = nullptr;
    SDL_GPUFence* fence = nullptr;

    ~Resources() {
        SDL_WaitForGPUIdle(device);
        if (pipeline) {
            SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        }
        if (vertex) {
            SDL_ReleaseGPUShader(device, vertex);
        }
        if (fragment) {
            SDL_ReleaseGPUShader(device, fragment);
        }
        for (auto* sampler : samplers) {
            if (sampler) {
                SDL_ReleaseGPUSampler(device, sampler);
            }
        }
        for (auto* texture : textures) {
            if (texture) {
                SDL_ReleaseGPUTexture(device, texture);
            }
        }
        if (vertices) {
            SDL_ReleaseGPUBuffer(device, vertices);
        }
        if (upload) {
            SDL_ReleaseGPUTransferBuffer(device, upload);
        }
        if (download) {
            SDL_ReleaseGPUTransferBuffer(device, download);
        }
        if (fence) {
            SDL_ReleaseGPUFence(device, fence);
        }
        if (color) {
            SDL_ReleaseGPUTexture(device, color);
        }
        if (depth) {
            SDL_ReleaseGPUTexture(device, depth);
        }
    }
};

SDL_GPUTexture* Texture(SDL_GPUDevice* device, std::uint32_t width, std::uint32_t height,
    SDL_GPUTextureFormat format, SDL_GPUTextureUsageFlags usage) {
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = format;
    info.usage = usage;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    return SDL_CreateGPUTexture(device, &info);
}

SDL_GPUShader* Shader(SDL_GPUDevice* device, const std::vector<std::uint8_t>& bytes,
    SDL_GPUShaderFormat format, bool vertex) {
    SDL_GPUShaderCreateInfo info{};
    info.code = bytes.data();
    info.code_size = bytes.size();
    info.entrypoint = vertex ? "LegacyVertex" : "LegacyFragment";
    info.format = format;
    info.stage = vertex ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT;
    info.num_samplers = vertex ? 0 : static_cast<Uint32>(kTextureUnits);
    info.num_uniform_buffers = 1;
    return SDL_CreateGPUShader(device, &info);
}

SDL_GPUBlendFactor Blend(BlendFactor factor, bool alpha) {
    if (alpha) {
        switch (factor) {
        case BlendFactor::SourceColor:
            factor = BlendFactor::SourceAlpha;
            break;
        case BlendFactor::InverseSourceColor:
            factor = BlendFactor::InverseSourceAlpha;
            break;
        case BlendFactor::DestinationColor:
            factor = BlendFactor::DestinationAlpha;
            break;
        case BlendFactor::InverseDestinationColor:
            factor = BlendFactor::InverseDestinationAlpha;
            break;
        default:
            break;
        }
    }
    constexpr std::array<SDL_GPUBlendFactor, 10> factors{{SDL_GPU_BLENDFACTOR_ZERO, SDL_GPU_BLENDFACTOR_ONE,
        SDL_GPU_BLENDFACTOR_SRC_COLOR, SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR, SDL_GPU_BLENDFACTOR_SRC_ALPHA,
        SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_GPU_BLENDFACTOR_DST_ALPHA, SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_ALPHA,
        SDL_GPU_BLENDFACTOR_DST_COLOR, SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_COLOR}};
    return factors[static_cast<std::size_t>(factor)];
}

SDL_GPUGraphicsPipeline* Pipeline(Resources& resources, const RasterState& raster) {
    SDL_GPUColorTargetDescription color{};
    color.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    color.blend_state.enable_blend = raster.enabled[static_cast<std::size_t>(Capability::Blend)];
    color.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    color.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    color.blend_state.src_color_blendfactor = Blend(raster.blendSource, false);
    color.blend_state.dst_color_blendfactor = Blend(raster.blendDestination, false);
    color.blend_state.src_alpha_blendfactor = Blend(raster.blendSource, true);
    color.blend_state.dst_alpha_blendfactor = Blend(raster.blendDestination, true);
    SDL_GPUVertexBufferDescription buffer{};
    buffer.pitch = static_cast<Uint32>(sizeof(Vertex));
    buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    std::array<SDL_GPUVertexAttribute, 5> attributes{};
    for (std::size_t index = 0; index < attributes.size(); ++index) {
        attributes[index].location = static_cast<Uint32>(index);
        attributes[index].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        attributes[index].offset = static_cast<Uint32>(index * sizeof(std::array<float, 4>));
    }
    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = resources.vertex;
    info.fragment_shader = resources.fragment;
    info.vertex_input_state.vertex_buffer_descriptions = &buffer;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.vertex_attributes = attributes.data();
    info.vertex_input_state.num_vertex_attributes = static_cast<Uint32>(attributes.size());
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    info.rasterizer_state.cull_mode = !raster.enabled[static_cast<std::size_t>(Capability::CullFace)] ?
        SDL_GPU_CULLMODE_NONE : raster.cull == Cull::Front ? SDL_GPU_CULLMODE_FRONT : SDL_GPU_CULLMODE_BACK;
    info.rasterizer_state.front_face = raster.frontCounterClockwise ?
        SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE : SDL_GPU_FRONTFACE_CLOCKWISE;
    info.rasterizer_state.enable_depth_clip = true;
    info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    info.depth_stencil_state.enable_depth_test = raster.enabled[static_cast<std::size_t>(Capability::DepthTest)];
    info.depth_stencil_state.enable_depth_write = raster.depthWrite;
    info.depth_stencil_state.compare_op = static_cast<SDL_GPUCompareOp>(
        static_cast<std::uint32_t>(SDL_GPU_COMPAREOP_NEVER) + static_cast<std::uint32_t>(raster.depthFunction));
    info.target_info.num_color_targets = 1;
    info.target_info.color_target_descriptions = &color;
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    return SDL_CreateGPUGraphicsPipeline(resources.device, &info);
}

}

GpuStatus RenderGpuOffscreen(SDL_GPUDevice* device, const GpuOfflineShaders& shaders,
    const DrawPacket& packet, std::uint32_t width, std::uint32_t height,
    std::vector<std::uint8_t>& rgbaTopFirst, std::string& error) {
    GpuDrawData draw;
    const auto preparation = PrepareGpuDraw(packet, draw);
    if (preparation != GpuStatus::Ok) {
        error = "Captured draw is outside the first GPU profile";
        return preparation;
    }
    const auto rowBytes = Aligned(static_cast<std::uint64_t>(width) * kPixelComponents, kTextureRowAlignment);
    const auto readbackBytes = rowBytes * height;
    const auto vertexBytes = draw.vertices.size() * sizeof(Vertex);
    if (!device || width == 0 || height == 0 || width > kTextureDimension || height > kTextureDimension ||
        readbackBytes > kMaximumStagingBytes || draw.vertices.empty() || vertexBytes > kMaximumStagingBytes ||
        shaders.vertex.empty() || shaders.fragment.empty()) {
        error = "Invalid or oversized first GPU render input";
        return GpuStatus::InvalidValue;
    }
    const auto& viewport = draw.raster.viewport;
    if (viewport[0] < 0 || viewport[1] < 0 || static_cast<std::uint32_t>(viewport[0]) > width ||
        static_cast<std::uint32_t>(viewport[1]) > height ||
        static_cast<std::uint32_t>(viewport[2]) > width - static_cast<std::uint32_t>(viewport[0]) ||
        static_cast<std::uint32_t>(viewport[3]) > height - static_cast<std::uint32_t>(viewport[1])) {
        error = "Viewport outside first offscreen target profile";
        return GpuStatus::Unsupported;
    }
    Resources resources(device);
    resources.vertex = Shader(device, shaders.vertex, shaders.format, true);
    resources.fragment = Shader(device, shaders.fragment, shaders.format, false);
    if (!resources.vertex || !resources.fragment) {
        return Failure(error, "Create offline shader");
    }
    resources.pipeline = Pipeline(resources, draw.raster);
    resources.color = Texture(device, width, height, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);
    resources.depth = Texture(device, width, height, SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET);
    SDL_GPUBufferCreateInfo buffer{};
    buffer.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    buffer.size = static_cast<Uint32>(vertexBytes);
    resources.vertices = SDL_CreateGPUBuffer(device, &buffer);
    if (!resources.pipeline || !resources.color || !resources.depth || !resources.vertices) {
        return Failure(error, "Create pipeline/offscreen/vertex resources");
    }
    std::array<Uint32, kTextureUnits> offsets{};
    std::array<Uint32, kTextureUnits> pitches{};
    std::uint64_t uploadBytes = vertexBytes;
    // SDL 3.4.20's Metal upload uses destination width instead of pixels_per_row.
    // Its RGBA8 path accepts tight rows; retain D3D12's padded transfer alignment.
    const bool tightUploadRows = std::strcmp(SDL_GetGPUDeviceDriver(device), "metal") == 0;
    for (std::size_t unit = 0; unit < kTextureUnits; ++unit) {
        const auto& image = draw.images[unit];
        const auto textureWidth = image ? static_cast<Uint32>(image->width) : 1;
        const auto textureHeight = image ? static_cast<Uint32>(image->height) : 1;
        uploadBytes = Aligned(uploadBytes, kTextureOffsetAlignment);
        const auto tightPitch = static_cast<std::uint64_t>(textureWidth) * kPixelComponents;
        const auto pitch = tightUploadRows ? tightPitch : Aligned(tightPitch, kTextureRowAlignment);
        if (uploadBytes + pitch * textureHeight > kMaximumStagingBytes) {
            error = "Owned upload exceeds first staging budget";
            return GpuStatus::InvalidValue;
        }
        offsets[unit] = static_cast<Uint32>(uploadBytes);
        pitches[unit] = static_cast<Uint32>(pitch);
        uploadBytes += pitch * textureHeight;
        resources.textures[unit] = Texture(device, textureWidth, textureHeight,
            SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER);
        const auto& sourceSampler = draw.samplers[unit];
        SDL_GPUSamplerCreateInfo sampler{};
        sampler.min_filter = sourceSampler.minification == Filter::Linear ?
            SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
        sampler.mag_filter = sourceSampler.magnification == Filter::Linear ?
            SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
        sampler.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        sampler.address_mode_u = sourceSampler.wrapS == Wrap::Repeat ?
            SDL_GPU_SAMPLERADDRESSMODE_REPEAT : SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sampler.address_mode_v = sourceSampler.wrapT == Wrap::Repeat ?
            SDL_GPU_SAMPLERADDRESSMODE_REPEAT : SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        resources.samplers[unit] = SDL_CreateGPUSampler(device, &sampler);
        if (!resources.textures[unit] || !resources.samplers[unit]) {
            return Failure(error, "Create sampled image/sampler");
        }
    }
    SDL_GPUTransferBufferCreateInfo transfer{};
    transfer.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer.size = static_cast<Uint32>(uploadBytes);
    resources.upload = SDL_CreateGPUTransferBuffer(device, &transfer);
    transfer.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    transfer.size = static_cast<Uint32>(readbackBytes);
    resources.download = SDL_CreateGPUTransferBuffer(device, &transfer);
    if (!resources.upload || !resources.download) {
        return Failure(error, "Create transfer storage");
    }
    auto* mapped = static_cast<std::uint8_t*>(SDL_MapGPUTransferBuffer(device, resources.upload, false));
    if (!mapped) {
        return Failure(error, "Map upload storage");
    }
    std::memset(mapped, 0, static_cast<std::size_t>(uploadBytes));
    std::memcpy(mapped, draw.vertices.data(), vertexBytes);
    constexpr std::array<std::uint8_t, 4> kInactiveWhite{{255, 255, 255, 255}};
    for (std::size_t unit = 0; unit < kTextureUnits; ++unit) {
        const auto& image = draw.images[unit];
        const auto rows = image ? image->height : 1;
        const auto bytes = image ? static_cast<std::size_t>(image->width) * kPixelComponents : kInactiveWhite.size();
        for (int row = 0; row < rows; ++row) {
            const auto* source = image ? image->pixels.data() + static_cast<std::size_t>(row) * bytes :
                kInactiveWhite.data();
            std::memcpy(mapped + offsets[unit] + static_cast<std::size_t>(row) * pitches[unit], source, bytes);
        }
    }
    SDL_UnmapGPUTransferBuffer(device, resources.upload);
    auto* command = SDL_AcquireGPUCommandBuffer(device);
    if (!command) {
        return Failure(error, "Acquire GPU commands");
    }
    auto* copy = SDL_BeginGPUCopyPass(command);
    if (!copy) {
        SDL_CancelGPUCommandBuffer(command);
        return Failure(error, "Begin upload pass");
    }
    SDL_GPUTransferBufferLocation source{};
    source.transfer_buffer = resources.upload;
    SDL_GPUBufferRegion destination{};
    destination.buffer = resources.vertices;
    destination.size = static_cast<Uint32>(vertexBytes);
    SDL_UploadToGPUBuffer(copy, &source, &destination, false);
    for (std::size_t unit = 0; unit < kTextureUnits; ++unit) {
        SDL_GPUTextureTransferInfo textureSource{};
        textureSource.transfer_buffer = resources.upload;
        textureSource.offset = offsets[unit];
        textureSource.pixels_per_row = pitches[unit] / kPixelComponents;
        textureSource.rows_per_layer = draw.images[unit] ? static_cast<Uint32>(draw.images[unit]->height) : 1;
        SDL_GPUTextureRegion textureDestination{};
        textureDestination.texture = resources.textures[unit];
        textureDestination.w = draw.images[unit] ? static_cast<Uint32>(draw.images[unit]->width) : 1;
        textureDestination.h = textureSource.rows_per_layer;
        textureDestination.d = 1;
        SDL_UploadToGPUTexture(copy, &textureSource, &textureDestination, false);
    }
    SDL_EndGPUCopyPass(copy);
    SDL_GPUColorTargetInfo color{};
    color.texture = resources.color;
    color.clear_color = {0, 0, 0, 1};
    color.load_op = SDL_GPU_LOADOP_CLEAR;
    color.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPUDepthStencilTargetInfo depth{};
    depth.texture = resources.depth;
    depth.clear_depth = 1;
    depth.load_op = SDL_GPU_LOADOP_CLEAR;
    depth.store_op = SDL_GPU_STOREOP_DONT_CARE;
    depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    auto* pass = SDL_BeginGPURenderPass(command, &color, 1, &depth);
    if (!pass) {
        SDL_CancelGPUCommandBuffer(command);
        return Failure(error, "Begin render pass");
    }
    SDL_BindGPUGraphicsPipeline(pass, resources.pipeline);
    const SDL_GPUViewport gpuViewport{static_cast<float>(viewport[0]),
        static_cast<float>(height - static_cast<Uint32>(viewport[1] + viewport[3])),
        static_cast<float>(viewport[2]), static_cast<float>(viewport[3]),
        static_cast<float>(draw.raster.depthRange[0]), static_cast<float>(draw.raster.depthRange[1])};
    SDL_SetGPUViewport(pass, &gpuViewport);
    SDL_Rect scissor{0, 0, static_cast<int>(width), static_cast<int>(height)};
    if (draw.raster.enabled[static_cast<std::size_t>(Capability::ScissorTest)]) {
        const auto& rect = draw.raster.scissor;
        const auto left = std::max<std::int64_t>(0, rect[0]);
        const auto bottom = std::max<std::int64_t>(0, rect[1]);
        const auto right = std::min<std::int64_t>(width, static_cast<std::int64_t>(rect[0]) + rect[2]);
        const auto top = std::min<std::int64_t>(height, static_cast<std::int64_t>(rect[1]) + rect[3]);
        scissor = {static_cast<int>(std::min<std::int64_t>(left, width)),
            static_cast<int>(height - std::max<std::int64_t>(0, top)),
            static_cast<int>(std::max<std::int64_t>(0, right - left)),
            static_cast<int>(std::max<std::int64_t>(0, top - bottom))};
    }
    SDL_SetGPUScissor(pass, &scissor);
    SDL_GPUBufferBinding vertexBinding{};
    vertexBinding.buffer = resources.vertices;
    SDL_BindGPUVertexBuffers(pass, 0, &vertexBinding, 1);
    std::array<SDL_GPUTextureSamplerBinding, kTextureUnits> bindings{};
    for (std::size_t unit = 0; unit < kTextureUnits; ++unit) {
        bindings[unit] = {resources.textures[unit], resources.samplers[unit]};
    }
    SDL_BindGPUFragmentSamplers(pass, 0, bindings.data(), static_cast<Uint32>(bindings.size()));
    SDL_PushGPUVertexUniformData(command, 0, &draw.vertex, static_cast<Uint32>(sizeof(draw.vertex)));
    SDL_PushGPUFragmentUniformData(command, 0, &draw.fragment, static_cast<Uint32>(sizeof(draw.fragment)));
    SDL_DrawGPUPrimitives(pass, static_cast<Uint32>(draw.vertices.size()), 1, 0, 0);
    SDL_EndGPURenderPass(pass);
    copy = SDL_BeginGPUCopyPass(command);
    if (!copy) {
        SDL_CancelGPUCommandBuffer(command);
        return Failure(error, "Begin readback pass");
    }
    SDL_GPUTextureRegion colorSource{};
    colorSource.texture = resources.color;
    colorSource.w = width;
    colorSource.h = height;
    colorSource.d = 1;
    SDL_GPUTextureTransferInfo colorDestination{};
    colorDestination.transfer_buffer = resources.download;
    colorDestination.pixels_per_row = static_cast<Uint32>(rowBytes / kPixelComponents);
    colorDestination.rows_per_layer = height;
    SDL_DownloadFromGPUTexture(copy, &colorSource, &colorDestination);
    SDL_EndGPUCopyPass(copy);
    resources.fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command);
    if (!resources.fence || !SDL_WaitForGPUFences(device, true, &resources.fence, 1)) {
        return Failure(error, "Submit/fence render");
    }
    const auto* pixels = static_cast<const std::uint8_t*>(SDL_MapGPUTransferBuffer(device, resources.download, false));
    if (!pixels) {
        return Failure(error, "Map completed readback");
    }
    std::vector<std::uint8_t> result(static_cast<std::size_t>(width) * height * kPixelComponents);
    const auto tightRow = static_cast<std::size_t>(width) * kPixelComponents;
    for (std::uint32_t row = 0; row < height; ++row) {
        std::memcpy(result.data() + row * tightRow, pixels + row * rowBytes, tightRow);
    }
    SDL_UnmapGPUTransferBuffer(device, resources.download);
    rgbaTopFirst = std::move(result);
    error.clear();
    return GpuStatus::Ok;
}

}
}
