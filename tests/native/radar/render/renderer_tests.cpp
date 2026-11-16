#include "Test.h"

#include <array>
#include <cstdarg>
#include <cstdio>
#include <memory>

#include <SDL3/SDL.h>
#include <glad/glad.h>
#include <ncl_gl/loader.h>

#include "NativeSdk.h"
#include "hud/radar/render/RadarDraw.h"
#include "hud/radar/map/MapRender.h"
#include "RadarMapLayerFixtures.h"

cl_enginefunc_t gEngfuncs{};

namespace {
constexpr int kImageSize = 128;
constexpr int kPixelComponents = 4;

void ShaderFailure(char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    std::vfprintf(stderr, format, arguments);
    va_end(arguments);
}

class Context {
  public:
    SDL_Window* window{};
    SDL_GLContext context{};

    void Initialize() {
        CHECK_TRUE(SDL_Init(SDL_INIT_VIDEO)) << SDL_GetError();
        CHECK_TRUE(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2));
        CHECK_TRUE(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1));
        CHECK_TRUE(SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0));
        window =
            SDL_CreateWindow("Native radar GPU tests", kImageSize, kImageSize, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
        CHECK_TRUE(window) << SDL_GetError();
        context = SDL_GL_CreateContext(window);
        CHECK_TRUE(context) << SDL_GetError();
        CHECK_TRUE(SDL_GL_MakeCurrent(window, context));
        CHECK_TRUE(ncl_gl::Reload());
        CHECK_TRUE(GLAD_GL_VERSION_2_0);
    }

    ~Context() {
        if (context) {
            SDL_GL_DestroyContext(context);
        }
        if (window) {
            SDL_DestroyWindow(window);
        }
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
};

class RadarGpuTest {
  protected:
    Context context_;
    radar::integration::Performance performance_;
    RadarDrawContext draw_{performance_};
    GLuint texture_{};
    GLuint framebuffer_{};
    GLuint baseline_atlas_{};
    PFNGLGENFRAMEBUFFERSPROC gen_framebuffers_{};
    PFNGLBINDFRAMEBUFFERPROC bind_framebuffer_{};
    PFNGLFRAMEBUFFERTEXTURE2DPROC framebuffer_texture_{};
    PFNGLCHECKFRAMEBUFFERSTATUSPROC check_framebuffer_{};
    PFNGLDELETEFRAMEBUFFERSPROC delete_framebuffers_{};

