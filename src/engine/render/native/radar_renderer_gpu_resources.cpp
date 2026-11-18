#include "radar_renderer_gpu_internal.h"
#include "radar_embedded.h"
namespace cse {
namespace render {
RadarRendererGpu::Impl::Impl(MetalSession& owner, std::uint64_t generation) : session(owner), epoch(generation),
    device(owner.EffectAccess().device) {}

RadarRendererGpu::Impl::~Impl() {
    ClearPanels();
    if (filterVertices) {
        SDL_ReleaseGPUBuffer(device, filterVertices);
    }
    for (auto& texture : textures) {
        Release(texture.second);
    }
    Release(white);
    Release(radarBlur.filtered);
    Release(radarBlur.scratch);
    Release(smokedBlur.filtered);
    Release(smokedBlur.scratch);
    for (auto& pipeline : pipelines) {
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline.second);
    }
    if (vertex) {
        SDL_ReleaseGPUShader(device, vertex);
    }
    for (auto* fragment : fragments) {
        if (fragment) {
            SDL_ReleaseGPUShader(device, fragment);
        }
    }
}

void RadarRendererGpu::Impl::ClearPanels() {
    for (auto& panel : panels) {
        if (panel.second.gpu) {
            SDL_ReleaseGPUBuffer(device, panel.second.gpu);
        }
    }
    panels.clear();
    panelBytes = 0;
}

void RadarRendererGpu::Impl::Release(Texture& texture) {
    if (texture.gpu) {
        SDL_ReleaseGPUTexture(device, texture.gpu);
    }
    if (texture.sampler) {
        SDL_ReleaseGPUSampler(device, texture.sampler);
    }
    texture.gpu = nullptr;
    texture.sampler = nullptr;
}

bool RadarRendererGpu::Impl::Upload(Texture& texture) {
    if (texture.gpu) {
        return true;
    }
    auto object = texture.object.lock();
    if (!object) {
        if (texture.identity) {
            return false;
        }
        auto image = std::make_shared<TextureImage>();
        image->width = 1;
        image->height = 1;
        image->defined = true;
        image->pixels = {255, 255, 255, 255};
        auto fallback = std::make_shared<TextureObject>();
        fallback->levels[0] = image;
        fallback->sampler.minification = Filter::Linear;
        fallback->sampler.magnification = Filter::Linear;
        object = std::move(fallback);
    }
    MetalTextureData data;
    std::string error;
    if (!PrepareMetalTexture(*object, texture.palette, data, error) || data.levels.empty()) {
        return false;
    }
    const auto& base = data.levels.front();
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = base.width;
    info.height = base.height;
    info.layer_count_or_depth = 1;
    info.num_levels = static_cast<Uint32>(data.levels.size());
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    texture.gpu = SDL_CreateGPUTexture(device, &info);
    SDL_GPUSamplerCreateInfo sampling{};
    const auto& settings = object->sampler;
    sampling.min_filter = settings.minification == Filter::Linear ||
        settings.minification == Filter::LinearMipmapLinear || settings.minification == Filter::LinearMipmapNearest ?
        SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
    sampling.mag_filter = settings.magnification == Filter::Linear ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
    sampling.mipmap_mode = settings.minification == Filter::LinearMipmapLinear ||
        settings.minification == Filter::NearestMipmapLinear ? SDL_GPU_SAMPLERMIPMAPMODE_LINEAR :
        SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sampling.address_mode_u = settings.wrapS == Wrap::Repeat && texture.role == TextureRole::World ?
        SDL_GPU_SAMPLERADDRESSMODE_REPEAT : SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampling.address_mode_v = settings.wrapT == Wrap::Repeat && texture.role == TextureRole::World ?
        SDL_GPU_SAMPLERADDRESSMODE_REPEAT : SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampling.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampling.max_lod = std::max(1.0f, settings.minification > Filter::Linear ? float(data.levels.size() - 1) : 0.0f);
    const float anisotropy = EffectiveMetalAnisotropy(settings.requestedAnisotropy);
    sampling.enable_anisotropy = anisotropy > 1;
    sampling.max_anisotropy = anisotropy;
    texture.sampler = SDL_CreateGPUSampler(device, &sampling);
    if (!texture.gpu || !texture.sampler) {
        Release(texture);
        return false;
    }
    constexpr std::size_t kTextureTransferAlignment = 512;
    std::size_t bytes = 0;
    for (const auto& level : data.levels) {
        bytes += (level.pixels.size() + kTextureTransferAlignment - 1) / kTextureTransferAlignment * kTextureTransferAlignment;
    }
    auto* command = session.UploadCommands(bytes, error);
    if (!command) {
        Release(texture);
        return false;
    }
    SDL_GPUTransferBufferCreateInfo transferInfo{};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = static_cast<Uint32>(bytes);
    auto* transfer = SDL_CreateGPUTransferBuffer(device, &transferInfo);
    auto* mapped = transfer ? static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(device, transfer, false)) : nullptr;
    if (!mapped) {
        if (transfer) {
            SDL_ReleaseGPUTransferBuffer(device, transfer);
        }
        Release(texture);
        return false;
    }
    std::size_t offset = 0;
    for (const auto& level : data.levels) {
        std::memcpy(mapped + offset, level.pixels.data(), level.pixels.size());
        offset += (level.pixels.size() + kTextureTransferAlignment - 1) / kTextureTransferAlignment * kTextureTransferAlignment;
    }
    SDL_UnmapGPUTransferBuffer(device, transfer);
    auto* copy = SDL_BeginGPUCopyPass(command);
    if (!copy) {
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        Release(texture);
        return false;
    }
    offset = 0;
    for (std::size_t index = 0; index < data.levels.size(); ++index) {
        const auto& level = data.levels[index];
        SDL_GPUTextureTransferInfo source{};
        source.transfer_buffer = transfer;
        source.offset = static_cast<Uint32>(offset);
        source.pixels_per_row = level.width;
        source.rows_per_layer = level.height;
        SDL_GPUTextureRegion target{};
        target.texture = texture.gpu;
        target.mip_level = static_cast<Uint32>(index);
        target.w = level.width;
        target.h = level.height;
        target.d = 1;
        SDL_UploadToGPUTexture(copy, &source, &target, false);
        offset += (level.pixels.size() + kTextureTransferAlignment - 1) / kTextureTransferAlignment * kTextureTransferAlignment;
    }
    SDL_EndGPUCopyPass(copy);
    session.CountCopyPass(bytes);
    // SDL retains the encoded transfer reference until the shared command has completed.
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    return true;
}

