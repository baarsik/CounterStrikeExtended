#pragma once
#include <array>
#include <cstddef>
#include <string>

struct SDL_Window;

namespace cse {
namespace render {
class FrameRecorder;
class MetalSession;

#if defined(SW_RENDERER)
inline void ClearBackendSurfaceMeshes() {
}
inline bool UsingMetal() {
    return false;
}
#else
bool UsingMetal();
void ClearBackendSurfaceMeshes();
bool SubmitBackendQuad(const std::array<float, 4>& bounds, const std::array<float, 4>& uv);
bool SubmitBackendSurface(const void* identity, const float* vertices, std::size_t count, std::size_t stride,
    float scroll, bool detail);
// Resolve platform/build/command-line policy before creating any graphics window.
bool ConfigureBackend(bool glFlag, bool metalFlag, std::string& error, bool preferOpenGL = false);
constexpr unsigned kDefaultSceneSamples = 4;
constexpr unsigned kSingleSceneSample = 1;
bool InitializeBackend(SDL_Window* window, std::string& error, unsigned samples = kDefaultSceneSamples);
void ShutdownBackend();
void BeginBackendFrame(int width, int height);
void PresentBackendFrame(bool vsync, bool preserveAspect);
void FlushBackend(bool resolveIfAvailable = false);
bool BackendReadPixels(bool presented, int x, int y, int width, int height, unsigned components,
    unsigned alignment, void* pixels);
bool BackendUploadSucceeded();
void BackendDrawableSize(int& width, int& height);
// Captures use the retained presentation, which can predate a pending window resize.
bool BackendPresentedSize(int& width, int& height);
// Private engine services; callers must flush captured work before executing session operations.
FrameRecorder* BackendRecorder();
MetalSession* BackendMetalSession();
bool BackendSceneActive();
#endif
}
}
