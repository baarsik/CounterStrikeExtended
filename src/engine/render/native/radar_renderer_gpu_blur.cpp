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
        Release(blur.scene);
        Release(blur.filtered);
        Release(blur.scratch);
        blur = {};
        blur.scene.gpu = Target(width, height);
        blur.scene.sampler = LinearSampler();
        blur.filtered.gpu = Target(smallWidth, smallHeight);
        blur.filtered.sampler = LinearSampler();
        blur.scratch.gpu = Target(smallWidth, smallHeight);
        blur.scratch.sampler = LinearSampler();
        if (!blur.scene.gpu || !blur.scene.sampler || !blur.filtered.gpu || !blur.filtered.sampler ||
            !blur.scratch.gpu || !blur.scratch.sampler) {
            return false;
        }
        blur.width = width;
        blur.height = height;
    }
    blur.originX = x;
    blur.originY = y;
    auto* command = SDL_AcquireGPUCommandBuffer(device);
    if (!command) {
        return false;
    }
    SDL_GPUBlitInfo capture{};
    capture.source.texture = scene.color; // Render/effect passes resolve at identical scene dimensions.
    capture.source.x = static_cast<Uint32>(x);
    capture.source.y = scene.height - static_cast<Uint32>(y) - height;
    capture.source.w = width;
    capture.source.h = height;
    capture.destination.texture = blur.scene.gpu;
    capture.destination.w = width;
    capture.destination.h = height;
    capture.load_op = SDL_GPU_LOADOP_DONT_CARE;
    capture.filter = SDL_GPU_FILTER_NEAREST;
    SDL_BlitGPUTexture(command, &capture);
    SDL_GPUBlitInfo downsampleInfo{};
    downsampleInfo.source.texture = blur.scene.gpu;
    downsampleInfo.source.w = width;
    downsampleInfo.source.h = height;
    downsampleInfo.destination.texture = blur.filtered.gpu;
    downsampleInfo.destination.w = smallWidth;
    downsampleInfo.destination.h = smallHeight;
    downsampleInfo.load_op = SDL_GPU_LOADOP_DONT_CARE;
    downsampleInfo.filter = SDL_GPU_FILTER_LINEAR;
    SDL_BlitGPUTexture(command, &downsampleInfo);
    if (!SDL_SubmitGPUCommandBuffer(command)) {
        return false;
    }
    MetalEffectAccess target{device, blur.scratch.gpu, nullptr, smallWidth, smallHeight, SDL_GPU_SAMPLECOUNT_1};
    RasterState raster;
    raster.viewport = {{0, 0, static_cast<int>(smallWidth), static_cast<int>(smallHeight)}};
    Uniforms uniforms;
    uniforms.transform = {{2, -2, -1, 1}};
    uniforms.effects = {{1.0f / smallWidth, 0, smoked ? 1.0f : 0.0f, 0}};
    if (Draw(Quad(0, 0, 1, 1), {1, 1}, uniforms, 0, raster, &blur.filtered, 4, &target, nullptr,
        nullptr, true) != CSE_RADAR_OK) {
        return false;
    }
    target.color = blur.filtered.gpu;
    uniforms.effects[0] = 0;
    uniforms.effects[1] = 1.0f / smallHeight;
    return Draw(Quad(0, 0, 1, 1), {1, 1}, uniforms, 0, raster, &blur.scratch, 4, &target, nullptr,
        nullptr, true) == CSE_RADAR_OK;
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
    impl_->smokedCaptured = false;
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
    const float tint = 1.0f - ui::UnitValue(style.darkness);
    std::vector<CseRadarPrimitiveVertex> vertices;
    auto append = [&](float x0, float y0, float x1, float y1, float alpha0, float alpha1) {
        auto quad = Quad(x0, y0, x1 - x0, y1 - y0);
        for (auto& vertex : quad) {
            const float alpha = vertex.position[1] == y0 ? alpha0 : alpha1;
            vertex.color[0] = tint;
            vertex.color[1] = tint;
            vertex.color[2] = tint;
            vertex.color[3] = alpha;
            vertex.triangle[0] = vertex.position[0] / width;
            vertex.triangle[1] = vertex.position[1] / height;
            vertices.push_back(vertex);
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
    if (vertices.empty()) {
        return true;
    }
    Uniforms uniforms;
    uniforms.effectMode[0] = 5;
    auto mapping = raster;
    mapping.viewport = {{impl_->smokedBlur.originX, impl_->smokedBlur.originY,
        static_cast<int>(impl_->smokedBlur.width), static_cast<int>(impl_->smokedBlur.height)}};
    return impl_->Draw(vertices, {float(width), float(height)}, uniforms, 0, mapping, &impl_->smokedBlur.filtered, 5) ==
        CSE_RADAR_OK;
}

}
}
