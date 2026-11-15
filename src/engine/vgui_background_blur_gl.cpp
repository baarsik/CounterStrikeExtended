#include "vgui_background_blur_gl.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <array>
#include <cstdint>
#include <iterator>

namespace cse {
namespace ui {
namespace {
constexpr int Downsample = 4;
constexpr int GradientStrips = 32;
constexpr int SceneTarget = 0;
constexpr int BlurTarget = 1;
constexpr int ScratchTarget = 2;
constexpr std::size_t TargetCount = 3;

struct GlFunctions {
    PFNGLACTIVETEXTUREPROC activeTexture = nullptr;
    PFNGLUSEPROGRAMPROC useProgram = nullptr;
    PFNGLBINDBUFFERPROC bindBuffer = nullptr;
    PFNGLBLENDEQUATIONPROC blendEquation = nullptr;
    PFNGLGENFRAMEBUFFERSPROC genFramebuffers = nullptr;
    PFNGLDELETEFRAMEBUFFERSPROC deleteFramebuffers = nullptr;
    PFNGLBINDFRAMEBUFFERPROC bindFramebuffer = nullptr;
    PFNGLFRAMEBUFFERTEXTURE2DPROC framebufferTexture = nullptr;
    PFNGLCHECKFRAMEBUFFERSTATUSPROC checkFramebuffer = nullptr;
    PFNGLBLITFRAMEBUFFERPROC blitFramebuffer = nullptr;
    bool rectangleTextures = false;
};

template<typename Function>
Function Load(const char* name, const char* alternate = nullptr) {
    SDL_FunctionPointer function = SDL_GL_GetProcAddress(name);
    if (!function && alternate) {
        function = SDL_GL_GetProcAddress(alternate);
    }
    return reinterpret_cast<Function>(function);
}

bool LoadFunctions(GlFunctions& functions) {
    if (!SDL_GL_ExtensionSupported("GL_ARB_framebuffer_object") &&
        !(SDL_GL_ExtensionSupported("GL_EXT_framebuffer_object") &&
          SDL_GL_ExtensionSupported("GL_EXT_framebuffer_blit"))) {
        return false;
    }
    functions.activeTexture = Load<PFNGLACTIVETEXTUREPROC>("glActiveTexture", "glActiveTextureARB");
    functions.useProgram = Load<PFNGLUSEPROGRAMPROC>("glUseProgram");
    functions.bindBuffer = Load<PFNGLBINDBUFFERPROC>("glBindBuffer", "glBindBufferARB");
    functions.blendEquation = Load<PFNGLBLENDEQUATIONPROC>("glBlendEquation", "glBlendEquationEXT");
    functions.genFramebuffers = Load<PFNGLGENFRAMEBUFFERSPROC>("glGenFramebuffers", "glGenFramebuffersEXT");
    functions.deleteFramebuffers = Load<PFNGLDELETEFRAMEBUFFERSPROC>("glDeleteFramebuffers", "glDeleteFramebuffersEXT");
    functions.bindFramebuffer = Load<PFNGLBINDFRAMEBUFFERPROC>("glBindFramebuffer", "glBindFramebufferEXT");
    functions.framebufferTexture = Load<PFNGLFRAMEBUFFERTEXTURE2DPROC>(
        "glFramebufferTexture2D", "glFramebufferTexture2DEXT");
    functions.checkFramebuffer = Load<PFNGLCHECKFRAMEBUFFERSTATUSPROC>(
        "glCheckFramebufferStatus", "glCheckFramebufferStatusEXT");
    functions.blitFramebuffer = Load<PFNGLBLITFRAMEBUFFERPROC>("glBlitFramebuffer", "glBlitFramebufferEXT");
    functions.rectangleTextures = SDL_GL_ExtensionSupported("GL_ARB_texture_rectangle") ||
        SDL_GL_ExtensionSupported("GL_EXT_texture_rectangle");
    return functions.activeTexture && functions.useProgram && functions.bindBuffer && functions.blendEquation &&
        functions.genFramebuffers &&
        functions.deleteFramebuffers && functions.bindFramebuffer && functions.framebufferTexture &&
        functions.checkFramebuffer && functions.blitFramebuffer;
}

// Preserve actual driver state so engine texture caches remain valid as well.
class GlState {
public:
    explicit GlState(const GlFunctions& functions) : gl_(functions) {
        glGetIntegerv(GL_MATRIX_MODE, &matrixMode_);
        glGetIntegerv(GL_CURRENT_PROGRAM, &program_);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer_);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer_);
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpackBuffer_);
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        gl_.useProgram(0);
        gl_.blendEquation(GL_FUNC_ADD);
        gl_.bindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        GLint textureUnits = 0;
        glGetIntegerv(GL_MAX_TEXTURE_UNITS, &textureUnits);
        for (GLint unit = 0; unit < textureUnits; ++unit) {
            gl_.activeTexture(GL_TEXTURE0 + unit);
            glDisable(GL_TEXTURE_1D);
            glDisable(GL_TEXTURE_2D);
            glDisable(GL_TEXTURE_3D);
            glDisable(GL_TEXTURE_CUBE_MAP);
            if (gl_.rectangleTextures) {
                glDisable(GL_TEXTURE_RECTANGLE);
            }
        }
        gl_.activeTexture(GL_TEXTURE0);
        glMatrixMode(GL_TEXTURE);
        glPushMatrix();
        glLoadIdentity();
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        glDisable(GL_ALPHA_TEST);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_LIGHTING);
        glDisable(GL_FOG);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_COLOR_LOGIC_OP);
        glDisable(GL_COLOR_SUM);
        glDisable(GL_POLYGON_STIPPLE);
        glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        glDisable(GL_SAMPLE_ALPHA_TO_ONE);
        glDisable(GL_SAMPLE_COVERAGE);
        GLint clipPlanes = 0;
        glGetIntegerv(GL_MAX_CLIP_PLANES, &clipPlanes);
        for (GLint plane = 0; plane < clipPlanes; ++plane) {
            glDisable(GL_CLIP_PLANE0 + plane);
        }
        glDisable(GL_TEXTURE_GEN_S);
        glDisable(GL_TEXTURE_GEN_T);
        glDisable(GL_TEXTURE_GEN_R);
        glDisable(GL_TEXTURE_GEN_Q);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glShadeModel(GL_SMOOTH);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    }

    ~GlState() {
        glMatrixMode(GL_TEXTURE);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glMatrixMode(matrixMode_);
        gl_.bindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer_);
        gl_.bindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebuffer_);
        gl_.bindBuffer(GL_PIXEL_UNPACK_BUFFER, unpackBuffer_);
        gl_.useProgram(program_);
        glPopAttrib();
    }

