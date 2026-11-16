#include "radar_renderer_gpu_internal.h"
namespace cse {
namespace render {
RadarRendererGpu::RadarRendererGpu(MetalSession& session, std::uint64_t epoch) : impl_(new Impl(session, epoch)) {}
RadarRendererGpu::~RadarRendererGpu() = default;
void RadarRendererGpu::SetTextureSource(const TextureStore* textures, const LegacyState* state, PaletteProvider palettes) {
    impl_->textureSource = textures;
    impl_->sourceState = state;
    impl_->paletteSource = palettes;
}

CseRadarResult RadarRendererGpu::DrawMaskRing(const CseRadarMaskRingDraw& draw, const RasterState& raster) {
    if (draw.size < sizeof(draw) || draw.reserved || draw.reserved2 || !ValidHud(draw.hud) ||
        !ValidMask(draw.outer) || !ValidFade(draw.fade) || !Finite(draw.color, 4) ||
        !std::isfinite(draw.innerRadius) || draw.innerRadius < 0 || draw.innerRadius > draw.outer.radius) {
        return CSE_RADAR_INVALID_ARGUMENT;
    }
    if (draw.rendererEpoch != impl_->epoch) {
        return CSE_RADAR_STALE_EPOCH;
    }
    Uniforms uniforms;
    CopyMask(uniforms, draw.outer);
    CopyFade(uniforms, draw.fade);
    InheritAlpha(uniforms, raster);
    std::copy(draw.color, draw.color + 4, uniforms.color.begin());
    uniforms.effectMode[0] = 3;
    uniforms.effects[2] = draw.innerRadius;
    constexpr float kMaskPadding = 2;
    const float radius = draw.outer.radius + kMaskPadding;
    return impl_->Draw(Quad(draw.outer.center[0] - radius, draw.outer.center[1] - radius, radius * 2, radius * 2),
        draw.hud, uniforms, CSE_RADAR_BLEND_STRAIGHT, raster, nullptr, 1);
}

CseRadarResult RadarRendererGpu::DrawMapTexture(const CseRadarMapTextureDraw& draw, const RasterState& raster) {
    if (draw.size < sizeof(draw) || draw.reserved || !ValidHud(draw.hud) || !Finite(draw.modulation, 4) ||
        draw.mode > CSE_RADAR_MAP_PREMULTIPLIED_IMAGE || draw.vertexCount < 3 ||
        draw.vertexCount > CSE_RADAR_TEXTURED_POLYGON_CAPACITY || draw.alphaAa > 1 ||
        (draw.mode != CSE_RADAR_MAP_PREMULTIPLIED_IMAGE && draw.alphaAa) ||
        (draw.mode != CSE_RADAR_MAP_PLAIN_OVERVIEW && (!ValidMask(draw.mask) || !ValidFade(draw.fade) ||
            !std::isfinite(draw.gamma) || draw.gamma < 0))) {
        return CSE_RADAR_INVALID_ARGUMENT;
    }
    if (draw.mode == CSE_RADAR_MAP_PLAIN_OVERVIEW &&
        (draw.mask.shape || draw.mask.reserved || draw.mask.center[0] || draw.mask.center[1] || draw.mask.radius ||
            draw.mask.feather || draw.gamma || draw.fade.amount || draw.fade.rgb[0] || draw.fade.rgb[1] || draw.fade.rgb[2])) {
        return CSE_RADAR_INVALID_ARGUMENT;
    }
    if (draw.rendererEpoch != impl_->epoch) {
        return CSE_RADAR_STALE_EPOCH;
    }
    Impl::Texture* texture = nullptr;
    auto status = impl_->Resolve(draw.texture, texture);
    if (status != CSE_RADAR_OK) {
        return status;
    }
    const bool generated = draw.mode == CSE_RADAR_MAP_PREMULTIPLIED_IMAGE;
    if (texture->owned != generated || (!generated && texture->role != TextureRole::Sprite)) {
        return CSE_RADAR_INVALID_TEXTURE;
    }
    float orientation = 0;
    for (unsigned index = 0; index < draw.vertexCount; ++index) {
        const auto& vertex = draw.vertices[index];
        if (!Finite(vertex.position, 2) || !Finite(vertex.uv, 2)) {
            return CSE_RADAR_INVALID_ARGUMENT;
        }
        const auto& next = draw.vertices[(index + 1) % draw.vertexCount];
        // Every vertex must lie on the same side of every edge, including nonadjacent edges.
        // Adjacent cross products alone also accept self-intersecting stars.
        for (unsigned other = 0; other < draw.vertexCount; ++other) {
            const auto& point = draw.vertices[other];
            const double cross = (double(next.position[0]) - vertex.position[0]) *
                (double(point.position[1]) - vertex.position[1]) -
                (double(next.position[1]) - vertex.position[1]) *
                (double(point.position[0]) - vertex.position[0]);
            if (cross != 0) {
                if (orientation && (orientation > 0) != (cross > 0)) {
                    return CSE_RADAR_INVALID_ARGUMENT;
                }
                orientation = cross > 0 ? 1 : -1;
            }
        }
    }
    if (!orientation) {
        return CSE_RADAR_INVALID_ARGUMENT;
    }
    std::vector<CseRadarPrimitiveVertex> vertices;
    for (unsigned triangle = 1; triangle + 1 < draw.vertexCount; ++triangle) {
        for (unsigned index : {0u, triangle, triangle + 1}) {
            CseRadarPrimitiveVertex vertex{};
            std::copy(draw.vertices[index].position, draw.vertices[index].position + 2, vertex.position);
            std::copy(draw.vertices[index].uv, draw.vertices[index].uv + 2, vertex.triangle);
            vertices.push_back(vertex);
        }
    }
    Uniforms uniforms;
    CopyMask(uniforms, draw.mask);
    CopyFade(uniforms, draw.fade);
    InheritAlpha(uniforms, raster);
    std::copy(draw.modulation, draw.modulation + 4, uniforms.color.begin());
    if (generated) {
        for (unsigned index = 0; index < 3; ++index) {
            uniforms.color[index] *= draw.modulation[3];
        }
    }
    uniforms.effectMode[0] = static_cast<float>(draw.mode);
    uniforms.effects[1] = draw.gamma;
    uniforms.effects[3] = static_cast<float>(draw.alphaAa);
    return impl_->Draw(vertices, draw.hud, uniforms,
        generated ? CSE_RADAR_BLEND_PREMULTIPLIED : CSE_RADAR_BLEND_STRAIGHT, raster, texture, 1);
}

CseRadarResult RadarRendererGpu::BuildMapImage(const CseRadarMapImageBuild& build, CseRadarTexture& image) {
    image = {};
    if (build.size < sizeof(build) || build.tone > CSE_RADAR_TONE_TEXTURED || !build.passCount || !build.passes ||
        (build.vertexCount && !build.vertices) || (build.indexCount && !build.indices) ||
        (build.drawCount && !build.draws) || build.vertexCount > kMaximumVertexBytes / sizeof(CseRadarMapVertex) ||
        build.indexCount > kMaximumVertexBytes / sizeof(CseRadarPrimitiveVertex) ||
        build.drawCount > kMaximumVertexBytes / sizeof(CseRadarMapDraw) ||
        build.passCount > kMaximumVertexBytes / sizeof(CseRadarMapPass)) {
        return CSE_RADAR_INVALID_ARGUMENT;
    }
    if (build.rendererEpoch != impl_->epoch) {
        return CSE_RADAR_STALE_EPOCH;
    }
    for (unsigned index = 0; index < build.vertexCount; ++index) {
        const auto& vertex = build.vertices[index];
        if (!Finite(vertex.position, 3) || !Finite(vertex.baseUv, 2) || !Finite(vertex.lightmapUv, 2) ||
            !Finite(vertex.color, 4)) {
            return CSE_RADAR_INVALID_ARGUMENT;
        }
    }
    for (unsigned index = 0; index < build.indexCount; ++index) {
        if (build.indices[index] >= build.vertexCount) {
            return CSE_RADAR_INVALID_ARGUMENT;
        }
    }
    std::vector<std::array<Impl::Texture*, 2>> materials;
    for (unsigned index = 0; index < build.drawCount; ++index) {
        const auto& draw = build.draws[index];
        if (draw.reserved || draw.textureFlags > 3 || draw.textureFlags == CSE_RADAR_MAP_USE_LIGHTMAP ||
            draw.indexCount % 3 || draw.firstIndex > build.indexCount || draw.indexCount > build.indexCount - draw.firstIndex) {
            return CSE_RADAR_INVALID_ARGUMENT;
        }
        std::array<Impl::Texture*, 2> textures{};
        const CseRadarTexture handles[] = {draw.base, draw.lightmap};
        for (unsigned unit = 0; unit < 2; ++unit) {
            const bool enabled = (draw.textureFlags & (1u << unit)) != 0;
            auto status = impl_->Resolve(handles[unit], textures[unit], !enabled);
            if (status != CSE_RADAR_OK) {
                return status;
            }
            if ((!enabled && textures[unit]) || (textures[unit] &&
                (textures[unit]->owned || textures[unit]->role != TextureRole::World))) {
                return CSE_RADAR_INVALID_TEXTURE;
            }
        }
        materials.push_back(textures);
    }
    for (unsigned index = 0; index < build.passCount; ++index) {
        const auto& pass = build.passes[index];
        const float bounds[] = {pass.minX, pass.minY, pass.maxX, pass.maxY, pass.minZ, pass.maxZ};
        const auto& first = build.passes[0];
        if (!Finite(bounds, 6) || pass.maxX <= pass.minX || pass.maxY <= pass.minY || pass.maxZ <= pass.minZ ||
            pass.minX != first.minX || pass.minY != first.minY || pass.maxX != first.maxX || pass.maxY != first.maxY ||
            pass.depthOrder > CSE_RADAR_DEPTH_BOTTOMMOST || pass.clearColor != (index == 0 ? 1u : 0u) ||
            pass.firstDraw > build.drawCount || pass.drawCount > build.drawCount - pass.firstDraw) {
            return CSE_RADAR_INVALID_ARGUMENT;
        }
    }
    // Keep temporary GPU objects exception-safe until the public image takes ownership.
    struct BuildTargets {
        Impl* owner;
        Impl::Texture intermediate;
        Impl::Texture final;
        SDL_GPUTexture* depth = nullptr;
        ~BuildTargets() {
            owner->Release(intermediate);
            owner->Release(final);
            if (depth) {
                SDL_ReleaseGPUTexture(owner->device, depth);
            }
        }
    } targets{impl_.get(), {}, {}, nullptr};
    auto& intermediate = targets.intermediate;
    auto& final = targets.final;
    auto*& depth = targets.depth;
    intermediate.gpu = impl_->Target(CSE_RADAR_MAP_IMAGE_SIZE, CSE_RADAR_MAP_IMAGE_SIZE);
    intermediate.sampler = impl_->LinearSampler();
    depth = impl_->Target(CSE_RADAR_MAP_IMAGE_SIZE, CSE_RADAR_MAP_IMAGE_SIZE, true);
    final.owned = true;
    final.gpu = impl_->Target(CSE_RADAR_MAP_IMAGE_SIZE, CSE_RADAR_MAP_IMAGE_SIZE);
    final.sampler = impl_->LinearSampler();
    auto status = CseRadarResult(CSE_RADAR_OK);
    if (!intermediate.gpu || !intermediate.sampler || !depth || !final.gpu || !final.sampler) {
        status = CSE_RADAR_OUT_OF_MEMORY;
    }
    MetalEffectAccess target{impl_->device, intermediate.gpu, nullptr,
        CSE_RADAR_MAP_IMAGE_SIZE, CSE_RADAR_MAP_IMAGE_SIZE, SDL_GPU_SAMPLECOUNT_1};
    RasterState raster;
    raster.viewport = {{0, 0, CSE_RADAR_MAP_IMAGE_SIZE, CSE_RADAR_MAP_IMAGE_SIZE}};
    for (unsigned index = 0; index < build.passCount && status == CSE_RADAR_OK; ++index) {
        const auto& pass = build.passes[index];
        const bool bottom = pass.depthOrder == CSE_RADAR_DEPTH_BOTTOMMOST;
        auto* command = SDL_AcquireGPUCommandBuffer(impl_->device);
        if (!command) {
            status = CSE_RADAR_DEVICE_LOST;
            break;
        }
        SDL_GPUColorTargetInfo color{};
        color.texture = intermediate.gpu;
        color.load_op = pass.clearColor ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
        color.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPUDepthStencilTargetInfo depthInfo{};
        depthInfo.texture = depth;
        depthInfo.clear_depth = bottom ? 0 : 1;
        depthInfo.load_op = SDL_GPU_LOADOP_CLEAR;
        depthInfo.store_op = SDL_GPU_STOREOP_STORE;
        depthInfo.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
        depthInfo.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        auto* clear = SDL_BeginGPURenderPass(command, &color, 1, &depthInfo);
        SDL_EndGPURenderPass(clear);
        if (!SDL_SubmitGPUCommandBuffer(command)) {
            status = CSE_RADAR_DEVICE_LOST;
            break;
        }
        Uniforms uniforms;
        // Generated UV (0,0) maps to minX,minY: physical top row stores world minY.
        uniforms.transform = {{2 / (pass.maxX - pass.minX), -2 / (pass.maxY - pass.minY),
            -1 - 2 * pass.minX / (pass.maxX - pass.minX), 1 + 2 * pass.minY / (pass.maxY - pass.minY)}};
        uniforms.depth = {{pass.minZ, pass.maxZ, 1, 0}};
        for (unsigned ordinal = 0; ordinal < pass.drawCount && status == CSE_RADAR_OK; ++ordinal) {
            const auto drawIndex = pass.firstDraw + ordinal;
            const auto& draw = build.draws[drawIndex];
            if (!draw.indexCount) {
                continue;
            }
            uniforms.effects[0] = (draw.textureFlags & CSE_RADAR_MAP_USE_BASE) ? 1 : 0;
            uniforms.effects[1] = (draw.textureFlags & CSE_RADAR_MAP_USE_LIGHTMAP) ? 1 : 0;
            std::vector<CseRadarPrimitiveVertex> vertices;
            vertices.reserve(draw.indexCount);
            for (unsigned element = 0; element < draw.indexCount; ++element) {
                const auto& input = build.vertices[build.indices[draw.firstIndex + element]];
                CseRadarPrimitiveVertex vertex{};
                std::copy(input.position, input.position + 2, vertex.position);
                std::copy(input.color, input.color + 4, vertex.color);
                std::copy(input.baseUv, input.baseUv + 2, vertex.triangle);
                std::copy(input.lightmapUv, input.lightmapUv + 2, vertex.triangle + 2);
                vertex.triangle2[3] = input.position[2];
                vertices.push_back(vertex);
            }
            status = impl_->Draw(vertices, {1, 1}, uniforms, 0, raster, materials[drawIndex][0], 2, &target,
                materials[drawIndex][1], depth, false, false, bottom);
        }
    }
    if (status == CSE_RADAR_OK) {
        target.color = final.gpu;
        Uniforms tone;
        tone.transform = {{2, -2, -1, 1}};
        tone.effects[0] = build.tone == CSE_RADAR_TONE_TEXTURED ? 1 : 0;
        status = impl_->Draw(Quad(0, 0, 1, 1), {1, 1}, tone, 0, raster, &intermediate, 3, &target, nullptr,
            nullptr, true);
    }
    if (status != CSE_RADAR_OK) {
        return status;
    }
    const auto serial = impl_->nextTexture++;
    impl_->textures.emplace(serial, std::move(final));
    final.gpu = nullptr;
    final.sampler = nullptr;
    image = {impl_->epoch, serial};
    return CSE_RADAR_OK;
}

CseRadarResult RadarRendererGpu::ReleaseImage(CseRadarTexture image) {
    Impl::Texture* texture = nullptr;
    const auto result = impl_->Resolve(image, texture, true);
    if (result != CSE_RADAR_OK || !texture) {
        return result;
    }
    if (!texture->owned) {
        return CSE_RADAR_INVALID_TEXTURE;
    }
    impl_->Release(*texture);
    impl_->textures.erase(image.serial);
    return CSE_RADAR_OK;
}

CseRadarTexture RadarRendererGpu::RegisterTexture(std::uint32_t identity, TextureRole role,
    std::shared_ptr<const TextureObject> object, std::shared_ptr<const SharedPalette> palette) {
    if (!object || !identity) {
        return {};
    }
    for (auto& pair : impl_->textures) {
        auto& texture = pair.second;
        if (texture.identity == identity && texture.role == role) {
            if (texture.object->generation != object->generation) {
                impl_->Release(texture);
                impl_->textures.erase(pair.first);
                break; // A new engine object receives a fresh service serial.
            }
            const bool samePalette = texture.palette == palette ||
                (texture.palette && palette && *texture.palette == *palette);
            if (texture.object != object || !samePalette) {
                impl_->Release(texture);
                texture.object = std::move(object);
                texture.palette = std::move(palette);
            }
            return {impl_->epoch, pair.first};
        }
    }
    Impl::Texture texture;
    texture.identity = identity;
    texture.role = role;
    texture.object = std::move(object);
    texture.palette = std::move(palette);
    const auto serial = impl_->nextTexture++;
    impl_->textures.emplace(serial, std::move(texture));
    return {impl_->epoch, serial};
}

CseRadarResult RadarRendererGpu::DrawPrimitiveQuads(const CseRadarPrimitiveBatch& batch, const RasterState& raster) {
    if (batch.size < sizeof(batch) || batch.reserved || batch.blend > CSE_RADAR_BLEND_ADDITIVE_ALPHA ||
        !ValidHud(batch.hud) || !ValidFade(batch.fade) || !Finite(batch.clip.center, 2) ||
        !std::isfinite(batch.clip.radius) || batch.clip.shape > CSE_RADAR_SHAPE_SQUARE ||
        (batch.clip.shape && batch.clip.radius <= 0) || (batch.quadCount && !batch.vertices) ||
        batch.quadCount > kMaximumVertexBytes / (kQuadTriangles.size() * sizeof(CseRadarPrimitiveVertex))) {
        return CSE_RADAR_INVALID_ARGUMENT;
    }
    if (batch.rendererEpoch != impl_->epoch) {
        return CSE_RADAR_STALE_EPOCH;
    }
    Impl::Texture* atlas = nullptr;
    const auto atlasStatus = impl_->Resolve(batch.atlas, atlas, true);
    if (atlasStatus != CSE_RADAR_OK) {
        return atlasStatus;
    }
    if (atlas && (atlas->owned || atlas->role != TextureRole::Glyph)) {
        return CSE_RADAR_INVALID_TEXTURE;
    }
    for (std::size_t index = 0; index < std::size_t(batch.quadCount) * kVerticesPerQuad; ++index) {
        const auto& vertex = batch.vertices[index];
        if (vertex.shape[0] == CSE_RADAR_PRIMITIVE_GLYPH && !atlas) {
            return CSE_RADAR_INVALID_TEXTURE;
        }
        if (!Finite(vertex.position, 2) || !Finite(vertex.shape, 4) || !Finite(vertex.center, 4) ||
            !Finite(vertex.color, 4) || !Finite(vertex.triangle, 4) || !Finite(vertex.triangle2, 4) ||
            vertex.shape[0] < 0 || vertex.shape[0] > CSE_RADAR_PRIMITIVE_PLAYER_FADE ||
            std::floor(vertex.shape[0]) != vertex.shape[0]) {
            return CSE_RADAR_INVALID_ARGUMENT;
        }
    }
    for (std::size_t quad = 0; quad < batch.quadCount; ++quad) {
        const auto& first = batch.vertices[quad * kVerticesPerQuad];
        const auto shape = static_cast<unsigned>(first.shape[0]);
        const bool analytic = shape != CSE_RADAR_PRIMITIVE_SOLID && shape != CSE_RADAR_PRIMITIVE_GLYPH;
        if ((analytic && first.shape[1] <= 0) ||
            (analytic && shape != CSE_RADAR_PRIMITIVE_CONE && shape != CSE_RADAR_PRIMITIVE_TRIANGLE && first.shape[2] <= 0) ||
            (shape == CSE_RADAR_PRIMITIVE_CONE && (first.shape[3] <= 0 || first.triangle2[2] <= 0)) ||
            (shape == CSE_RADAR_PRIMITIVE_RING && (first.shape[3] < 0 || first.shape[3] > first.shape[2])) ||
            (shape == CSE_RADAR_PRIMITIVE_PLAYER_FADE && (first.triangle2[0] < 0 || first.triangle2[0] > 3 ||
                std::floor(first.triangle2[0]) != first.triangle2[0] || first.triangle2[1] < 0 || first.triangle2[2] < 0))) {
            return CSE_RADAR_INVALID_ARGUMENT;
        }
        for (std::size_t corner = 1; corner < kVerticesPerQuad; ++corner) {
            const auto& next = batch.vertices[quad * kVerticesPerQuad + corner];
            if (!std::equal(first.shape, first.shape + 4, next.shape) ||
                !std::equal(first.center, first.center + 4, next.center) ||
                !std::equal(first.triangle2, first.triangle2 + 4, next.triangle2) ||
                (shape != CSE_RADAR_PRIMITIVE_GLYPH && !std::equal(first.triangle, first.triangle + 4, next.triangle)) ||
                (shape != CSE_RADAR_PRIMITIVE_SOLID && !std::equal(first.color, first.color + 4, next.color))) {
                return CSE_RADAR_INVALID_ARGUMENT;
            }
        }
    }
    if (!batch.quadCount) {
        return CSE_RADAR_OK;
    }
    Uniforms uniforms;
    uniforms.clip = {{batch.clip.center[0], batch.clip.center[1],
        batch.clip.shape ? batch.clip.radius : kUnboundedClipRadius,
        batch.clip.shape == CSE_RADAR_SHAPE_SQUARE ? 1.0f : 0.0f}};
    std::copy(batch.fade.rgb, batch.fade.rgb + 3, uniforms.fade.begin());
    uniforms.fade[3] = batch.fade.amount;
    std::vector<CseRadarPrimitiveVertex> vertices;
    vertices.reserve(std::size_t(batch.quadCount) * kQuadTriangles.size());
    for (std::size_t quad = 0; quad < batch.quadCount; ++quad) {
        for (auto corner : kQuadTriangles) {
            vertices.push_back(batch.vertices[quad * kVerticesPerQuad + corner]);
        }
    }
    return impl_->Draw(vertices, batch.hud, uniforms, batch.blend, raster, atlas);
}
}
}
