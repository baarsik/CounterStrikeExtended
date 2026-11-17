#pragma once

#include "frame_stream.h"
#include <memory>
#include <string>

struct SDL_Window;
struct SDL_GPUCommandBuffer;
struct SDL_GPUBufferBinding;
struct SDL_GPUBuffer;

namespace cse {
namespace render {

struct MetalEffectAccess;
class RadarRendererGpu;

// Opt-in CPU/submission diagnostics; no GPU synchronization is introduced by collection.
struct RenderFrameCounters {
    std::uint64_t drawPackets = 0;
    std::uint64_t gpuDraws = 0;
    std::uint64_t effectDraws = 0;
    std::uint64_t commandBuffers = 0;
    std::uint64_t renderPasses = 0;
    std::uint64_t sceneResolves = 0;
    std::uint64_t copyPasses = 0;
    std::uint64_t uploadBytes = 0;
    std::uint64_t fenceWaits = 0;
    double prepareMs = 0;
    double encodeMs = 0;
    double waitMs = 0;
    double presentMs = 0;
};

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
    bool PresentedSize(int& width, int& height) const;
    bool ReadPresentedPixels(int x, int y, int width, int height, unsigned components, unsigned alignment,
        void* pixels, std::string& error);
    bool Finish(std::string& error);
    // Engine-private ordered encoding context. SDL retains resources as they are encoded;
    // uploads use distinct frame-owned storage until the single submission fence retires.
    SDL_GPUCommandBuffer* Commands(std::string& error);
    SDL_GPUCommandBuffer* UploadCommands(std::size_t bytes, std::string& error);
    bool UploadEffectVertices(const void* vertices, std::size_t bytes, SDL_GPUBufferBinding& binding,
        std::string& error);
    SDL_GPUBuffer* CreateEffectMesh(const void* vertices, std::size_t bytes, std::string& error);
    bool ResolveScene(std::string& error);
    std::uint64_t EffectClock() const;
    void CountEffectDraw(std::uint64_t start, bool scene = true);
    void CountRenderPass();
    void CountCopyPass(std::size_t bytes = 0);
    void EnableDiagnostics(bool enabled);
    const RenderFrameCounters& Diagnostics() const;
    MetalEffectAccess EffectAccess() const;
    RadarRendererGpu& RadarEffects(std::uint64_t epoch);
    void Shutdown();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
}