    void SetUp() {
        context_.Initialize();
        std::printf("OpenGL renderer: %s; version: %s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));
        std::printf("FBO capabilities: ARB=%d EXT=%d core=%d ext=%d blit=%d ext_blit=%d\n",
                    GLAD_GL_ARB_framebuffer_object, GLAD_GL_EXT_framebuffer_object, glGenFramebuffers != nullptr,
                    glGenFramebuffersEXT != nullptr, glBlitFramebuffer != nullptr, glBlitFramebufferEXT != nullptr);
        gEngfuncs.Con_DPrintf = ShaderFailure;
        draw_.Prepare(context_.context, context_.window);
        CHECK_TRUE(draw_.CheckPrimsAvailable());
        // macOS legacy contexts expose framebuffer objects through EXT entry points.
        gen_framebuffers_ =
            glGenFramebuffers ? glGenFramebuffers : reinterpret_cast<PFNGLGENFRAMEBUFFERSPROC>(glGenFramebuffersEXT);
        bind_framebuffer_ =
            glBindFramebuffer ? glBindFramebuffer : reinterpret_cast<PFNGLBINDFRAMEBUFFERPROC>(glBindFramebufferEXT);
        framebuffer_texture_ = glFramebufferTexture2D
                                   ? glFramebufferTexture2D
                                   : reinterpret_cast<PFNGLFRAMEBUFFERTEXTURE2DPROC>(glFramebufferTexture2DEXT);
        check_framebuffer_ = glCheckFramebufferStatus
                                 ? glCheckFramebufferStatus
                                 : reinterpret_cast<PFNGLCHECKFRAMEBUFFERSTATUSPROC>(glCheckFramebufferStatusEXT);
        delete_framebuffers_ = glDeleteFramebuffers
                                   ? glDeleteFramebuffers
                                   : reinterpret_cast<PFNGLDELETEFRAMEBUFFERSPROC>(glDeleteFramebuffersEXT);
        CHECK_TRUE(gen_framebuffers_ && bind_framebuffer_ && framebuffer_texture_ && check_framebuffer_ &&
                   delete_framebuffers_);
        glGenTextures(1, &texture_);
        glBindTexture(GL_TEXTURE_2D, texture_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kImageSize, kImageSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        gen_framebuffers_(1, &framebuffer_);
        bind_framebuffer_(GL_FRAMEBUFFER, framebuffer_);
        framebuffer_texture_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_, 0);
        CHECK_EQ(check_framebuffer_(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);
        glViewport(0, 0, kImageSize, kImageSize);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, kImageSize, 0, kImageSize, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        for (GLenum capability :
             {GL_ALPHA_TEST, GL_DEPTH_TEST, GL_CULL_FACE, GL_SCISSOR_TEST, GL_LIGHTING, GL_FOG, GL_DITHER}) {
            glDisable(capability);
        }
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        baseline_atlas_ = Atlas();
        Clear();
        CHECK_EQ(glGetError(), GL_NO_ERROR);
    }

    void Clear() {
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    std::array<unsigned char, kPixelComponents> Pixel(int x, int y) {
        std::array<unsigned char, kPixelComponents> pixel{};
        glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        CHECK_EQ(glGetError(), GL_NO_ERROR);
        return pixel;
    }

    std::vector<unsigned char> Pixels() {
        std::vector<unsigned char> pixels(kImageSize * kImageSize * kPixelComponents);
        glReadPixels(0, 0, kImageSize, kImageSize, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        CHECK_EQ(glGetError(), GL_NO_ERROR);
        return pixels;
    }

    GLuint Atlas() {
        GLuint atlas = 0;
        glGenTextures(1, &atlas);
        glBindTexture(GL_TEXTURE_2D, atlas);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        const std::array<unsigned char, 2> luminance_alpha{255, 255};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, 1, 1, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE,
                     luminance_alpha.data());
        return atlas;
    }

  public:
    ~RadarGpuTest() {
        if (!context_.context) {
            return;
        }
        SDL_GL_MakeCurrent(context_.window, context_.context);
        draw_.ResetGl(draw_.is_context(context_.context, context_.window));
        if (baseline_atlas_) {
            glDeleteTextures(1, &baseline_atlas_);
        }
        if (texture_) {
            glDeleteTextures(1, &texture_);
        }
        if (framebuffer_) {
            delete_framebuffers_(1, &framebuffer_);
        }
    }
};
} // namespace

RADAR_TEST_F(RadarGpuTest, GlyphsAndPrimitivesShareRoundAndSquareClip) {
    const GLuint atlas = Atlas();
    for (bool square : {false, true}) {
        RADAR_TRACE(square);
        Clear();
        draw_.SetPrimClip({64, 64}, 24, square);
        draw_.BeginPrims();
        draw_.DrawRect({0, 0}, {128, 128}, {255, 0, 0, 255});
        draw_.EndPrims();
        const auto primitives = Pixels();
        Clear();
        draw_.BeginPrims();
        draw_.DrawGlyph(atlas, {{0, 0}, {128, 128}}, {{0, 0}, {1, 1}}, {255, 0, 0, 255});
        draw_.EndPrims();
        CHECK_EQ(Pixels(), primitives);
        CHECK_GT(Pixel(64, 64)[0], 250);
        CHECK_EQ(Pixel(100, 64)[3], 0);
        CHECK_EQ(Pixel(84, 84)[3] > 200, square);
    }
    glDeleteTextures(1, &atlas);
}

RADAR_TEST_F(RadarGpuTest, TranslucentMarkersIgnoreAndRestoreInheritedAlphaTest) {
    const auto render = [&] {
        Clear();
        draw_.BeginPrims();
        draw_.DrawBomb({64, 64}, 8, {255, 215, 95, 102});
        draw_.DrawDisc({32, 64}, 5, {255, 0, 0, 128});
        draw_.EndPrims();
        return Pixels();
    };
    const auto reference = render();
    CHECK_GT(Pixel(64, 64)[0], 50);
    for (float cutoff : {0.25f, 0.5f, 0.9f}) {
        RADAR_TRACE(cutoff);
        glAlphaFunc(GL_GREATER, cutoff);
        glEnable(GL_ALPHA_TEST);
        CHECK_EQ(render(), reference);
        CHECK_EQ(glIsEnabled(GL_ALPHA_TEST), GL_TRUE);
        GLint function = 0;
        GLfloat restored_cutoff = 0;
        glGetIntegerv(GL_ALPHA_TEST_FUNC, &function);
        glGetFloatv(GL_ALPHA_TEST_REF, &restored_cutoff);
        CHECK_EQ(function, GL_GREATER);
        CHECK_FLOAT_EQ(restored_cutoff, cutoff);
    }
}

RADAR_TEST_F(RadarGpuTest, GlyphFlushRestoresTextureUnitBindingsAndArrayBuffer) {
    const GLuint atlas = Atlas();
    GLuint incoming_texture = 0;
    GLuint incoming_buffer = 0;
    glGenTextures(1, &incoming_texture);
    glGenBuffers(1, &incoming_buffer);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, incoming_texture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, incoming_texture);
    glBindBuffer(GL_ARRAY_BUFFER, incoming_buffer);
    draw_.BeginPrims();
    draw_.DrawGlyph(atlas, {{32, 32}, {96, 96}}, {{0, 0}, {1, 1}}, {255, 255, 255, 255});
    draw_.EndPrims();
    GLint state = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &state);
    CHECK_EQ(state, GL_TEXTURE1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &state);
    CHECK_EQ(state, static_cast<GLint>(incoming_texture));
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &state);
    CHECK_EQ(state, static_cast<GLint>(incoming_texture));
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &state);
    CHECK_EQ(state, static_cast<GLint>(incoming_buffer));
    CHECK_GT(Pixel(64, 64)[0], 250);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &incoming_buffer);
    glDeleteTextures(1, &incoming_texture);
    glDeleteTextures(1, &atlas);
}

