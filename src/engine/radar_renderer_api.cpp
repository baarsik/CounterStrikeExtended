#include "quakedef.h"
#include "gl_model.h"
#include "client.h"
#include "../public/radar_renderer_api.h"
#include "../public/radar_engine_api.h"
#include "render/backend.h"
#include <cstring>
#include <algorithm>
#include <new>

#if defined(CSE_ENABLE_METAL)
#include "render/native/metal_session.h"
#include "render/native/metal_effect_access.h"
#include "render/native/radar_renderer_gpu.h"
#include "render/native/frame_stream.h"
#endif

namespace cse::radar {
std::uint64_t GraphicsGeneration();
bool GetEngineGlyph(std::uint32_t font, std::uint32_t character, Glyph* glyph);
}
bool ReadRadarTexturePalette(unsigned int encoded, unsigned char* output);
extern model_t mod_known[];
extern int mod_numknown;
extern int lightmap_textures[cse::radar::LightmapCount];

namespace {
static_assert(sizeof(float) == 4 && sizeof(CseRadarTexture) == 16 && sizeof(CseRadarState) == 32);
static_assert(sizeof(CseRadarWorldMaterial) == 40 && sizeof(CseRadarGlyph) == 56);
static_assert(sizeof(CseRadarMapVertex) == 44 && sizeof(CseRadarMapDraw) == 48 && sizeof(CseRadarMapPass) == 40);
static_assert(sizeof(CseRadarPrimitiveVertex) == 88 && sizeof(CseRadarMapTextureDraw) == 272);
static_assert(sizeof(CseRadarMaskRingDraw) == 88 && sizeof(CseRadarBackdropDraw) == 64);

CseRadarResult Epoch(std::uint64_t epoch) {
    if (!epoch || epoch != cse::radar::GraphicsGeneration()) {
        return CSE_RADAR_STALE_EPOCH;
    }
#if defined(CSE_ENABLE_METAL)
    if (cse::render::UsingMetal() && cse::render::BackendMetalSession()) {
        return CSE_RADAR_OK;
    }
#endif
    return CSE_RADAR_UNAVAILABLE;
}

#if defined(CSE_ENABLE_METAL)
using cse::render::RadarRendererGpu;
std::shared_ptr<const cse::render::SharedPalette> CapturePalette(std::uint32_t encoded) {
    auto palette = std::make_shared<cse::render::SharedPalette>();
    return ReadRadarTexturePalette(encoded, palette->data()) ? palette : nullptr;
}

RadarRendererGpu& Effects() {
    auto& effects = cse::render::BackendMetalSession()->RadarEffects(cse::radar::GraphicsGeneration());
    const auto* recorder = cse::render::BackendRecorder();
    effects.SetTextureSource(&recorder->Textures(), &recorder->State(), CapturePalette);
    return effects;
}

CseRadarTexture ResolveTexture(std::uint32_t encoded, RadarRendererGpu::TextureRole role) {
    auto* recorder = cse::render::BackendRecorder();
    if (!recorder || !encoded || !recorder->Textures().IsTexture(encoded & 0xffffu)) {
        return {};
    }
    auto state = recorder->State();
    // Local state copy selects a logical texture without mutating any engine binding/cache.
    state.textures[state.ActiveTexture()].texture2D = encoded & 0xffffu;
    auto object = recorder->Textures().Bound(state, cse::render::TextureTarget::Texture2D);
    if (!object || object->levels.empty() || !object->levels.begin()->second->defined) {
        return {};
    }
    std::shared_ptr<const cse::render::SharedPalette> palette;
    if (object->levels.begin()->second->internal == cse::render::InternalFormat::ColorIndex8) {
        palette = CapturePalette(encoded);
        if (!palette) {
            return {};
        }
    }
    return Effects().RegisterTexture(encoded, role, std::move(object), std::move(palette));
}
#endif

CseRadarResult CSE_ABI_CALL GetState(CseRadarState* output, std::uint32_t bytes) {
    if (!output || bytes < sizeof(*output)) {
        return CSE_RADAR_INVALID_ARGUMENT;
    }
    *output = {};
    output->size = sizeof(*output);
    output->version = CSE_RADAR_RENDERER_VERSION;
    output->backend = cse::render::UsingMetal() ? CSE_RADAR_BACKEND_METAL : CSE_RADAR_BACKEND_OPENGL;
#if defined(CSE_ENABLE_METAL)
    auto* session = cse::render::BackendMetalSession();
    if (cse::render::UsingMetal() && session && session->EffectAccess().device) {
        output->rendererEpoch = cse::radar::GraphicsGeneration();
        output->capabilities = CSE_RADAR_CAP_ALL;
        output->flags = CSE_RADAR_STATE_READY;
        if (cse::render::BackendSceneActive()) {
            output->flags |= CSE_RADAR_STATE_DRAW_TARGET;
        }
        return CSE_RADAR_OK;
    }
#endif
    return CSE_RADAR_UNAVAILABLE;
}

CseRadarResult CSE_ABI_CALL ResolveWorldMaterial(std::uint64_t epoch, std::int32_t index,
    CseRadarWorldMaterial* output, std::uint32_t bytes) {
    if (!output || bytes < sizeof(*output)) {
        return CSE_RADAR_INVALID_ARGUMENT;
    }
    *output = {};
    output->size = sizeof(*output);
    const auto status = Epoch(epoch);
    if (status != CSE_RADAR_OK) {
        return status;
    }
#if defined(CSE_ENABLE_METAL)
    auto* world = cl.worldmodel;
    if (!world || !world->surfaces || world->numsurfaces <= 0) {
        return CSE_RADAR_WAITING;
    }
    if (index < 0 || index >= world->numsurfaces) {
        return CSE_RADAR_INVALID_ARGUMENT;
    }
    const auto& surface = world->surfaces[index];
    const bool needsLightmap = (surface.flags & (SURF_DRAWSKY | SURF_DRAWTILED)) == 0;
    if (needsLightmap) {
        output->flags |= CSE_RADAR_WORLD_NEEDS_LIGHTMAP;
    }
    if (surface.texinfo && surface.texinfo->texture) {
        output->base = ResolveTexture(surface.texinfo->texture->gl_texturenum, RadarRendererGpu::TextureRole::World);
    }
    if (surface.lightmaptexturenum >= 0 && surface.lightmaptexturenum < cse::radar::LightmapCount) {
        output->lightmap = ResolveTexture(lightmap_textures[surface.lightmaptexturenum], RadarRendererGpu::TextureRole::World);
    }
    if (output->base.serial) {
        output->flags |= CSE_RADAR_WORLD_BASE_READY;
    }
    if (output->lightmap.serial) {
        output->flags |= CSE_RADAR_WORLD_LIGHTMAP_READY;
    }
    return output->base.serial && (!needsLightmap || output->lightmap.serial) ? CSE_RADAR_OK : CSE_RADAR_WAITING;
#else
    (void)index;
    return CSE_RADAR_UNAVAILABLE;
#endif
}

CseRadarResult CSE_ABI_CALL ResolveSpriteTexture(std::uint64_t epoch, const void* model, std::int32_t frame,
    CseRadarTexture* output) {
    if (!output) {
        return CSE_RADAR_INVALID_ARGUMENT;
    }
    *output = {};
    const auto status = Epoch(epoch);
    if (status != CSE_RADAR_OK) {
        return status;
    }
#if defined(CSE_ENABLE_METAL)
    const auto address = reinterpret_cast<std::uintptr_t>(model);
    const auto start = reinterpret_cast<std::uintptr_t>(mod_known);
    if (!model || address < start || (address - start) % sizeof(model_t) ||
        (address - start) / sizeof(model_t) >= static_cast<std::size_t>(mod_numknown)) {
        return CSE_RADAR_INVALID_ARGUMENT;
    }
    const auto* spriteModel = static_cast<const model_t*>(model);
    if (spriteModel->type != mod_sprite) {
        return CSE_RADAR_INVALID_ARGUMENT;
    }
    auto* sprite = static_cast<msprite_t*>(spriteModel->cache.data);
    if (!sprite || sprite->numframes <= 0) {
        return CSE_RADAR_WAITING;
    }
    const auto selected = frame < 0 || frame >= sprite->numframes ? 0 : frame;
    if (sprite->frames[selected].type != SPR_SINGLE) {
        return CSE_RADAR_UNAVAILABLE;
    }
    auto* image = R_GetSpriteFrame(sprite, frame);
    if (image) {
        *output = ResolveTexture(image->gl_texturenum, RadarRendererGpu::TextureRole::Sprite);
    }
    return output->serial ? CSE_RADAR_OK : CSE_RADAR_WAITING;
#else
    (void)model;
    (void)frame;
    return CSE_RADAR_UNAVAILABLE;
#endif
}

CseRadarResult CSE_ABI_CALL ResolveGlyph(std::uint64_t epoch, std::uint32_t font, std::uint32_t character,
    CseRadarGlyph* output, std::uint32_t bytes) {
    if (!output || bytes < sizeof(*output)) {
        return CSE_RADAR_INVALID_ARGUMENT;
    }
    *output = {};
    output->size = sizeof(*output);
    if (!font || character > 0xffffu) {
        return CSE_RADAR_INVALID_ARGUMENT;
    }
    const auto status = Epoch(epoch);
    if (status != CSE_RADAR_OK) {
        return status;
    }
#if defined(CSE_ENABLE_METAL)
    cse::radar::Glyph glyph{};
    if (!cse::radar::GetEngineGlyph(font, character, &glyph)) {
        return CSE_RADAR_WAITING;
    }
    output->bearingX = glyph.bearing_x;
    output->width = glyph.wide;
    output->height = glyph.tall;
    output->advance = glyph.advance;
    std::copy(glyph.uv, glyph.uv + 4, output->uv);
    if (!glyph.wide) {
        return CSE_RADAR_OK;
    }
    output->atlas = ResolveTexture(glyph.texture, RadarRendererGpu::TextureRole::Glyph);
    return output->atlas.serial ? CSE_RADAR_OK : CSE_RADAR_WAITING;
#else
    return CSE_RADAR_UNAVAILABLE;
#endif
}

CseRadarResult CSE_ABI_CALL BuildMapImage(const CseRadarMapImageBuild* build, CseRadarTexture* output) {
    if (!output) {
        return CSE_RADAR_INVALID_ARGUMENT;
    }
    *output = {};
    if (!build || build->size < sizeof(*build)) {
        return CSE_RADAR_INVALID_ARGUMENT;
    }
    const auto status = Epoch(build->rendererEpoch);
    if (status != CSE_RADAR_OK) {
        return status;
    }
#if defined(CSE_ENABLE_METAL)
    cse::render::FlushBackend();
    return Effects().BuildMapImage(*build, *output);
#else
    return CSE_RADAR_UNAVAILABLE;
#endif
}

CseRadarResult CSE_ABI_CALL ReleaseImage(const CseRadarTexture* image) {
    if (!image) {
        return CSE_RADAR_INVALID_ARGUMENT;
    }
    if (!image->rendererEpoch && !image->serial) {
        return CSE_RADAR_OK;
    }
    const auto status = Epoch(image->rendererEpoch);
    if (status != CSE_RADAR_OK) {
        return status;
    }
#if defined(CSE_ENABLE_METAL)
    return Effects().ReleaseImage(*image);
#else
    return CSE_RADAR_UNAVAILABLE;
#endif
}

template<typename Descriptor, typename Operation>
CseRadarResult Draw(const Descriptor* draw, Operation operation) {
    if (!draw || draw->size < sizeof(*draw)) {
        return CSE_RADAR_INVALID_ARGUMENT;
    }
    const auto status = Epoch(draw->rendererEpoch);
    if (status != CSE_RADAR_OK) {
        return status;
    }
#if defined(CSE_ENABLE_METAL)
    if (!cse::render::BackendSceneActive()) {
        return CSE_RADAR_NO_DRAW_TARGET;
    }
    cse::render::FlushBackend();
    return operation(Effects(), *draw, cse::render::BackendRecorder()->State().raster);
#else
    (void)operation;
    return CSE_RADAR_UNAVAILABLE;
#endif
}

#define CSE_RADAR_DRAW(Name, Descriptor) \
CseRadarResult CSE_ABI_CALL Name(const Descriptor* draw) { \
    return Draw(draw, [](auto& effects, const auto& input, const auto& raster) { \
        return effects.Name(input, raster); \
    }); \
}
CSE_RADAR_DRAW(DrawPrimitiveQuads, CseRadarPrimitiveBatch)
CSE_RADAR_DRAW(DrawMapTexture, CseRadarMapTextureDraw)
CSE_RADAR_DRAW(DrawMaskRing, CseRadarMaskRingDraw)
CSE_RADAR_DRAW(DrawRadarBackdrop, CseRadarBackdropDraw)
#undef CSE_RADAR_DRAW

// Every ABI entry translates allocation failures; no C++ exception crosses the module table.
template<auto Function, typename... Arguments>
CseRadarResult CSE_ABI_CALL Guard(Arguments... arguments) {
    try {
        return Function(arguments...);
    } catch (const std::bad_alloc&) {
        return CSE_RADAR_OUT_OF_MEMORY;
    } catch (...) {
        return CSE_RADAR_DEVICE_LOST;
    }
}

CseRadarRendererApi api{sizeof(api), CSE_RADAR_RENDERER_VERSION, sizeof(void*) * 8, 0,
    Guard<GetState, CseRadarState*, std::uint32_t>,
    Guard<ResolveWorldMaterial, std::uint64_t, std::int32_t, CseRadarWorldMaterial*, std::uint32_t>,
    Guard<ResolveSpriteTexture, std::uint64_t, const void*, std::int32_t, CseRadarTexture*>,
    Guard<ResolveGlyph, std::uint64_t, std::uint32_t, std::uint32_t, CseRadarGlyph*, std::uint32_t>,
    Guard<BuildMapImage, const CseRadarMapImageBuild*, CseRadarTexture*>,
    Guard<ReleaseImage, const CseRadarTexture*>,
    Guard<DrawPrimitiveQuads, const CseRadarPrimitiveBatch*>,
    Guard<DrawMapTexture, const CseRadarMapTextureDraw*>,
    Guard<DrawMaskRing, const CseRadarMaskRingDraw*>,
    Guard<DrawRadarBackdrop, const CseRadarBackdropDraw*>};
void* CreateRadarRenderer() {
    return &api;
}
EXPOSE_INTERFACE_FN(CreateRadarRenderer, CseRadarRendererApi, CSE_RADAR_RENDERER_INTERFACE);
}
