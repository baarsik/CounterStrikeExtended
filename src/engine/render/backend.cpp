#include "backend.h"
#include "backend_selection.h"
#include "native/legacy_dispatch.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <memory>
#if defined(CSE_ENABLE_METAL)
#include "native/metal_session.h"
#endif

[[noreturn]] void Sys_Error(const char* format, ...);

namespace cse {
namespace render {
namespace {
Backend selected = Backend::OpenGL;
SDL_Window* backendWindow = nullptr;
#if defined(CSE_ENABLE_METAL)
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

bool Submit() {
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
    return session->Render(frame, failure);
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

bool UsingMetal() {
    return selected == Backend::Metal;
}

bool ConfigureBackend(bool glFlag, bool metalFlag, std::string& error) {
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
    const auto selection = SelectBackend(kMacOS, kMetalCompiled, glFlag, metalFlag);
    if (selection.error) {
        error = selection.error;
        return false;
    }
    selected = selection.backend;
    return true;
}

bool InitializeBackend(SDL_Window* window, std::string& error, unsigned samples) {
    backendWindow = window;
#if defined(CSE_ENABLE_METAL)
    if (UsingMetal()) {
        recorder = std::make_unique<FrameRecorder>();
        session = std::make_unique<MetalSession>();
        failure.clear();
        recording = false;
        sampleCount = samples;
        int width = 0;
        int height = 0;
        SDL_GetWindowSizeInPixels(window, &width, &height);
        info = {0, 1, static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
#if defined(NDEBUG)
        constexpr bool kDebugDevice = false;
#else
        constexpr bool kDebugDevice = true;
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
        Require(Submit());
        Require(session->Present(vsync, preserveAspect, failure));
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
}
}
}