CseRadarResult RadarRendererGpu::Impl::Resolve(CseRadarTexture handle, Texture*& texture, bool allowNull) {
    texture = nullptr;
    if (!handle.rendererEpoch && !handle.serial) {
        return allowNull ? CSE_RADAR_OK : CSE_RADAR_INVALID_TEXTURE;
    }
    if (!handle.rendererEpoch || !handle.serial) {
        return CSE_RADAR_INVALID_TEXTURE;
    }
    if (handle.rendererEpoch != epoch) {
        return CSE_RADAR_STALE_EPOCH;
    }
    const auto found = textures.find(handle.serial);
    if (found == textures.end()) {
        return CSE_RADAR_INVALID_TEXTURE;
    }
    auto& resource = found->second;
    if (!resource.owned && textureSource && sourceState) {
        auto current = textureSource->Find(resource.identity & kMaximumGeneratedTextureName);
        if (!current || current->generation != resource.generation) {
            Release(resource);
            textures.erase(found);
            return CSE_RADAR_INVALID_TEXTURE;
        }
        auto palette = resource.palette;
        if (paletteSource && !current->levels.empty() &&
            current->levels.begin()->second->internal == InternalFormat::ColorIndex8) {
            palette = paletteSource(resource.identity);
        }
        const bool samePalette = palette == resource.palette ||
            (palette && resource.palette && *palette == *resource.palette);
        if (current != resource.object.lock() || !samePalette) {
            Release(resource);
            resource.object = std::move(current);
            resource.palette = std::move(palette);
        }
    }
    if (!resource.owned && resource.object.expired()) {
        Release(resource);
        textures.erase(found);
        return CSE_RADAR_INVALID_TEXTURE;
    }
    texture = &resource;
    return CSE_RADAR_OK;
}

void RadarRendererGpu::Impl::RetireBorrowedTextures() {
    // Never require a successful upload or a query of an abandoned client handle to reclaim map resources.
    // Owned map images have an explicit ReleaseImage lifetime and are excluded from this sweep.
    for (auto iterator = textures.begin(); iterator != textures.end();) {
        const auto current = iterator++;
        auto& resource = current->second;
        if (resource.owned) {
            continue;
        }
        const auto object = textureSource ? textureSource->Find(resource.identity & kMaximumGeneratedTextureName) :
            resource.object.lock();
        if (!object || object->generation != resource.generation) {
            Release(resource);
            textures.erase(current);
        }
    }
}

SDL_GPUTexture* RadarRendererGpu::Impl::Target(unsigned width, unsigned height, bool depth) {
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = depth ? SDL_GPU_TEXTUREFORMAT_D32_FLOAT : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.usage = depth ? SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET :
        SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    return SDL_CreateGPUTexture(device, &info);
}

