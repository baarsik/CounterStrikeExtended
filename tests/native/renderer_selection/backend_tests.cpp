#include "backend.h"
#include "native/legacy_dispatch.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <type_traits>

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::cerr << __LINE__ << ": " #condition << "\n"; \
        std::abort(); \
    } \
} while (false)

// The engine supplies these exact slots; this fixture starts without a GL loader.
#define CORE_SLOTS(X) \
    X(AlphaFunc) X(Begin) X(BlendFunc) X(Clear) X(ClearColor) X(Color3f) X(Color4f) \
    X(Color4ub) X(Color4ubv) X(CullFace) X(DepthFunc) X(DepthMask) X(DepthRange) X(Disable) \
    X(Enable) X(End) X(Finish) X(Fogf) X(Fogfv) X(Fogi) X(Frustum) X(GetFloatv) X(GetIntegerv) \
    X(GetString) X(Hint) X(IsEnabled) X(LoadIdentity) X(MatrixMode) X(Ortho) X(PopMatrix) \
    X(PushMatrix) X(Rotatef) X(Scalef) X(ShadeModel) X(TexCoord2f) X(TexEnvf) X(TexEnvi) \
    X(TexParameterf) X(Translatef) X(Vertex2f) X(Vertex3f) X(Vertex3fv) X(Viewport)
#define DEFINE_SLOT(name) decltype(&gl##name) qgl##name = nullptr;
CORE_SLOTS(DEFINE_SLOT)
#undef DEFINE_SLOT
#define CHECK_SIGNATURE(name) \
    static_assert(std::is_same<decltype(&CseRender##name), decltype(&gl##name)>::value, "GL ABI: " #name);
CORE_SLOTS(CHECK_SIGNATURE)
#undef CHECK_SIGNATURE
void (APIENTRY* qglSelectTextureSGIS)(GLenum) = nullptr;
void (APIENTRY* qglMTexCoord2fSGIS)(GLenum, GLfloat, GLfloat) = nullptr;

#include "../render_dispatch/t2b_slots.inc"
#define NATIVE_SLOTS(X) X(ReadPixels) X(ReadBuffer) X(DrawBuffer) X(PolygonMode) X(FrontFace) \
    X(ClearDepth) X(ClearStencil) X(ColorMask) X(LoadMatrixf) X(MultMatrixf)
#define NATIVE_DEFINE_SLOT(name) decltype(&gl##name) qgl##name = nullptr;
NATIVE_SLOTS(NATIVE_DEFINE_SLOT)
#undef NATIVE_DEFINE_SLOT
#define NATIVE_CHECK_SIGNATURE(name) \
    static_assert(std::is_same<decltype(&CseRender##name), decltype(&gl##name)>::value, "Native GL ABI: " #name);
NATIVE_SLOTS(NATIVE_CHECK_SIGNATURE)
#undef NATIVE_CHECK_SIGNATURE
[[noreturn]] void Sys_Error(const char*, ...) {
    throw std::runtime_error("backend failed");
}
int main() {
    using namespace cse::render;
    std::string error;
    CHECK(SDL_Init(SDL_INIT_VIDEO));
    auto* window = SDL_CreateWindow("backend lifecycle", 32, 32, 0);
    CHECK(window);
    CHECK(ConfigureBackend(false, false, error));
    CHECK(UsingMetal());
    CHECK(InitializeBackend(window, error));
    CHECK(!BackendSceneActive());
    CHECK(!SDL_GL_GetCurrentContext());
    // Startup uploads/state happen before the first ordinary frame.
    GLuint texture = 0;
    CseRenderGenTextures(1, &texture);
    CHECK(texture);
    CseRenderBindTexture(GL_TEXTURE_2D, texture);
    const std::array<unsigned char, 4> texel{{255, 255, 255, 255}};
    CseRenderTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, texel.data());
    CHECK(BackendUploadSucceeded());
    BeginBackendFrame(32, 32);
    CHECK(BackendSceneActive());
    CseRenderClearColor(1, 0, 0, 1);
    CseRenderClear(GL_COLOR_BUFFER_BIT);
    FlushBackend();
    CHECK(BackendSceneActive());
    // Flush leaves the session reusable without discarding current state.
    CseRenderClearColor(0, 1, 0, 1);
    CseRenderClear(GL_COLOR_BUFFER_BIT);
    PresentBackendFrame(false, true);
    CHECK(!BackendSceneActive());
    std::array<unsigned char, 4> pixel{};
    CHECK(BackendReadPixels(true, 8, 8, 1, 1, 4, 1, pixel.data()));
    CHECK(pixel[0] == 0 && pixel[1] == 255 && pixel[3] == 255);
    CseRenderReadBuffer(GL_FRONT);
    CseRenderPixelStorei(GL_PACK_ALIGNMENT, 4);
    pixel[3] = 77;
    CseRenderReadPixels(8, 8, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel.data());
    CHECK(pixel[0] == 0 && pixel[1] == 255 && pixel[3] == 77);
    BeginBackendFrame(16, 16);
    CseRenderClearColor(0, 0, 1, 1);
    CseRenderClear(GL_COLOR_BUFFER_BIT);
    CHECK(BackendReadPixels(false, 8, 8, 1, 1, 4, 1, pixel.data()));
    CHECK(pixel[2] == 255);
    CHECK(SDL_SetWindowSize(window, 48, 32));
    CHECK(SDL_SyncWindow(window));
    SDL_PumpEvents();
    PresentBackendFrame(false, false);
    CHECK(BackendReadPixels(true, 47, 31, 1, 1, 4, 1, pixel.data()));
    CHECK(pixel[2] == 255);
    CHECK(SDL_MinimizeWindow(window));
    CHECK(SDL_SyncWindow(window));
    SDL_PumpEvents();
    PresentBackendFrame(false, true);
    CHECK(SDL_RestoreWindow(window));
    CHECK(SDL_SyncWindow(window));
    SDL_PumpEvents();
    PresentBackendFrame(false, false);
    CHECK(BackendReadPixels(true, 47, 31, 1, 1, 4, 1, pixel.data()));
    CHECK(pixel[2] == 255);
    CseRenderDeleteTextures(1, &texture);
    // Re-entering the frame lifecycle cannot discard a failed primitive to recover silently.
    CseRenderBegin(GL_TRIANGLES);
    CseRenderBegin(GL_TRIANGLES);
    CHECK(!BackendUploadSucceeded());
    bool failurePreserved = false;
    try {
        BeginBackendFrame(16, 16);
    } catch (const std::runtime_error&) {
        failurePreserved = true;
    }
    CHECK(failurePreserved);
    ShutdownBackend();
    SDL_DestroyWindow(window);
    SDL_Quit();
    std::cout << "backend startup, flush, presentation, between-frame capture, resize, teardown passed\n";
}
