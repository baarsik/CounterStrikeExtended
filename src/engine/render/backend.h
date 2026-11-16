#pragma once
#include <string>

struct SDL_Window;

namespace cse {
namespace render {
class FrameRecorder;
class MetalSession;

#if defined(SW_RENDERER)
inline bool UsingMetal() {
    return false;
}
#else
bool UsingMetal();
// Resolve platform/build/command-line policy before creating any graphics window.
bool ConfigureBackend(bool glFlag, bool metalFlag, std::string& error);
constexpr unsigned kDefaultSceneSamples = 4;
constexpr unsigned kSingleSceneSample = 1;
bool InitializeBackend(SDL_Window* window, std::string& error, unsigned samples = kDefaultSceneSamples);
void ShutdownBackend();
void BeginBackendFrame(int width, int height);
void PresentBackendFrame(bool vsync, bool preserveAspect);
void FlushBackend();
bool BackendReadPixels(bool presented, int x, int y, int width, int height, unsigned components,
    unsigned alignment, void* pixels);
bool BackendUploadSucceeded();
void BackendDrawableSize(int& width, int& height);
// Private engine services; callers must flush captured work before executing session operations.
FrameRecorder* BackendRecorder();
MetalSession* BackendMetalSession();
#endif
}
}