RADAR_TEST_F(RadarGpuTest, EngineBlendChangesBetweenRegionsDoNotReuseCachedFactors) {
    draw_.BeginPrims();
    draw_.DrawDisc({32, 64}, 5, {255, 0, 0, 128});
    draw_.EndPrims();
    const auto reference = Pixel(32, 64);
    glBlendFunc(GL_ZERO, GL_ZERO);
    glDisable(GL_BLEND);
    draw_.BeginPrims();
    draw_.DrawDisc({96, 64}, 5, {255, 0, 0, 128});
    draw_.EndPrims();
    CHECK_EQ(Pixel(96, 64), reference);
    GLint source = 0;
    GLint destination = 0;
    glGetIntegerv(GL_BLEND_SRC, &source);
    glGetIntegerv(GL_BLEND_DST, &destination);
    CHECK_EQ(source, GL_SRC_ALPHA);
    CHECK_EQ(destination, GL_ONE_MINUS_SRC_ALPHA);
    CHECK_EQ(glIsEnabled(GL_BLEND), GL_TRUE);
}

RADAR_TEST_F(RadarGpuTest, ResetRecreatesShadersAndEndFrameClearsClipAndFade) {
    draw_.SetPrimClip({1, 1}, 1, true);
    draw_.SetFade({255, 255, 255, 255});
    draw_.BeginPrims();
    draw_.DrawDisc({64, 64}, 5, {255, 0, 0, 255});
    draw_.EndFrame();
    CHECK_EQ(Pixel(64, 64)[3], 0);
    draw_.BeginPrims();
    draw_.DrawDisc({64, 64}, 5, {255, 0, 0, 255});
    draw_.EndPrims();
    const auto reference = Pixels();
    CHECK_EQ(Pixel(64, 64)[0], 255);
    CHECK_EQ(Pixel(64, 64)[1], 0);
    draw_.ResetGl(true);
    draw_.Prepare(context_.context, context_.window);
    CHECK_TRUE(draw_.CheckPrimsAvailable());
    Clear();
    draw_.BeginPrims();
    draw_.DrawDisc({64, 64}, 5, {255, 0, 0, 255});
    draw_.EndPrims();
    CHECK_EQ(Pixels(), reference);
}

RADAR_TEST_F(RadarGpuTest, BackgroundUsesBlurWhenAvailableAndSafelyFallsBackOtherwise) {
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    const bool available = glGenFramebuffers && glBindFramebuffer && glFramebufferTexture2D && glBlitFramebuffer;
    CHECK_EQ(draw_.DrawBlurBackground(32, 32, 64, true, 0.5f, 1.0f, 128, 128), available);
    if (available) {
        CHECK_NEAR(Pixel(64, 64)[0], 127, 2);
    } else {
        CHECK_EQ(Pixel(64, 64)[0], 255);
    }
}

