#include "backend.h"
#include "backend_selection.h"
#include "native/legacy_dispatch.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <memory>
#if defined(CSE_ENABLE_METAL)
#include "native/metal_session.h"
#include "native/surface_mesh_cache.h"
#endif

[[noreturn]] void Sys_Error(const char* format, ...);

namespace cse {
namespace render {
namespace {
Backend selected = Backend::OpenGL;
SDL_Window* backendWindow = nullptr;
bool sceneActive = false;
#if defined(CSE_ENABLE_METAL)
SurfaceMeshCache surfaceMeshes;
std::unique_ptr<FrameRecorder> recorder;
std::unique_ptr<MetalSession> session;
FrameInfo info;
unsigned sampleCount = 1;
bool recording = false;
std::string failure;

bool EnsureRecording(void*) {
    if (!failure.empty() || !recorder) {
        return false;
    }
    if (recording) {
        return recorder->CaptureStatus() == Status::Ok;
    }
    ++info.serial;
    if (recorder->BeginFrame(info) != Status::Ok) {
        failure = "Cannot begin native frame";
        return false;
    }
    recording = true;
    return true;
}

bool Submit(SceneOutput output = SceneOutput::Deferred) {
    if (!failure.empty()) {
        return false;
    }
    if (!recording) {
        return true;
    }
    FrameSnapshot frame;
    if (recorder->Seal(frame) != Status::Ok) {
        failure = LegacyDispatchDiagnostic();
        if (failure.empty()) {
            failure = "Cannot seal native frame";
        }
        return false;
    }
    recording = false;
    const bool rendered = session->Render(frame, failure, output);
    recorder->Recycle(std::move(frame));
    return rendered;
}

bool Finish(void*) {
    return Submit() && session->Finish(failure);
}

bool Read(void*, int x, int y, int width, int height, unsigned components, unsigned alignment, void* pixels) {
    return BackendReadPixels(LegacyNativeReadTarget() == ReadbackStage::PresentedColor,
        x, y, width, height, components, alignment, pixels);
}

void Require(bool success) {
    if (!success) {
        Sys_Error("Metal renderer: %s (%s)", failure.c_str(), LegacyDispatchDiagnostic().c_str());
    }
}
#endif
}

void ClearBackendSurfaceMeshes() {
#if defined(CSE_ENABLE_METAL)
    surfaceMeshes.Clear();
    ++info.surfaceMeshEpoch;
#endif
}

bool SubmitBackendQuad(const std::array<float, 4>& bounds, const std::array<float, 4>& uv) {
#if defined(CSE_ENABLE_METAL)
    // Display-list compilation keeps the compatible command path and its current-attribute semantics.
    if (UsingMetal() && recorder && recorder->AtFrameBoundary()) {
        std::array<Vertex, 4> vertices{};
        vertices[0].position = {{bounds[0], bounds[1], 0, 1}};
        vertices[1].position = {{bounds[2], bounds[1], 0, 1}};
        vertices[2].position = {{bounds[2], bounds[3], 0, 1}};
        vertices[3].position = {{bounds[0], bounds[3], 0, 1}};
        vertices[0].texcoords[0] = {{uv[0], uv[1], 0, 1}};
        vertices[1].texcoords[0] = {{uv[2], uv[1], 0, 1}};
        vertices[2].texcoords[0] = {{uv[2], uv[3], 0, 1}};
        vertices[3].texcoords[0] = {{uv[0], uv[3], 0, 1}};
        const auto status = SubmitNativeQuad(vertices);
        if (status != Status::Ok) {
            Sys_Error("Metal quad submission failed (status %u)", static_cast<unsigned>(status));
        }
        return true;
    }
#else
    (void)bounds;
    (void)uv;
#endif
    return false;
}

bool SubmitBackendSurface(const void* identity, const float* vertices, std::size_t count, std::size_t stride,
    float scroll, bool detail) {
#if defined(CSE_ENABLE_METAL)
    if (UsingMetal()) {
        try {
            auto mesh = surfaceMeshes.Get(identity, vertices, count, stride, failure);
            if (!mesh) {
                if (!failure.empty()) {
                    Sys_Error("Metal surface: %s", failure.c_str());
                }
                return false;
            }
            const auto status = SubmitNativeSurface(mesh, scroll, detail);
            if (status != Status::Ok) {
                Sys_Error("Metal surface submission failed (status %u)", static_cast<unsigned>(status));
            }
            return true;
        } catch (const std::bad_alloc&) {
            Sys_Error("Metal surface storage allocation failed");
        }
    }
#else
    (void)identity;
    (void)vertices;
    (void)count;
    (void)stride;
    (void)scroll;
    (void)detail;
#endif
    return false;
}

bool UsingMetal() {
    return selected == Backend::Metal;
}

bool ConfigureBackend(bool glFlag, bool metalFlag, std::string& error, bool preferOpenGL) {
#if defined(__APPLE__)
    constexpr bool kMacOS = true;
#else
    constexpr bool kMacOS = false;
#endif
#if defined(CSE_ENABLE_METAL)
    constexpr bool kMetalCompiled = true;
#else
    constexpr bool kMetalCompiled = false;
#endif
    const auto selection = SelectBackend(kMacOS, kMetalCompiled, glFlag, metalFlag, preferOpenGL);
    if (selection.error) {
        error = selection.error;
        return false;
    }
    selected = selection.backend;
    return true;
}

bool BackendSceneActive() {
    return UsingMetal() && sceneActive;
}

bool InitializeBackend(SDL_Window* window, std::string& error, unsigned samples) {
    backendWindow = window;
    sceneActive = false;
#if defined(CSE_ENABLE_METAL)
    if (UsingMetal()) {
        // Custom server model sets can retain over 650 MiB before the first world frame.
        // These CPU sources live until texture deletion, independently of the evictable GPU cache.
        constexpr int kBaselineSystemRamMiB = 8 * 1024;
        constexpr std::size_t kBaselineTextureStorageBytes = std::size_t{1024} * 1024 * 1024;
        constexpr std::size_t kLargerTextureStorageBytes = 2 * kBaselineTextureStorageBytes;
        FrameLimits limits;
        // SDL reports installed RAM in MiB; unknown values retain the smaller budget.
        limits.capture.textureStorageBytes = SDL_GetSystemRAM() > kBaselineSystemRamMiB ?
            kLargerTextureStorageBytes : kBaselineTextureStorageBytes;
        recorder = std::make_unique<FrameRecorder>(limits);
        session = std::make_unique<MetalSession>();
        failure.clear();
        recording = false;
        sampleCount = samples;
        int width = 0;
        int height = 0;
        SDL_GetWindowSizeInPixels(window, &width, &height);
        info = {0, 1, static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1};
#if CSE_METAL_DEBUG_DEVICE
        constexpr bool kDebugDevice = true;
#else
        constexpr bool kDebugDevice = false;
#endif
        if (!session->Initialize(window, kDebugDevice, error) ||
            !session->Resize(info.width, info.height, sampleCount, error)) {
            ShutdownBackend();
            return false;
        }
        LegacyNativeBridge bridge;
        bridge.ensureRecording = EnsureRecording;
        bridge.finish = Finish;
        bridge.readPixels = Read;
        if (!InstallNativeDispatch(recorder.get(), bridge, error)) {
            ShutdownBackend();
            return false;
        }
        std::fprintf(stderr, "Renderer: Metal (SDL_GPU metal); no OpenGL context\n");
    }
#else
    (void)error;
    (void)samples;
#endif
    return true;
}

void BeginBackendFrame(int width, int height) {
#if defined(CSE_ENABLE_METAL)
    if (UsingMetal()) {
        Require(Submit());
        if (width <= 0 || height <= 0) {
            return;
        }
        if (info.width != static_cast<unsigned>(width) || info.height != static_cast<unsigned>(height)) {
            ++info.displayEpoch;
            Require(session->Resize(static_cast<unsigned>(width), static_cast<unsigned>(height), sampleCount, failure));
            info.width = static_cast<unsigned>(width);
            info.height = static_cast<unsigned>(height);
        }
        Require(EnsureRecording(nullptr));
        sceneActive = true;
    }
#else
    (void)width;
    (void)height;
#endif
}

void FlushBackend() {
#if defined(CSE_ENABLE_METAL)
    if (UsingMetal()) {
        Require(Submit());
    }
#endif
}

void PresentBackendFrame(bool vsync, bool preserveAspect) {
#if defined(CSE_ENABLE_METAL)
    if (UsingMetal()) {
        Require(Submit(SceneOutput::Resolved));
        Require(session->Present(vsync, preserveAspect, failure));
        sceneActive = false;
    }
#else
    (void)vsync;
    (void)preserveAspect;
#endif
}

bool BackendReadPixels(bool presented, int x, int y, int width, int height, unsigned components,
    unsigned alignment, void* pixels) {
#if defined(CSE_ENABLE_METAL)
    if (UsingMetal()) {
        if (!Submit()) {
            return false;
        }
        const bool success = presented ?
            session->ReadPresentedPixels(x, y, width, height, components, alignment, pixels, failure) :
            session->ReadPixels(x, y, width, height, components, alignment, pixels, failure);
        if (!success) {
            std::fprintf(stderr, "Metal readback: %s\n", failure.c_str());
        }
        return success;
    }
#else
    (void)presented;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)components;
    (void)alignment;
    (void)pixels;
#endif
    return false;
}

bool BackendUploadSucceeded() {
#if defined(CSE_ENABLE_METAL)
    return UsingMetal() && failure.empty() && recorder && recorder->CaptureStatus() == Status::Ok &&
        LegacyDispatchStatus() == Status::Ok;
#else
    return false;
#endif
}

void BackendDrawableSize(int& width, int& height) {
    if (backendWindow) {
        SDL_GetWindowSizeInPixels(backendWindow, &width, &height);
    }
}

bool BackendPresentedSize(int& width, int& height) {
    width = 0;
    height = 0;
#if defined(CSE_ENABLE_METAL)
    return UsingMetal() && session && session->PresentedSize(width, height);
#else
    return false;
#endif
}

FrameRecorder* BackendRecorder() {
#if defined(CSE_ENABLE_METAL)
    return recorder.get();
#else
    return nullptr;
#endif
}

MetalSession* BackendMetalSession() {
#if defined(CSE_ENABLE_METAL)
    return session.get();
#else
    return nullptr;
#endif
}

void ShutdownBackend() {
    ClearBackendSurfaceMeshes();
#if defined(CSE_ENABLE_METAL)
    if (session) {
        // VGUI font retirement has already completed while dispatch/device were still alive.
        RemoveLegacyDispatch();
        session->Shutdown();
        session.reset();
        recorder.reset();
        recording = false;
    }
#endif
    backendWindow = nullptr;
    sceneActive = false;
}
}
}
