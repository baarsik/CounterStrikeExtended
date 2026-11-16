#include "backend.h"
#include "native/legacy_dispatch.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>

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

namespace {
constexpr int kBaselineSystemRamMiB = 8 * 1024;
int systemRamMiB = kBaselineSystemRamMiB;
}

// Supply installed RAM independently of the host so both runtime budgets are exercised.
extern "C" int SDLCALL SDL_GetSystemRAM(void) {
    return systemRamMiB;
}

void RuntimeTextureBudgets(SDL_Window* window) {
    using namespace cse::render;
    constexpr std::size_t kGiB = std::size_t{1024} * 1024 * 1024;
    struct Case {
        int ramMiB;
        std::size_t textureBytes;
    };
    // Catch an inclusive threshold, a 16 GiB cutoff, incorrect RAM units and unknown-RAM handling.
    const std::array<Case, 9> cases{{
        {-1, kGiB}, {0, kGiB}, {4096, kGiB}, {8191, kGiB}, {8192, kGiB},
        {8193, 2 * kGiB}, {12288, 2 * kGiB}, {16384, 2 * kGiB},
        {std::numeric_limits<int>::max(), 2 * kGiB}
    }};
    for (const auto& test : cases) {
        systemRamMiB = test.ramMiB;
        std::string error;
        CHECK(InitializeBackend(window, error));
        CHECK(BackendRecorder());
        CHECK(BackendRecorder()->Limits().capture.textureStorageBytes == test.textureBytes);
        ShutdownBackend();
    }
}

void LargeTexturePrecache() {
    using namespace cse::render;
    constexpr GLsizei kTextureDimension = 1024;
    constexpr std::size_t kRgbaComponents = 4;
    // A 768 MiB precache covers custom model sets that already exceed 650 MiB before drawing.
    constexpr std::size_t kTextureCount = 192;
    constexpr unsigned char kFirstColor = 127;
    constexpr unsigned char kLastColor = 191;
    std::array<GLuint, kTextureCount> textures{};
    std::vector<unsigned char> pixels(kTextureDimension * kTextureDimension * kRgbaComponents, kFirstColor);
    CseRenderGenTextures(static_cast<GLsizei>(textures.size()), textures.data());
    for (std::size_t index = 0; index < textures.size(); ++index) {
        CHECK(textures[index]);
        if (index == textures.size() - 1) {
            std::fill(pixels.begin(), pixels.end(), kLastColor);
        }
        CseRenderBindTexture(GL_TEXTURE_2D, textures[index]);
        CseRenderTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        CseRenderTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        CseRenderTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kTextureDimension, kTextureDimension, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        CHECK(BackendUploadSucceeded());
    }
    // Precache must keep both early and late textures usable when they are first drawn.
    constexpr int kFrameDimension = 32;
    const std::array<std::size_t, 2> sampled{{0, textures.size() - 1}};
    for (const auto index : sampled) {
        BeginBackendFrame(kFrameDimension, kFrameDimension);
        CseRenderViewport(0, 0, kFrameDimension, kFrameDimension);
        CseRenderEnable(GL_TEXTURE_2D);
        CseRenderTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        CseRenderBindTexture(GL_TEXTURE_2D, textures[index]);
        CseRenderBegin(GL_QUADS);
        CseRenderVertex2f(-1, -1);
        CseRenderVertex2f(1, -1);
        CseRenderVertex2f(1, 1);
        CseRenderVertex2f(-1, 1);
        CseRenderEnd();
        std::array<unsigned char, kRgbaComponents> pixel{};
        CHECK(BackendReadPixels(false, kFrameDimension / 2, kFrameDimension / 2, 1, 1,
            kRgbaComponents, 1, pixel.data()));
        const auto expected = index == 0 ? kFirstColor : kLastColor;
        for (const auto component : pixel) {
            CHECK(component == expected);
        }
    }
    CseRenderDeleteTextures(static_cast<GLsizei>(textures.size()), textures.data());
    CHECK(BackendUploadSucceeded());
}

int main(int argc, char** argv) {
    using namespace cse::render;
    std::string error;
    CHECK(SDL_Init(SDL_INIT_VIDEO));
    auto* window = SDL_CreateWindow("backend lifecycle", 32, 32, 0);
    CHECK(window);
    CHECK(ConfigureBackend(false, false, error));
    CHECK(UsingMetal());
    if (argc == 2 && std::string(argv[1]) == "ram-budget") {
        RuntimeTextureBudgets(window);
        SDL_DestroyWindow(window);
        SDL_Quit();
        std::cout << "runtime texture budgets at the installed-RAM threshold passed\n";
        return 0;
    }
    CHECK(InitializeBackend(window, error));
    CHECK(!BackendSceneActive());
    int absentWidth = 1;
    int absentHeight = 1;
    CHECK(!BackendPresentedSize(absentWidth, absentHeight));
    CHECK(absentWidth == 0 && absentHeight == 0);
    CHECK(!SDL_GL_GetCurrentContext());
    if (argc == 2 && std::string(argv[1]) == "large-precache") {
        LargeTexturePrecache();
        ShutdownBackend();
        SDL_DestroyWindow(window);
        SDL_Quit();
        std::cout << "large texture precache and first-use rendering passed\n";
        return 0;
    }
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
    // A window event can precede the next frame. Screenshot reads the retained prior presentation.
    int captureWidth = 0;
    int captureHeight = 0;
    CHECK(BackendPresentedSize(captureWidth, captureHeight));
    std::vector<unsigned char> capture(captureWidth * captureHeight * 4);
    CHECK(BackendReadPixels(true, 0, 0, captureWidth, captureHeight, 4, 1, capture.data()));
    CHECK(captureWidth == 32 && captureHeight == 32);
    CHECK(capture[0] == 0 && capture[1] == 255 && capture[3] == 255);
    BeginBackendFrame(16, 16);
    CHECK(BackendUploadSucceeded());
    PresentBackendFrame(false, false);
    CHECK(BackendPresentedSize(captureWidth, captureHeight));
    CHECK(captureWidth == 48 && captureHeight == 32);
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
    CHECK(BackendPresentedSize(captureWidth, captureHeight));
    CHECK(captureWidth == 48 && captureHeight == 32);
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