private:
    const GlFunctions& gl_;
    GLint matrixMode_ = GL_MODELVIEW;
    GLint program_ = 0;
    GLint readFramebuffer_ = 0;
    GLint drawFramebuffer_ = 0;
    GLint unpackBuffer_ = 0;
};

void Quad(float left, float top, float right, float bottom, float s0, float t0, float s1, float t1) {
    glBegin(GL_QUADS);
    glTexCoord2f(s0, t0);
    glVertex2f(left, top);
    glTexCoord2f(s1, t0);
    glVertex2f(right, top);
    glTexCoord2f(s1, t1);
    glVertex2f(right, bottom);
    glTexCoord2f(s0, t1);
    glVertex2f(left, bottom);
    glEnd();
}
} // namespace

struct BackgroundBlur::Impl {
    GlFunctions gl;
    std::array<GLuint, TargetCount> textures{};
    std::array<GLuint, TargetCount> framebuffers{};
    int width = 0;
    int height = 0;
    int originX = 0;
    int originY = 0;
    bool requested = false;
    bool captured = false;
    bool attempted = false;
    bool supported = false;

    void ReleaseTargets() {
        if (gl.deleteFramebuffers) {
            gl.deleteFramebuffers(static_cast<GLsizei>(framebuffers.size()), framebuffers.data());
        }
        glDeleteTextures(static_cast<GLsizei>(textures.size()), textures.data());
        textures.fill(0);
        framebuffers.fill(0);
        width = 0;
        height = 0;
        captured = false;
    }

