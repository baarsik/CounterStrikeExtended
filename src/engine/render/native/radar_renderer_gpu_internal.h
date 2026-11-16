#pragma once
// Private shared implementation for descriptor, streaming-resource and blur effect owners.
#include "radar_renderer_gpu.h"
#include "metal_session.h"
#include "metal_effect_access.h"
#include "metal_draw.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
namespace cse {
namespace render {
namespace radar_gpu {

constexpr std::size_t kStreamingSlots = 64;
constexpr std::size_t kMaximumVertexBytes = 16 * 1024 * 1024;
constexpr std::size_t kInitialStreamBytes = 4096;
constexpr std::size_t kVerticesPerQuad = 4;
constexpr std::array<unsigned, 6> kQuadTriangles{{0, 1, 2, 0, 2, 3}};
constexpr float kUnboundedClipRadius = 1.0e9f;

struct Uniforms {
    std::array<float, 4> transform{};
    std::array<float, 4> clip{};
    std::array<float, 4> fade{};
    std::array<float, 4> effects{};
    std::array<float, 4> effectMode{};
    std::array<float, 4> color{};
    std::array<float, 4> alpha{};
    std::array<float, 4> depth{};
};

inline bool Finite(const float* values, std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) {
        if (!std::isfinite(values[index])) {
            return false;
        }
    }
    return true;
}

inline bool ValidHud(CseRadarHudSpace hud) {
    return std::isfinite(hud.width) && std::isfinite(hud.height) && hud.width > 0 && hud.height > 0;
}

inline bool ValidFade(const CseRadarFade& fade) {
    return Finite(fade.rgb, 3) && std::isfinite(fade.amount) && fade.amount >= 0 && fade.amount <= 1;
}

inline SDL_Rect SceneScissor(const RasterState& raster, const MetalEffectAccess& access) {
    std::int64_t left = 0;
    std::int64_t bottom = 0;
    std::int64_t right = access.width;
    std::int64_t top = access.height;
    if (raster.enabled[static_cast<std::size_t>(Capability::ScissorTest)]) {
        left = std::max<std::int64_t>(left, raster.scissor[0]);
        bottom = std::max<std::int64_t>(bottom, raster.scissor[1]);
        right = std::min<std::int64_t>(right, std::int64_t(raster.scissor[0]) + raster.scissor[2]);
        top = std::min<std::int64_t>(top, std::int64_t(raster.scissor[1]) + raster.scissor[3]);
    }
    left = std::min<std::int64_t>(access.width, left);
    bottom = std::min<std::int64_t>(access.height, bottom);
    right = std::max(left, right);
    top = std::max(bottom, top);
    return {static_cast<int>(left), static_cast<int>(access.height - top), static_cast<int>(right - left),
        static_cast<int>(top - bottom)};
}
inline void CopyFade(Uniforms& uniforms, const CseRadarFade& fade) {
    std::copy(fade.rgb, fade.rgb + 3, uniforms.fade.begin());
    uniforms.fade[3] = fade.amount;
}

inline void InheritAlpha(Uniforms& uniforms, const RasterState& raster) {
    uniforms.alpha = {{raster.enabled[static_cast<std::size_t>(Capability::AlphaTest)] ? 1.0f : 0.0f,
        static_cast<float>(raster.alphaFunction), raster.alphaReference, 0}};
}

inline bool ValidMask(const CseRadarMask& mask) {
    return !mask.reserved && Finite(mask.center, 2) && std::isfinite(mask.radius) && mask.radius > 0 &&
        std::isfinite(mask.feather) && mask.feather > 0 &&
        (mask.shape == CSE_RADAR_SHAPE_CIRCLE || mask.shape == CSE_RADAR_SHAPE_SQUARE);
}

inline void CopyMask(Uniforms& uniforms, const CseRadarMask& mask) {
    uniforms.clip = {{mask.center[0], mask.center[1], mask.radius,
        mask.shape == CSE_RADAR_SHAPE_SQUARE ? 1.0f : 0.0f}};
    uniforms.effects[0] = mask.feather;
}

inline std::vector<CseRadarPrimitiveVertex> Quad(float x, float y, float width, float height) {
    CseRadarPrimitiveVertex corners[4]{};
    constexpr float kCoordinates[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (unsigned index = 0; index < 4; ++index) {
        corners[index].position[0] = x + kCoordinates[index][0] * width;
        corners[index].position[1] = y + kCoordinates[index][1] * height;
        corners[index].triangle[0] = kCoordinates[index][0];
        corners[index].triangle[1] = kCoordinates[index][1];
    }
    std::vector<CseRadarPrimitiveVertex> result;
    for (auto index : kQuadTriangles) {
        result.push_back(corners[index]);
    }
    return result;
}
}
using namespace radar_gpu;
struct RadarRendererGpu::Impl {
    struct Stream {
        SDL_GPUBuffer* vertices = nullptr;
        SDL_GPUTransferBuffer* upload = nullptr;
        SDL_GPUFence* fence = nullptr;
        std::size_t capacity = 0;
    };
    struct Texture {
        bool owned = false;
        std::uint32_t identity = 0;
        TextureRole role = TextureRole::World;
        std::shared_ptr<const TextureObject> object;
        std::shared_ptr<const SharedPalette> palette;
        SDL_GPUTexture* gpu = nullptr;
        SDL_GPUSampler* sampler = nullptr;
    };
    const TextureStore* textureSource = nullptr;
    const LegacyState* sourceState = nullptr;
    PaletteProvider paletteSource = nullptr;
    std::map<std::uint64_t, Texture> textures;
    std::uint64_t nextTexture = 1;
    Texture white;
    struct Blur {
        Texture scene;
        Texture filtered;
        Texture scratch;
        unsigned width = 0;
        unsigned height = 0;
        int originX = 0;
        int originY = 0;
    } radarBlur, smokedBlur;
    bool smokedRequested = false;
    bool smokedCaptured = false;
    unsigned capturedSceneWidth = 0;
    unsigned capturedSceneHeight = 0;
    MetalSession& session;
    const std::uint64_t epoch;
    SDL_GPUDevice* device;
    SDL_GPUShader* vertex = nullptr;
    std::array<SDL_GPUShader*, 6> fragments{};
    std::map<unsigned, SDL_GPUGraphicsPipeline*> pipelines;
    std::array<Stream, kStreamingSlots> streams{};
    std::size_t streamIndex = 0;

    Impl(MetalSession& owner, std::uint64_t generation);

    ~Impl();

    void Release(Texture& texture);

    bool Upload(Texture& texture);

    bool CaptureBlur(Blur& blur, int x, int y, unsigned width, unsigned height, bool smoked);

    CseRadarResult Resolve(CseRadarTexture handle, Texture*& texture, bool allowNull = false);

    SDL_GPUTexture* Target(unsigned width, unsigned height, bool depth = false);

    SDL_GPUSampler* LinearSampler();

    SDL_GPUShader* Shader(const char* entry, bool isVertex, unsigned samplers = 1);

    SDL_GPUGraphicsPipeline* Pipeline(unsigned blend, SDL_GPUSampleCount samples, unsigned effect, bool bottom);

    CseRadarResult Draw(const std::vector<CseRadarPrimitiveVertex>& vertices, CseRadarHudSpace hud,
        const Uniforms& parameters, unsigned blend, const RasterState& raster, Texture* texture,
        unsigned effect = 0, const MetalEffectAccess* privateTarget = nullptr, Texture* lightmap = nullptr,
        SDL_GPUTexture* depthTarget = nullptr, bool clearColor = false, bool clearDepth = false, bool bottom = false);
};
}
}