RADAR_TEST_F(RadarGpuTest, GeneratedTexturedAndFlatFloorLayersRenderActualPixels) {
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0, 512}, {1024, 768}});
    fixture.AddFloor(0.0f, {{0, 0}, {512, 512}});
    fixture.AddFloor(256.0f, {{0, 0}, {512, 1024}});
    fixture.AddFloor(512.0f, {{0, 0}, {768, 512}});
    RadarMapLayers map;
    CHECK_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    std::string error;
    CHECK_TRUE(map.ApplyOverrides({{{"lower", -128, 128}, {"middle", 128, 384}, {"upper", 384, 640}}, {}}, error));
    std::array<GLuint, 4> textures{};
    glGenTextures(static_cast<GLsizei>(textures.size()), textures.data());
    const std::array<std::array<unsigned char, 4>, 4> colors{
        {{255, 0, 0, 255}, {255, 0, 0, 255}, {0, 255, 0, 255}, {0, 0, 255, 255}}};
    std::vector<MapLayerMaterial> materials;
    for (size_t index = 0; index < textures.size(); ++index) {
        glBindTexture(GL_TEXTURE_2D, textures[index]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, colors[index].data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        materials.push_back({textures[index], 0, false});
    }
    for (bool textured : {false, true}) {
        RADAR_TRACE(textured);
        MapLayerRenderer renderer(performance_);
        struct Release {
            MapLayerRenderer& renderer;
            ~Release() {
                renderer.Reset();
            }
        } release{renderer};
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        CHECK_EQ(renderer.Ensure(map, materials, 1, textured, 1), RadarMapStatus::Ready);
        GLint source = 0;
        GLint destination = 0;
        GLint framebuffer = 0;
        glGetIntegerv(GL_BLEND_SRC, &source);
        glGetIntegerv(GL_BLEND_DST, &destination);
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
        CHECK_EQ(glIsEnabled(GL_BLEND), GL_TRUE);
        CHECK_EQ(source, GL_ONE);
        CHECK_EQ(destination, GL_ONE);
        CHECK_EQ(framebuffer, static_cast<GLint>(framebuffer_));
        const auto* image = renderer.get_map(1);
        CHECK_TRUE(image);
        glBindTexture(GL_TEXTURE_2D, image->texture);
        std::vector<unsigned char> pixels(kMapImageSize * kMapImageSize * kPixelComponents);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        const auto generated_pixel = [&](int x, int y, int component) {
            return pixels[(static_cast<size_t>(y) * kMapImageSize + x) * kPixelComponents + component];
        };
        CHECK_EQ(generated_pixel(100, 100, 3), 255);
        CHECK_EQ(generated_pixel(650, 650, 3), 255);
        CHECK_EQ(generated_pixel(650, 100, 3), 255);
        CHECK_EQ(generated_pixel(900, 900, 3), 0);
        if (textured) {
            CHECK_GT(generated_pixel(100, 100, 1), generated_pixel(100, 100, 0));
            CHECK_GT(generated_pixel(650, 650, 0), generated_pixel(650, 650, 1));
            CHECK_GT(generated_pixel(650, 100, 2), generated_pixel(650, 100, 0));
        }
        CHECK_EQ(glGetError(), GL_NO_ERROR);
    }
    glDeleteTextures(static_cast<GLsizei>(textures.size()), textures.data());
}

RADAR_TEST_F(RadarGpuTest, ContextReplacementDiscardsOldNamesAndRebuildsPrograms) {
    {
        Context replacement;
        replacement.Initialize();
        std::array<GLuint, 12> foreign_programs{};
        for (auto& program : foreign_programs) {
            program = glCreateProgram();
        }
        draw_.ResetGl(false);
        for (const auto program : foreign_programs) {
            CHECK_TRUE(glIsProgram(program));
        }
        draw_.Prepare(replacement.context, replacement.window);
        CHECK_TRUE(draw_.CheckPrimsAvailable());
        draw_.ResetGl(true);
        for (const auto program : foreign_programs) {
            CHECK_TRUE(glIsProgram(program));
            glDeleteProgram(program);
        }
        CHECK_EQ(glGetError(), GL_NO_ERROR);
    }
    CHECK_TRUE(SDL_GL_MakeCurrent(context_.window, context_.context));
    CHECK_TRUE(ncl_gl::Reload());
    draw_.Prepare(context_.context, context_.window);
    CHECK_TRUE(draw_.CheckPrimsAvailable());
    Clear();
    draw_.BeginPrims();
    draw_.DrawDisc({64, 64}, 5, {255, 0, 0, 255});
    draw_.EndPrims();
    CHECK_EQ(Pixel(64, 64)[0], 255);
}
