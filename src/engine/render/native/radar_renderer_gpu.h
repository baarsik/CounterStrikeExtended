#pragma once

#include "../../../public/radar_renderer_api.h"
#include "legacy_texture.h"
#include <memory>

namespace cse {
namespace ui { struct BackdropRect; struct BackdropStyle; }
namespace render {
class MetalSession;

// Engine-private effect owner. All calls run on the rendering thread after flushing the recorder.
class RadarRendererGpu {
public:
    RadarRendererGpu(MetalSession& session, std::uint64_t epoch);
    ~RadarRendererGpu();
    RadarRendererGpu(const RadarRendererGpu&) = delete;
    RadarRendererGpu& operator=(const RadarRendererGpu&) = delete;
    using PaletteProvider = std::shared_ptr<const SharedPalette> (*)(std::uint32_t);
    void SetTextureSource(const TextureStore* textures, const LegacyState* state, PaletteProvider palettes);
    enum class TextureRole { World, Sprite, Glyph };
    CseRadarTexture RegisterTexture(std::uint32_t identity, TextureRole role,
        std::shared_ptr<const TextureObject> object, std::shared_ptr<const SharedPalette> palette);
    CseRadarResult DrawRadarBackdrop(const CseRadarBackdropDraw& draw, const RasterState& raster);
    void BeginSmokedFrame();
    bool SmokedCaptureRequested() const;
    bool CaptureSmoked(int x, int y, int width, int height);
    bool DrawSmoked(const ui::BackdropRect& panel, const ui::BackdropRect& clip, int width, int height,
        const ui::BackdropStyle& style, float radius, const RasterState& raster);
    CseRadarResult DrawMaskRing(const CseRadarMaskRingDraw& draw, const RasterState& raster);
    CseRadarResult DrawMapTexture(const CseRadarMapTextureDraw& draw, const RasterState& raster);
    CseRadarResult BuildMapImage(const CseRadarMapImageBuild& build, CseRadarTexture& image);
    CseRadarResult ReleaseImage(CseRadarTexture image);
    CseRadarResult DrawPrimitiveQuads(const CseRadarPrimitiveBatch& batch, const RasterState& raster);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
}
