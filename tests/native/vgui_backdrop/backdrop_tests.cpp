#define GL_SILENCE_DEPRECATION
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include "vgui_background_blur_gl.h"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr int Width = 128;
constexpr int Height = 96;
constexpr int Channels = 4;

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void Scene(float alpha = 1.0f) {
    glViewport(0, 0, Width, Height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, Width, Height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glBegin(GL_QUADS);
    for (int x = 0; x < Width; ++x) {
        const float value = x % 2 ? 1.0f : 0.0f;
        glColor4f(value, value, value, alpha);
        glVertex2i(x, 0);
        glVertex2i(x + 1, 0);
        glVertex2i(x + 1, Height);
        glVertex2i(x, Height);
    }
    glEnd();
}

std::vector<unsigned char> Pixels() {
    std::vector<unsigned char> pixels(Width * Height * Channels);
    glReadPixels(0, 0, Width, Height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    return pixels;
}

int Red(const std::vector<unsigned char>& pixels, int x, int y) {
    return pixels[((Height - 1 - y) * Width + x) * Channels];
}

void Exercise() {
    using cse::ui::BackgroundBlur;
    using cse::ui::BackdropRect;
    const BackdropRect screen{0, 0, Width, Height};
    BackgroundBlur blur;
    Scene();
    blur.BeginFrame();
    Require(!blur.Capture(0, 0, Width, Height), "Idle frames must not capture the scene");
    Require(!blur.Draw(screen, screen, Width, Height, {1, 0, 0.4f}), "First demand needs a fresh scene");
    blur.BeginFrame();
    const auto blendEquation = reinterpret_cast<PFNGLBLENDEQUATIONPROC>(SDL_GL_GetProcAddress("glBlendEquation"));
    Require(blendEquation != nullptr, "GL blend equation entry point required");
    blendEquation(GL_FUNC_REVERSE_SUBTRACT);
    Require(blur.Capture(0, 0, Width, Height), "Requested frame must capture and blur");
    GLint restoredEquation = 0;
    glGetIntegerv(GL_BLEND_EQUATION, &restoredEquation);
    Require(restoredEquation == GL_FUNC_REVERSE_SUBTRACT, "Capture must restore blend equation");

    // A foreground marker added after capture represents text/HUD; blur must sample the earlier scene.
    glEnable(GL_SCISSOR_TEST);
    glScissor(50, Height - 12, 6, 6);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    const auto before = Pixels();
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.9f);
    glEnable(GL_SCISSOR_TEST);
    glScissor(1, 2, 3, 4);
    std::array<GLint, 4> oldScissor{};
    glGetIntegerv(GL_SCISSOR_BOX, oldScissor.data());
    Require(blur.Draw(screen, screen, Width, Height, {1, 0, 0.4f}), "Captured backdrop must draw");
    std::array<GLint, 4> newScissor{};
    glGetIntegerv(GL_SCISSOR_BOX, newScissor.data());
    Require(oldScissor == newScissor && glIsEnabled(GL_SCISSOR_TEST), "Draw must restore scissor state");
    Require(glIsEnabled(GL_ALPHA_TEST), "Draw must restore alpha testing");
    glGetIntegerv(GL_BLEND_EQUATION, &restoredEquation);
    Require(restoredEquation == GL_FUNC_REVERSE_SUBTRACT, "Draw must restore blend equation");
    blendEquation(GL_FUNC_ADD);
    const auto result = Pixels();
    Require(Red(result, 62, 0) > 45 && Red(result, 62, 0) < 110, "Opaque edge must blur and darken black stripes");
    Require(Red(result, 63, 0) < 120, "Opaque edge must blur and darken white stripes");
    Require(Red(result, 62, Height - 1) == Red(before, 62, Height - 1), "Transparent edge must stay sharp");
    const int green = result[((Height - 1 - 8) * Width + 52) * Channels + 1];
    Require(green > 30, "Backdrop must sample pre-HUD scene, not the red foreground marker");
    Require(Red(result, 62, Height / 2) > 15 && Red(result, 62, Height / 2) < 70,
        "Matching gradient must interpolate blurred and sharp scene");

    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_ALPHA_TEST);
    Scene();
    const auto unclipped = Pixels();
    Require(blur.Draw(screen, {20, 20, 30, 30}, Width, Height, {1, 0, 0.4f}), "Clipped draw must succeed");
    const auto clipped = Pixels();
    Require(Red(clipped, 10, 10) == Red(unclipped, 10, 10), "Parent clip must preserve outside pixels");
    Require(Red(clipped, 22, 22) > 25, "Parent clip must retain original gradient coordinates");
    Require(!blur.Draw({0, 0, 0, 1}, screen, Width, Height, {1, 0, 0.4f}), "Empty panels must be rejected");

    // Foreground painted after the backdrop stays exact.
    glEnable(GL_SCISSOR_TEST);
    glScissor(60, 60, 2, 2);
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    const auto foreground = Pixels();
    Require(Red(foreground, 60, Height - 61) == 255, "Foreground must stay sharp");
    glDisable(GL_SCISSOR_TEST);
    blur.BeginFrame();
    Require(blur.Capture(0, 0, Width / 2, Height / 2), "Resize must recreate framebuffer resources");
    blur.Shutdown();
    blur.BeginFrame();
    Require(!blur.Draw(screen, screen, Width, Height, {1, 0, 0.4f}), "Shutdown must invalidate captured image");
    blur.BeginFrame();
    Require(blur.Capture(0, 0, Width, Height), "Shutdown must permit reuse in a new context lifecycle");
    blur.Shutdown();
    Require(glGetError() == GL_NO_ERROR, "Backdrop must not generate GL errors");
}