    bool Allocate(int newWidth, int newHeight) {
        if (newWidth == width && newHeight == height) {
            return true;
        }
        ReleaseTargets();
        GLint maximumSize = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximumSize);
        if (newWidth > maximumSize || newHeight > maximumSize) {
            return false;
        }
        glGenTextures(static_cast<GLsizei>(textures.size()), textures.data());
        gl.genFramebuffers(static_cast<GLsizei>(framebuffers.size()), framebuffers.data());
        for (std::size_t target = 0; target < textures.size(); ++target) {
            const int targetWidth = target == SceneTarget ? newWidth : (newWidth + Downsample - 1) / Downsample;
            const int targetHeight = target == SceneTarget ? newHeight : (newHeight + Downsample - 1) / Downsample;
            glBindTexture(GL_TEXTURE_2D, textures[target]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, targetWidth, targetHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            gl.bindFramebuffer(GL_FRAMEBUFFER, framebuffers[target]);
            gl.framebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[target], 0);
            if (!textures[target] || !framebuffers[target] ||
                gl.checkFramebuffer(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                ReleaseTargets();
                return false;
            }
        }
        width = newWidth;
        height = newHeight;
        return true;
    }

    void Filter(int source, int destination, bool horizontal) {
        const int smallWidth = (width + Downsample - 1) / Downsample;
        const int smallHeight = (height + Downsample - 1) / Downsample;
        gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffers[destination]);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glViewport(0, 0, smallWidth, smallHeight);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textures[source]);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        // Normalized nine-tap Gaussian, paired into five bilinear texture samples.
        constexpr float Weights[] = {0.2270270270f, 0.3162162162f, 0.3162162162f, 0.0702702703f, 0.0702702703f};
        constexpr float Offsets[] = {0.0f, 1.3846153846f, -1.3846153846f, 3.2307692308f, -3.2307692308f};
        // Widen the sample footprint slightly without adding filtering passes.
        constexpr float BlurRadiusScale = 1.15f;
        for (std::size_t tap = 0; tap < std::size(Weights); ++tap) {
            const float offsetX = horizontal ? Offsets[tap] * BlurRadiusScale / smallWidth : 0.0f;
            const float offsetY = horizontal ? 0.0f : Offsets[tap] * BlurRadiusScale / smallHeight;
            glColor4f(Weights[tap], Weights[tap], Weights[tap], Weights[tap]);
            Quad(-1, -1, 1, 1, offsetX, offsetY, 1 + offsetX, 1 + offsetY);
        }
    }
};

BackgroundBlur::BackgroundBlur() : impl_(std::make_unique<Impl>()) {}
BackgroundBlur::~BackgroundBlur() = default;

void BackgroundBlur::BeginFrame() {
    impl_->captured = false;
}

