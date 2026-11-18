#include "vgui_background_blur_metal.h"
#include "render/backend.h"
#include "render/native/metal_session.h"
#include "render/native/metal_effect_access.h"
#include "render/native/radar_renderer_gpu.h"
#include "render/native/frame_stream.h"
namespace cse::radar {
std::uint64_t GraphicsGeneration();
}
namespace cse::ui {
void BeginMetalBackgroundFrame() {
    if (auto* session = render::BackendMetalSession()) {
        session->RadarEffects(radar::GraphicsGeneration()).BeginSmokedFrame();
    }
}
void CaptureMetalBackground(int x, int y, int width, int height) {
    if (auto* session = render::BackendMetalSession()) {
        auto& effects = session->RadarEffects(radar::GraphicsGeneration());
        if (effects.SmokedCaptureRequested() && width > 0 && height > 0) {
            const auto scene = session->EffectAccess();
            const bool validSceneRect = scene.color && x >= 0 && y >= 0 &&
                static_cast<std::uint64_t>(x) + width <= scene.width &&
                static_cast<std::uint64_t>(y) + height <= scene.height;
            render::FlushBackend(validSceneRect);
        }
        effects.CaptureSmoked(x, y, width, height);
    }
}
bool DrawMetalBackground(const BackdropRect& panel, const BackdropRect& clip, int width, int height,
    const BackdropStyle& style, float radius) {
    auto* session = render::BackendMetalSession();
    if (!session || !render::BackendRecorder()) {
        return false;
    }
    render::FlushBackend();
    return session->RadarEffects(radar::GraphicsGeneration()).DrawSmoked(panel, clip, width, height, style, radius,
        render::BackendRecorder()->State().raster);
}
}