void ExerciseFramebuffer(bool multisampled) {
    const auto genFramebuffer = reinterpret_cast<PFNGLGENFRAMEBUFFERSPROC>(
        SDL_GL_GetProcAddress("glGenFramebuffersEXT"));
    const auto bindFramebuffer = reinterpret_cast<PFNGLBINDFRAMEBUFFERPROC>(
        SDL_GL_GetProcAddress("glBindFramebufferEXT"));
    const auto deleteFramebuffer = reinterpret_cast<PFNGLDELETEFRAMEBUFFERSPROC>(
        SDL_GL_GetProcAddress("glDeleteFramebuffersEXT"));
    const auto genRenderbuffer = reinterpret_cast<PFNGLGENRENDERBUFFERSPROC>(
        SDL_GL_GetProcAddress("glGenRenderbuffersEXT"));
    const auto bindRenderbuffer = reinterpret_cast<PFNGLBINDRENDERBUFFERPROC>(
        SDL_GL_GetProcAddress("glBindRenderbufferEXT"));
    const auto storage = reinterpret_cast<PFNGLRENDERBUFFERSTORAGEPROC>(
        SDL_GL_GetProcAddress("glRenderbufferStorageEXT"));
    const auto storageMultisample = reinterpret_cast<PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC>(
        SDL_GL_GetProcAddress("glRenderbufferStorageMultisampleEXT"));
    const auto attach = reinterpret_cast<PFNGLFRAMEBUFFERRENDERBUFFERPROC>(
        SDL_GL_GetProcAddress("glFramebufferRenderbufferEXT"));
    const auto deleteRenderbuffer = reinterpret_cast<PFNGLDELETERENDERBUFFERSPROC>(
        SDL_GL_GetProcAddress("glDeleteRenderbuffersEXT"));
    const auto blit = reinterpret_cast<PFNGLBLITFRAMEBUFFERPROC>(SDL_GL_GetProcAddress("glBlitFramebufferEXT"));
    Require(genFramebuffer && bindFramebuffer && deleteFramebuffer && genRenderbuffer && bindRenderbuffer &&
        storage && storageMultisample && attach && deleteRenderbuffer && blit, "Framebuffer entry points required");
    GLuint framebuffer = 0;
    GLuint renderbuffer = 0;
    genFramebuffer(1, &framebuffer);
    genRenderbuffer(1, &renderbuffer);
    bindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    if (multisampled) {
        constexpr int Samples = 2;
        storageMultisample(GL_RENDERBUFFER, Samples, GL_RGBA8, Width, Height);
    } else {
        storage(GL_RENDERBUFFER, GL_RGBA8, Width, Height);
    }
    bindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    attach(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbuffer);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glReadBuffer(GL_NONE);
    Scene();
    bindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glReadBuffer(GL_BACK);
    cse::ui::BackgroundBlur blur;
    const cse::ui::BackdropRect screen{0, 0, Width, Height};
    blur.Draw(screen, screen, Width, Height, {1, 0, 0.4f});
    blur.BeginFrame();
    Require(blur.Capture(0, 0, Width, Height), "Offscreen framebuffer must capture");
    GLint readFramebuffer = 0;
    GLint drawFramebuffer = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
    Require(readFramebuffer == 0 && drawFramebuffer == static_cast<GLint>(framebuffer),
        "Capture must restore distinct read/draw framebuffer bindings");
    bindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
    GLint sourceReadBuffer = 0;
    glGetIntegerv(GL_READ_BUFFER, &sourceReadBuffer);
    Require(sourceReadBuffer == GL_NONE, "Capture must restore source FBO's own read-buffer selection");
    bindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    Require(blur.Draw(screen, screen, Width, Height, {1, 0, 0.4f}), "Offscreen framebuffer must composite");
    bindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    bindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glDrawBuffer(GL_BACK);
    blit(0, 0, Width, Height, 0, 0, Width, Height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    bindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    const auto result = Pixels();
    Require(Red(result, 62, 0) > 45 && Red(result, 62, 0) < 110, "Offscreen/MSAA scene must blur correctly");
    blur.Shutdown();
    deleteFramebuffer(1, &framebuffer);
    deleteRenderbuffer(1, &renderbuffer);
    Require(glGetError() == GL_NO_ERROR, "Offscreen/MSAA path must not generate GL errors");
}

void ExerciseSceneAlpha() {
    Scene(0.0f);
    cse::ui::BackgroundBlur blur;
    const cse::ui::BackdropRect screen{0, 0, Width, Height};
    blur.Draw(screen, screen, Width, Height, {1, 0, 0.4f});
    blur.BeginFrame();
    Require(blur.Capture(0, 0, Width, Height), "Transparent-alpha scene must capture");
    Require(blur.Draw(screen, screen, Width, Height, {1, 0, 0.4f}), "Transparent-alpha scene must composite");
    const auto result = Pixels();
    Require(Red(result, 62, 0) > 45 && Red(result, 62, 0) < 110,
        "World framebuffer alpha must not weaken backdrop opacity");
    blur.Shutdown();
}

void ExerciseCallerState() {
    Scene();
    const auto activeTexture = reinterpret_cast<PFNGLACTIVETEXTUREPROC>(SDL_GL_GetProcAddress("glActiveTexture"));
    const auto secondaryColor = reinterpret_cast<PFNGLSECONDARYCOLOR3FPROC>(
        SDL_GL_GetProcAddress("glSecondaryColor3f"));
    Require(activeTexture && secondaryColor, "Compatibility texture/color entry points required");
    glEnable(GL_COLOR_SUM);
    secondaryColor(1, 1, 1);
    const GLdouble plane[] = {1, 0, 0, 0};
    glClipPlane(GL_CLIP_PLANE0, plane);
    glEnable(GL_CLIP_PLANE0);
    activeTexture(GL_TEXTURE1);
    glEnable(GL_TEXTURE_2D);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glTranslatef(0.2f, 0.3f, 0);
    std::array<GLfloat, 16> textureMatrix{};
    glGetFloatv(GL_TEXTURE_MATRIX, textureMatrix.data());
    cse::ui::BackgroundBlur blur;
    const cse::ui::BackdropRect screen{0, 0, Width, Height};
    blur.Draw(screen, screen, Width, Height, {1, 0, 0.4f});
    blur.BeginFrame();
    Require(blur.Capture(0, 0, Width, Height), "Caller-state scene must capture");
    Require(blur.Draw(screen, screen, Width, Height, {1, 0, 0.4f}), "Caller-state scene must composite");
    GLint active = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    Require(active == GL_TEXTURE1 && glIsEnabled(GL_TEXTURE_2D), "Active texture and texture enables must restore");
    std::array<GLfloat, 16> restoredMatrix{};
    glGetFloatv(GL_TEXTURE_MATRIX, restoredMatrix.data());
    Require(textureMatrix == restoredMatrix, "Caller texture matrix must restore");
    Require(glIsEnabled(GL_COLOR_SUM) && glIsEnabled(GL_CLIP_PLANE0), "Caller color sum and clipping must restore");
    const auto result = Pixels();
    Require(Red(result, 62, 0) > 45 && Red(result, 62, 0) < 110,
        "World clipping and secondary color must not affect backdrop filtering");
    blur.Shutdown();
    glDisable(GL_COLOR_SUM);
    glDisable(GL_CLIP_PLANE0);
    glDisable(GL_TEXTURE_2D);
    glLoadIdentity();
    activeTexture(GL_TEXTURE0);
    glMatrixMode(GL_MODELVIEW);
}
} // namespace

int main() {
    try {
        Require(SDL_Init(SDL_INIT_VIDEO), SDL_GetError());
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        SDL_Window* window = SDL_CreateWindow("Backdrop fixture", Width, Height, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
        Require(window != nullptr, SDL_GetError());
        SDL_GLContext context = SDL_GL_CreateContext(window);
        Require(context != nullptr, SDL_GetError());
        Exercise();
        ExerciseSceneAlpha();
        ExerciseCallerState();
        ExerciseFramebuffer(false);
        ExerciseFramebuffer(true);
        SDL_GL_DestroyContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        std::puts("Backdrop pixels, gradient, clipping, foreground, demand and lifecycle passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