bool BackgroundBlur::Capture(int x, int y, int width, int height) {
    Impl& state = *impl_;
    state.captured = false;
    const bool requested = state.requested;
    state.requested = false;
    if (!requested || x < 0 || y < 0 || width <= 0 || height <= 0 || !SDL_GL_GetCurrentContext()) {
        return false;
    }
    if (!state.attempted) {
        state.attempted = true;
        state.supported = LoadFunctions(state.gl);
    }
    if (!state.supported) {
        return false;
    }
    GLint sourceFramebuffer = 0;
    GLint sourceBuffer = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &sourceFramebuffer);
    glGetIntegerv(GL_DRAW_BUFFER, &sourceBuffer);
    GlState saved(state.gl);
    if (!state.Allocate(width, height)) {
        return false;
    }
    // Resolve at identical dimensions first: multisampled sources cannot be scaled during resolve.
    state.gl.bindFramebuffer(GL_READ_FRAMEBUFFER, sourceFramebuffer);
    GLint sourceReadBuffer = 0;
    glGetIntegerv(GL_READ_BUFFER, &sourceReadBuffer);
    glReadBuffer(sourceBuffer);
    state.gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, state.framebuffers[SceneTarget]);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    state.gl.blitFramebuffer(x, y, x + width, y + height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glReadBuffer(sourceReadBuffer);
    state.gl.bindFramebuffer(GL_READ_FRAMEBUFFER, state.framebuffers[SceneTarget]);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    state.gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, state.framebuffers[BlurTarget]);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    const int smallWidth = (width + Downsample - 1) / Downsample;
    const int smallHeight = (height + Downsample - 1) / Downsample;
    state.gl.blitFramebuffer(0, 0, width, height, 0, 0, smallWidth, smallHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    state.Filter(BlurTarget, ScratchTarget, true);
    state.Filter(ScratchTarget, BlurTarget, false);
    state.originX = x;
    state.originY = y;
    state.captured = true;
    return true;
}

bool BackgroundBlur::Draw(const BackdropRect& panel, const BackdropRect& clip, int screenWidth, int screenHeight,
    const BackdropStyle& style) {
    Impl& state = *impl_;
    if (panel.width <= 0 || panel.height <= 0 || clip.width <= 0 || clip.height <= 0 ||
        screenWidth <= 0 || screenHeight <= 0) {
        return false;
    }
    state.requested = true;
    if (!state.captured) {
        return false;
    }
    using Coordinate = std::int64_t;
    const Coordinate left = (std::max)({Coordinate{0}, Coordinate{panel.x}, Coordinate{clip.x}});
    const Coordinate right = (std::min)({Coordinate{screenWidth}, Coordinate{panel.x} + panel.width,
        Coordinate{clip.x} + clip.width});
    const Coordinate top = (std::max)({Coordinate{0}, Coordinate{panel.y}, Coordinate{clip.y}});
    const Coordinate bottom = (std::min)({Coordinate{screenHeight}, Coordinate{panel.y} + panel.height,
        Coordinate{clip.y} + clip.height});
    if (left >= right || top >= bottom) {
        return true;
    }
    GlState saved(state.gl);
    glViewport(state.originX, state.originY, state.width, state.height);
    glMatrixMode(GL_PROJECTION);
    glOrtho(0, screenWidth, screenHeight, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, state.textures[BlurTarget]);
    // Framebuffer alpha is not coverage. Only the panel gradient controls composite opacity.
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
    glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 1);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PRIMARY_COLOR);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
    glTexEnvi(GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    const float tint = 1.0f - UnitValue(style.darkness);
    const float s0 = static_cast<float>(left) / screenWidth;
    const float s1 = static_cast<float>(right) / screenWidth;
    glBegin(GL_QUAD_STRIP);
    for (int strip = 0; strip <= GradientStrips; ++strip) {
        const float y = static_cast<float>(top) + static_cast<float>(bottom - top) * strip / GradientStrips;
        const float position = (y - panel.y) / panel.height;
        glColor4f(tint, tint, tint, BackdropOpacity(style, position));
        glTexCoord2f(s0, 1.0f - y / screenHeight);
        glVertex2f(static_cast<float>(left), y);
        glTexCoord2f(s1, 1.0f - y / screenHeight);
        glVertex2f(static_cast<float>(right), y);
    }
    glEnd();
    return true;
}

void BackgroundBlur::Shutdown() {
    if (SDL_GL_GetCurrentContext()) {
        impl_->ReleaseTargets();
    }
    *impl_ = Impl{};
}

} // namespace ui
} // namespace cse
