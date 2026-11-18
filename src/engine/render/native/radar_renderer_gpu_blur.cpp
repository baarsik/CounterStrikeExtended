#include "../../../public/vgui_smoked_glass.h"
#include "radar_renderer_gpu_internal.h"
namespace cse {
namespace render {
bool RadarRendererGpu::Impl::CaptureBlur(Blur& blur, int x, int y, unsigned width, unsigned height, bool smoked) {
    const auto scene = session.EffectAccess();
    if (!scene.color || x < 0 || y < 0 || !width || !height || std::uint64_t(x) + width > scene.width ||
        std::uint64_t(y) + height > scene.height) {
        return false;
    }
    const unsigned downsample = smoked ? 4 : 2;
    const unsigned smallWidth = std::max(1u, (width + (smoked ? downsample - 1 : 0)) / downsample);
    const unsigned smallHeight = std::max(1u, (height + (smoked ? downsample - 1 : 0)) / downsample);
    if (blur.width != width || blur.height != height) {
        Release(blur.filtered);
        Release(blur.scratch);
        blur = {};
        blur.filtered.gpu = Target(smallWidth, smallHeight);
        blur.filtered.sampler = LinearSampler();
        blur.scratch.gpu = Target(smallWidth, smallHeight);
        blur.scratch.sampler = LinearSampler();
        if (!blur.filtered.gpu || !blur.filtered.sampler ||
            !blur.scratch.gpu || !blur.scratch.sampler) {
            return false;
        }
        blur.width = width;
        blur.height = height;
    }
    blur.originX = x;
    blur.originY = y;
    std::string error;
    if (!session.ResolveScene(error)) {
        return false;
    }
    auto* command = session.Commands(error);
    if (!command) {
        return false;
    }
    SDL_GPUBlitInfo capture{};
    // ResolveScene may select a different texture to preserve the previous presentation.
    capture.source.texture = session.EffectAccess().color;
    capture.source.x = static_cast<Uint32>(x);
    capture.source.y = scene.height - static_cast<Uint32>(y) - height;
    capture.source.w = width;
    capture.source.h = height;
    capture.destination.texture = blur.filtered.gpu;
    capture.destination.w = smallWidth;
    capture.destination.h = smallHeight;
    capture.load_op = SDL_GPU_LOADOP_DONT_CARE;
    // The destination is no larger than the source rectangle. Each linear sample
    // remains within its texel centers, including the two edges.
    capture.filter = SDL_GPU_FILTER_LINEAR;
    SDL_BlitGPUTexture(command, &capture);
    session.CountRenderPass();
    static const auto quad = Quad(0, 0, 1, 1);
    if (!filterVertices) {
        // Both passes use the same normalized geometry. Draw can still stream it
        // if allocating the small retained buffer fails.
        filterVertices = session.CreateEffectMesh(quad.data(), quad.size() * sizeof(quad[0]), error);
    }
    MetalEffectAccess target{device, blur.scratch.gpu, nullptr, smallWidth, smallHeight, SDL_GPU_SAMPLECOUNT_1};
    RasterState raster;
    raster.viewport = {{0, 0, static_cast<int>(smallWidth), static_cast<int>(smallHeight)}};
    Uniforms uniforms;
    uniforms.transform = {{2, -2, -1, 1}};
    uniforms.effects = {{1.0f / smallWidth, 0, smoked ? 1.0f : 0.0f, 0}};
    if (Draw(quad, {1, 1}, uniforms, 0, raster, &blur.filtered, 4, &target, nullptr,
        nullptr, true, false, false, filterVertices) != CSE_RADAR_OK) {
        return false;
    }
    target.color = blur.filtered.gpu;
    uniforms.effects[0] = 0;
    uniforms.effects[1] = 1.0f / smallHeight;
    return Draw(quad, {1, 1}, uniforms, 0, raster, &blur.scratch, 4, &target, nullptr,
        nullptr, true, false, false, filterVertices) == CSE_RADAR_OK;
}

CseRadarResult RadarRendererGpu::DrawRadarBackdrop(const CseRadarBackdropDraw& draw, const RasterState& raster) {
    const float values[] = {draw.x, draw.y, draw.extent, draw.darken, draw.opacity};
    if (draw.size < sizeof(draw) || draw.reserved || !ValidHud(draw.hud) || !ValidFade(draw.fade) ||
        !Finite(values, 5) || draw.extent <= 0 || draw.darken < 0 || draw.darken > 1 ||
        draw.opacity < 0 || draw.opacity > 1 ||
        (draw.shape != CSE_RADAR_SHAPE_CIRCLE && draw.shape != CSE_RADAR_SHAPE_SQUARE)) {
        return CSE_RADAR_INVALID_ARGUMENT;
    }
    if (draw.rendererEpoch != impl_->epoch) {
        return CSE_RADAR_STALE_EPOCH;
    }
    if (!impl_->session.EffectAccess().color) {
        return CSE_RADAR_NO_DRAW_TARGET;
    }
    constexpr int kMinimumCapture = 4;
    if (draw.extent < kMinimumCapture) {
        return CSE_RADAR_UNAVAILABLE;
    }
    const auto& viewport = raster.viewport;
    const double scaleX = viewport[2] / double(draw.hud.width);
    const double scaleY = viewport[3] / double(draw.hud.height);
    const auto boundedInteger = [](double value) {
        constexpr double kCoordinateLimit = 1000000000;
        return static_cast<int>(std::max(-kCoordinateLimit, std::min(kCoordinateLimit, value)));
    };
    int x = viewport[0] + boundedInteger(draw.x * scaleX + 0.5);
    int y = viewport[1] + viewport[3] - boundedInteger((draw.y + double(draw.extent)) * scaleY + 0.5);
    int width = boundedInteger(draw.extent * scaleX + 0.5);
    int height = boundedInteger(draw.extent * scaleY + 0.5);
    x = std::max(x, viewport[0]);
    y = std::max(y, viewport[1]);
    width = std::min(std::max(width, kMinimumCapture), viewport[0] + viewport[2] - x);
    height = std::min(std::max(height, kMinimumCapture), viewport[1] + viewport[3] - y);
    if (width <= 0 || height <= 0) {
        return CSE_RADAR_UNAVAILABLE;
    }
    if (!impl_->CaptureBlur(impl_->radarBlur, x, y, static_cast<unsigned>(width), static_cast<unsigned>(height), false)) {
        return CSE_RADAR_DEVICE_LOST;
    }
    Uniforms uniforms;
    uniforms.clip = {{draw.x + draw.extent * 0.5f, draw.y + draw.extent * 0.5f, draw.extent * 0.5f,
        draw.shape == CSE_RADAR_SHAPE_SQUARE ? 1.0f : 0.0f}};
    uniforms.color = {{draw.darken, draw.darken, draw.darken, draw.opacity}};
    CopyFade(uniforms, draw.fade);
    InheritAlpha(uniforms, raster);
    return impl_->Draw(Quad(draw.x, draw.y, draw.extent, draw.extent), draw.hud, uniforms, 0, raster,
        &impl_->radarBlur.filtered, 5);
}

void RadarRendererGpu::BeginSmokedFrame() {
    impl_->RetireBorrowedTextures();
    impl_->smokedCaptured = false;
}

bool RadarRendererGpu::SmokedCaptureRequested() const {
    return impl_->smokedRequested;
}

bool RadarRendererGpu::CaptureSmoked(int x, int y, int width, int height) {
    const bool requested = impl_->smokedRequested;
    impl_->smokedRequested = false;
    impl_->smokedCaptured = false;
    if (!requested || width <= 0 || height <= 0) {
        return false;
    }
    impl_->smokedCaptured = impl_->CaptureBlur(impl_->smokedBlur, x, y, static_cast<unsigned>(width),
        static_cast<unsigned>(height), true);
    const auto access = impl_->session.EffectAccess();
    impl_->capturedSceneWidth = access.width;
    impl_->capturedSceneHeight = access.height;
    return impl_->smokedCaptured;
}

bool RadarRendererGpu::DrawSmoked(const ui::BackdropRect& panel, const ui::BackdropRect& clip, int width, int height,
    const ui::BackdropStyle& style, float radius, const RasterState& raster) {
    impl_->smokedRequested = true;
    const auto access = impl_->session.EffectAccess();
    if (!impl_->smokedCaptured || impl_->capturedSceneWidth != access.width || impl_->capturedSceneHeight != access.height ||
        panel.width <= 0 || panel.height <= 0 || width <= 0 || height <= 0) {
        return false;
    }
    using Coordinate = std::int64_t;
    const auto left = std::max<Coordinate>({0, panel.x, clip.x});
    const auto top = std::max<Coordinate>({0, panel.y, clip.y});
    const auto right = std::min<Coordinate>({width, Coordinate(panel.x) + panel.width, Coordinate(clip.x) + clip.width});
    const auto bottom = std::min<Coordinate>({height, Coordinate(panel.y) + panel.height, Coordinate(clip.y) + clip.height});
    if (left >= right || top >= bottom) {
        return true;
    }
    const auto floatBits = [](float value) {
        std::uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    };
    const std::array<std::uint32_t, 14> key{{static_cast<std::uint32_t>(panel.x),
        static_cast<std::uint32_t>(panel.y), static_cast<std::uint32_t>(panel.width),
        static_cast<std::uint32_t>(panel.height), static_cast<std::uint32_t>(clip.x),
        static_cast<std::uint32_t>(clip.y), static_cast<std::uint32_t>(clip.width),
        static_cast<std::uint32_t>(clip.height), static_cast<std::uint32_t>(width),
        static_cast<std::uint32_t>(height), floatBits(ui::UnitValue(style.opacityTop)),
        floatBits(ui::UnitValue(style.opacityBottom)), floatBits(ui::UnitValue(style.darkness)),
        floatBits(std::isfinite(radius) && radius > 0 ? radius : 0)}};
    auto found = impl_->panels.find(key);
    if (found == impl_->panels.end()) {
        constexpr std::size_t kMaximumPanelCacheBytes = 8 * 1024 * 1024;
        constexpr std::size_t kMaximumCachedPanels = 64;
        if (impl_->panelBytes >= kMaximumPanelCacheBytes || impl_->panels.size() >= kMaximumCachedPanels) {
            impl_->ClearPanels();
        }
        Impl::PanelMesh cached;
        const float tint = 1.0f - ui::UnitValue(style.darkness);
        auto& vertices = cached.vertices;
        auto append = [&](float x0, float y0, float x1, float y1, float alpha0, float alpha1) {
            std::array<CseRadarPrimitiveVertex, 4> corners{};
            constexpr float kCoordinates[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
            for (std::size_t index = 0; index < corners.size(); ++index) {
                auto& vertex = corners[index];
                vertex.position[0] = x0 + kCoordinates[index][0] * (x1 - x0);
                vertex.position[1] = y0 + kCoordinates[index][1] * (y1 - y0);
                const float alpha = vertex.position[1] == y0 ? alpha0 : alpha1;
                vertex.color[0] = tint;
                vertex.color[1] = tint;
                vertex.color[2] = tint;
                vertex.color[3] = alpha;
                vertex.triangle[0] = vertex.position[0] / width;
                vertex.triangle[1] = vertex.position[1] / height;
            }
            for (auto index : kQuadTriangles) {
                vertices.push_back(corners[index]);
            }
        };
        if (std::isfinite(radius) && radius > 0) {
            for (auto row = top; row < bottom; ++row) {
                const float localY = static_cast<float>(row - panel.y) + 0.5f;
                const float inset = ui::RoundedInset(float(panel.width), float(panel.height), radius, localY);
                const float x0 = std::max(float(left), panel.x + inset);
                const float x1 = std::min(float(right), panel.x + panel.width - inset);
                if (x0 < x1) {
                    const float alpha = ui::BackdropOpacity(style, localY / panel.height);
                    append(x0, float(row), x1, float(row + 1), alpha, alpha);
                }
            }
        } else {
            constexpr int kGradientStrips = 32;
            for (int strip = 0; strip < kGradientStrips; ++strip) {
                const float y0 = float(top) + float(bottom - top) * strip / kGradientStrips;
                const float y1 = float(top) + float(bottom - top) * (strip + 1) / kGradientStrips;
                append(float(left), y0, float(right), y1, ui::BackdropOpacity(style, (y0 - panel.y) / panel.height),
                    ui::BackdropOpacity(style, (y1 - panel.y) / panel.height));
            }
        }
        if (!cached.vertices.empty()) {
            std::string error;
            cached.gpu = impl_->session.CreateEffectMesh(cached.vertices.data(),
                cached.vertices.size() * sizeof(cached.vertices[0]), error);
            if (!cached.gpu) {
                return false;
            }
        }
        impl_->panelBytes += cached.vertices.size() * sizeof(cached.vertices[0]);
        found = impl_->panels.emplace(key, std::move(cached)).first;
    }
    const auto& cached = found->second;
    if (cached.vertices.empty()) {
        return true;
    }
    Uniforms uniforms;
    uniforms.effectMode[0] = 5;
    auto mapping = raster;
    mapping.viewport = {{impl_->smokedBlur.originX, impl_->smokedBlur.originY,
        static_cast<int>(impl_->smokedBlur.width), static_cast<int>(impl_->smokedBlur.height)}};
    return impl_->Draw(cached.vertices, {float(width), float(height)}, uniforms, 0, mapping,
        &impl_->smokedBlur.filtered, 5, nullptr, nullptr, nullptr, false, false, false, cached.gpu) ==
        CSE_RADAR_OK;
}

}
}