SDL_GPUSampler* RadarRendererGpu::Impl::LinearSampler() {
    SDL_GPUSamplerCreateInfo info{};
    info.min_filter = SDL_GPU_FILTER_LINEAR;
    info.mag_filter = SDL_GPU_FILTER_LINEAR;
    info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.max_lod = 1;
    return SDL_CreateGPUSampler(device, &info);
}

SDL_GPUShader* RadarRendererGpu::Impl::Shader(const char* entry, bool isVertex, unsigned samplers) {
    SDL_GPUShaderCreateInfo info{};
    info.code = kLegacyRuntimeMetal;
    info.code_size = sizeof(kLegacyRuntimeMetal);
    info.entrypoint = entry;
    info.format = SDL_GPU_SHADERFORMAT_METALLIB;
    info.stage = isVertex ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT;
    info.num_samplers = isVertex ? 0 : samplers;
    info.num_uniform_buffers = 1;
    return SDL_CreateGPUShader(device, &info);
}

SDL_GPUGraphicsPipeline* RadarRendererGpu::Impl::Pipeline(unsigned blend, SDL_GPUSampleCount samples, unsigned effect, bool bottom) {
    const unsigned key = blend + static_cast<unsigned>(samples) * 4 + effect * 32 + unsigned(bottom) * 512;
    const auto found = pipelines.find(key);
    if (found != pipelines.end()) {
        return found->second;
    }
    if (!vertex) {
        vertex = Shader("RadarVertex", true);
    }
    if (!fragments[effect]) {
        constexpr const char* kEntries[] = {"RadarPrimitive", "RadarMask", "RadarMesh", "RadarTone", "RadarFilter", "RadarBackdrop"};
        fragments[effect] = Shader(kEntries[effect], false, effect == 2 ? 2 : 1);
    }
    if (!vertex || !fragments[effect]) {
        return nullptr;
    }
    SDL_GPUVertexBufferDescription buffer{};
    buffer.pitch = sizeof(CseRadarPrimitiveVertex);
    buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    std::array<SDL_GPUVertexAttribute, 6> attributes{};
    constexpr std::array<Uint32, 6> kOffsets{{0, 8, 24, 40, 56, 72}};
    for (std::size_t index = 0; index < attributes.size(); ++index) {
        attributes[index].location = static_cast<Uint32>(index);
        attributes[index].format = index == 0 ? SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2 :
            SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        attributes[index].offset = kOffsets[index];
    }
    SDL_GPUColorTargetDescription target{};
    target.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    target.blend_state.enable_blend = effect < 2 || effect == 5;
    target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    target.blend_state.src_color_blendfactor = blend == CSE_RADAR_BLEND_PREMULTIPLIED ?
        SDL_GPU_BLENDFACTOR_ONE : SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    target.blend_state.src_alpha_blendfactor = target.blend_state.src_color_blendfactor;
    target.blend_state.dst_color_blendfactor = blend == CSE_RADAR_BLEND_ADDITIVE_ALPHA ?
        SDL_GPU_BLENDFACTOR_ONE : SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target.blend_state.dst_alpha_blendfactor = target.blend_state.dst_color_blendfactor;
    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = vertex;
    info.fragment_shader = fragments[effect];
    info.vertex_input_state.vertex_buffer_descriptions = &buffer;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.vertex_attributes = attributes.data();
    info.vertex_input_state.num_vertex_attributes = static_cast<Uint32>(attributes.size());
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.multisample_state.sample_count = samples;
    info.target_info.color_target_descriptions = &target;
    info.target_info.num_color_targets = 1;
    if (effect == 2) {
        info.target_info.has_depth_stencil_target = true;
        info.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        info.depth_stencil_state.enable_depth_test = true;
        info.depth_stencil_state.enable_depth_write = true;
        info.depth_stencil_state.compare_op = bottom ? SDL_GPU_COMPAREOP_GREATER_OR_EQUAL :
            SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    }
    auto* result = SDL_CreateGPUGraphicsPipeline(device, &info);
    if (result) {
        try {
            pipelines.emplace(key, result);
        } catch (...) {
            SDL_ReleaseGPUGraphicsPipeline(device, result);
            throw;
        }
    }
    return result;
}

