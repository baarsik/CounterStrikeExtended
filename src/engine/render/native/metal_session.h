#pragma once

#include "frame_stream.h"
#include <memory>
#include <string>

struct SDL_Window;

namespace cse {
namespace render {

struct MetalEffectAccess;
class RadarRendererGpu;

class MetalSession {
public:
    MetalSession();
    ~MetalSession();
    MetalSession(const MetalSession&) = delete;
    MetalSession& operator=(const MetalSession&) = delete;
    bool Initialize(SDL_Window* window, bool debug, std::string& error);
    bool Resize(std::uint32_t width, std::uint32_t height, std::uint32_t samples, std::string& error);
    bool Render(const FrameSnapshot& frame, std::string& error);
    bool Present(bool vsync, bool preserveAspect, std::string& error);
    bool ReadPixels(int x, int y, int width, int height, unsigned components, unsigned alignment,
        void* pixels, std::string& error);
    // Last drawable-sized final color, including aspect bars; independent of swapchain image lifetime.
    bool ReadPresentedPixels(int x, int y, int width, int height, unsigned components, unsigned alignment,
        void* pixels, std::string& error);
    bool Finish(std::string& error);
    MetalEffectAccess EffectAccess() const;
    RadarRendererGpu& RadarEffects(std::uint64_t epoch);
    void Shutdown();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
}