CseRadarResult RadarRendererGpu::Impl::Draw(const std::vector<CseRadarPrimitiveVertex>& vertices, CseRadarHudSpace hud,
    const Uniforms& parameters, unsigned blend, const RasterState& raster, Texture* texture,
    unsigned effect, const MetalEffectAccess* privateTarget, Texture* lightmap,
    SDL_GPUTexture* depthTarget, bool clearColor, bool clearDepth, bool bottom, SDL_GPUBuffer* retainedVertices) {
    const auto encodeStart = session.EffectClock();
    const auto access = privateTarget ? *privateTarget : session.EffectAccess();
    if (!access.color || !access.width || !access.height) {
        return CSE_RADAR_NO_DRAW_TARGET;
    }
    const auto scissor = SceneScissor(raster, access);
    if (!scissor.w || !scissor.h || !raster.viewport[2] || !raster.viewport[3]) {
        return CSE_RADAR_OK;
    }
    if (!texture) {
        texture = &white;
    }
    if (!Upload(*texture)) {
        return CSE_RADAR_OUT_OF_MEMORY;
    }
    if (effect == 2) {
        if (!lightmap) {
            lightmap = &white;
        }
        if (!Upload(*lightmap)) {
            return CSE_RADAR_OUT_OF_MEMORY;
        }
    }
    auto* pipeline = Pipeline(blend, access.samples, effect, bottom);
    if (!pipeline) {
        return CSE_RADAR_DEVICE_LOST;
    }
    std::string error;
    SDL_GPUBufferBinding binding{retainedVertices, 0};
    if (!retainedVertices &&
        !session.UploadEffectVertices(vertices.data(), vertices.size() * sizeof(vertices[0]), binding, error)) {
        return CSE_RADAR_OUT_OF_MEMORY;
    }
    const bool useScenePass = !privateTarget && !depthTarget && !clearColor && !clearDepth && retainedVertices;
    SDL_GPUCommandBuffer* command = nullptr;
    SDL_GPURenderPass* pass = nullptr;
    if (useScenePass) {
        pass = session.SceneEffectPass(command, error);
        if (!pass) {
            return CSE_RADAR_DEVICE_LOST;
        }
    } else {
        command = session.Commands(error);
        if (!command) {
            return CSE_RADAR_DEVICE_LOST;
        }
        SDL_PushGPUDebugGroup(command, "Radar / backdrop");
        SDL_GPUColorTargetInfo color{};
        color.texture = access.multisample ? access.multisample : access.color;
        color.load_op = clearColor ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
        color.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPUDepthStencilTargetInfo depthInfo{};
        depthInfo.texture = depthTarget;
        depthInfo.clear_depth = bottom ? 0.0f : 1.0f;
        depthInfo.load_op = clearDepth ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
        depthInfo.store_op = SDL_GPU_STOREOP_STORE;
        depthInfo.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
        depthInfo.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        pass = SDL_BeginGPURenderPass(command, &color, 1, depthTarget ? &depthInfo : nullptr);
        if (!pass) {
            SDL_PopGPUDebugGroup(command);
            return CSE_RADAR_DEVICE_LOST;
        }
        session.CountRenderPass();
    }
    SDL_BindGPUGraphicsPipeline(pass, pipeline);
    SDL_BindGPUVertexBuffers(pass, 0, &binding, 1);
    SDL_SetGPUScissor(pass, &scissor);
    SDL_GPUTextureSamplerBinding sampled{texture->gpu, texture->sampler};
    SDL_BindGPUFragmentSamplers(pass, 0, &sampled, 1);
    if (effect == 2) {
        SDL_GPUTextureSamplerBinding lighting{lightmap->gpu, lightmap->sampler};
        SDL_BindGPUFragmentSamplers(pass, 1, &lighting, 1);
    }
    auto uniforms = parameters;
    // Full target viewport with an explicit HUD transform also handles clipped/negative viewports.
    uniforms.transform = {{2.0f * raster.viewport[2] / (hud.width * access.width),
        -2.0f * raster.viewport[3] / (hud.height * access.height),
        2.0f * raster.viewport[0] / access.width - 1.0f,
        2.0f * (raster.viewport[1] + raster.viewport[3]) / access.height - 1.0f}};
    if (privateTarget) {
        uniforms.transform = parameters.transform;
    }
    SDL_PushGPUVertexUniformData(command, 0, &uniforms, sizeof(uniforms));
    SDL_PushGPUFragmentUniformData(command, 0, &uniforms, sizeof(uniforms));
    SDL_DrawGPUPrimitives(pass, static_cast<Uint32>(vertices.size()), 1, 0, 0);
    if (!useScenePass) {
        SDL_EndGPURenderPass(pass);
        SDL_PopGPUDebugGroup(command);
    }
    session.CountEffectDraw(encodeStart, !privateTarget);
    return CSE_RADAR_OK;
}
}
}
