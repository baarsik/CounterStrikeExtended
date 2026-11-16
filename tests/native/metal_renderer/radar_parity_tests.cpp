#define GL_GLEXT_PROTOTYPES
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include "radar_renderer_gpu.h"
#include "metal_session.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

using namespace cse::render;
namespace {
constexpr int kSide = 64;
constexpr unsigned kEpoch = 7;
constexpr int kTolerance = 2; // RGBA8 blending and shader arithmetic may round on either side of a byte boundary.

void Require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << message << " SDL: " << SDL_GetError() << '\n';
        std::exit(1);
    }
}

GLuint Shader(GLenum type, const char* name) {
    std::ifstream stream(std::string(CSE_RADAR_SHADER_DIRECTORY) + "/" + name);
    Require(bool(stream), name);
    const std::string source((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    const char* text = source.c_str();
    const auto shader = glCreateShader(type);
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);
    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    char log[4096]{};
    glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
    Require(success, log);
    return shader;
}

GLuint Program(const char* vertex, const char* fragment) {
    const auto program = glCreateProgram();
    const auto vs = Shader(GL_VERTEX_SHADER, vertex);
    const auto fs = Shader(GL_FRAGMENT_SHADER, fragment);
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    Require(success, "GL shader link");
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

void ComparePixels(MetalSession& session, const std::string& name) {
    std::vector<unsigned char> reference(kSide * kSide * 4);
    std::vector<unsigned char> actual(reference.size());
    glReadPixels(0, 0, kSide, kSide, GL_RGBA, GL_UNSIGNED_BYTE, reference.data());
    Require(glGetError() == GL_NO_ERROR, name + " GL error");
    std::string error;
    Require(session.ReadPixels(0, 0, kSide, kSide, 4, 1, actual.data(), error), error);
    int maximum = 0;
    unsigned changed = 0;
    for (unsigned index = 0; index < actual.size(); ++index) {
        const int delta = std::abs(int(actual[index]) - int(reference[index]));
        maximum = std::max(maximum, delta);
        changed += delta > kTolerance;
    }
    std::cout << name << " max=" << maximum << " outside_tolerance=" << changed << '\n';
    Require(maximum <= kTolerance, name + " pixel mismatch");
}
}

int main() {
    Require(SDL_Init(SDL_INIT_VIDEO), "SDL init");
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    auto* window = SDL_CreateWindow("Radar parity", kSide, kSide, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    Require(window, "GL window");
    auto context = SDL_GL_CreateContext(window);
    Require(context, "GL context");
    GLuint target = 0;
    GLuint color = 0;
    glGenFramebuffersEXT(1, &target);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, target);
    glGenRenderbuffersEXT(1, &color);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, color);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_RGBA8, kSide, kSide);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, color);
    Require(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT, "GL framebuffer");
    glViewport(0, 0, kSide, kSide);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, kSide, kSide, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DITHER);
    glEnable(GL_BLEND);
    const auto program = Program("radar_prim.vert", "radar_prim.frag");
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "uAtlas"), 0);
    std::string error;
    MetalSession session;
    Require(session.Initialize(nullptr, true, error), error);
    Require(session.Resize(kSide, kSide, 1, error), error);
    RadarRendererGpu effects(session, kEpoch);
    RasterState raster;
    raster.viewport = {{0, 0, kSide, kSide}};
    FrameSnapshot frame;
    frame.info.width = kSide;
    frame.info.height = kSide;
    FrameEvent clear;
    clear.kind = FrameEventKind::Clear;
    clear.clear.mask = kClearColor;
    clear.clear.color = {{0.2f, 0.3f, 0.4f, 0.6f}};
    frame.events.push_back(clear);
    auto image = std::make_shared<TextureImage>();
    image->defined = true;
    image->width = 2;
    image->height = 2;
    image->pixels = {255, 0, 0, 128, 0, 255, 0, 192, 128, 128, 128, 64, 255, 255, 255, 255};
    auto texture = std::make_shared<TextureObject>();
    texture->levels[0] = image;
    texture->sampler.minification = Filter::Nearest;
    texture->sampler.magnification = Filter::Nearest;
    const auto atlas = effects.RegisterTexture(1, RadarRendererGpu::TextureRole::Glyph, texture, nullptr);
    GLuint glAtlas = 0;
    glGenTextures(1, &glAtlas);
    glBindTexture(GL_TEXTURE_2D, glAtlas);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, image->pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    CseRadarPrimitiveVertex vertices[4]{};
    const float positions[4][2] = {{0, 0}, {64, 0}, {64, 64}, {0, 64}};
    const char* names[] = {"aPos", "aShape", "aCenter", "aColor", "aTri", "aTri2"};
    const std::size_t offsets[] = {offsetof(CseRadarPrimitiveVertex, position), offsetof(CseRadarPrimitiveVertex, shape),
        offsetof(CseRadarPrimitiveVertex, center), offsetof(CseRadarPrimitiveVertex, color),
        offsetof(CseRadarPrimitiveVertex, triangle), offsetof(CseRadarPrimitiveVertex, triangle2)};
    for (unsigned index = 0; index < std::size(names); ++index) {
        const auto location = glGetAttribLocation(program, names[index]);
        Require(location >= 0, names[index]);
        glEnableVertexAttribArray(location);
        glVertexAttribPointer(location, index == 0 ? 2 : 4, GL_FLOAT, GL_FALSE, sizeof(vertices[0]),
            reinterpret_cast<const char*>(vertices) + offsets[index]);
    }
    unsigned cases = 0;
    for (unsigned shape = 0; shape <= 10; ++shape) {
        for (unsigned variant = 0; variant < (shape == 10 ? 12u : 3u); ++variant) {
            for (unsigned corner = 0; corner < 4; ++corner) {
                auto& v = vertices[corner];
                v = {};
                std::copy(positions[corner], positions[corner] + 2, v.position);
                v.shape[0] = float(shape);
                v.shape[1] = 1.2f;
                v.shape[2] = 12;
                v.shape[3] = shape == 1 ? 25 : shape == 4 ? 7 : 2.5f;
                v.center[0] = 32;
                v.center[1] = 32;
                v.center[2] = 1;
                v.color[0] = 0.8f;
                v.color[1] = 0.4f;
                v.color[2] = 0.2f;
                v.color[3] = 0.7f;
                const float triangle[] = {18, 17, 48, 30};
                std::copy(triangle, triangle + 4, v.triangle);
                v.triangle2[0] = 25;
                v.triangle2[1] = 49;
                v.triangle2[2] = 0.65f;
                if (shape == 6) {
                    v.triangle[0] = positions[corner][0] / kSide;
                    v.triangle[1] = positions[corner][1] / kSide;
                }
                if (shape == 5) {
                    v.color[0] = positions[corner][0] / kSide;
                    v.color[1] = positions[corner][1] / kSide;
                }
                if (shape == 10) {
                    v.shape[3] = 0.4f;
                    v.center[2] = 25;
                    v.center[3] = 28;
                    v.triangle[0] = variant < 8 ? 1 : 0;
                    v.triangle[1] = 0;
                    v.triangle[2] = 0;
                    v.triangle[3] = variant < 8 ? 1 : 0;
                    v.triangle2[0] = float(variant % 4);
                    v.triangle2[1] = 0.4f;
                    v.triangle2[2] = 0.6f;
                }
            }
            CseRadarPrimitiveBatch batch{};
            batch.size = sizeof(batch);
            batch.rendererEpoch = kEpoch;
            batch.hud = {kSide, kSide};
            batch.quadCount = 1;
            batch.vertices = vertices;
            batch.atlas = shape == 6 ? atlas : CseRadarTexture{};
            batch.blend = shape == 1 ? CSE_RADAR_BLEND_ADDITIVE_ALPHA : CSE_RADAR_BLEND_STRAIGHT;
            batch.clip = {{32, 32}, 21, variant % 3};
            batch.fade = {{0.9f, 0.8f, 0.7f}, variant % 3 == 2 ? 0.35f : 0};
            glUniform2f(glGetUniformLocation(program, "uClipCenter"), 32, 32);
            glUniform1f(glGetUniformLocation(program, "uClipRadius"), batch.clip.shape ? 21 : 1.0e9f);
            glUniform1f(glGetUniformLocation(program, "uClipSquare"), batch.clip.shape == CSE_RADAR_SHAPE_SQUARE);
            glUniform3fv(glGetUniformLocation(program, "uFadeColor"), 1, batch.fade.rgb);
            glUniform1f(glGetUniformLocation(program, "uFadeAmt"), batch.fade.amount);
            glBlendFunc(GL_SRC_ALPHA, shape == 1 ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
            glClearColor(0.2f, 0.3f, 0.4f, 0.6f);
            glClear(GL_COLOR_BUFFER_BIT);
            glDrawArrays(GL_QUADS, 0, 4);
            Require(session.Render(frame, error), error);
            Require(effects.DrawPrimitiveQuads(batch, raster) == CSE_RADAR_OK, "Metal primitive");
            ComparePixels(session, "primitive-" + std::to_string(shape) + "-" + std::to_string(variant));
            ++cases;
        }
    }
    for (const auto* name : names) {
        glDisableVertexAttribArray(glGetAttribLocation(program, name));
    }
    const auto maskProgram = Program("radar.vert", "radar_mask.frag");
    glUseProgram(maskProgram);
    glUniform1i(glGetUniformLocation(maskProgram, "uTex"), 0);
    const auto sprite = effects.RegisterTexture(2, RadarRendererGpu::TextureRole::Sprite, texture, nullptr);
    for (unsigned shape = CSE_RADAR_SHAPE_CIRCLE; shape <= CSE_RADAR_SHAPE_SQUARE; ++shape) {
        for (unsigned comparison = 0; comparison < 8; ++comparison) {
            for (unsigned ringMode = 0; ringMode < 2; ++ringMode) {
                CseRadarMapTextureDraw tile{};
                tile.size = sizeof(tile);
                tile.rendererEpoch = kEpoch;
                tile.hud = {kSide, kSide};
                tile.texture = sprite;
                tile.mode = CSE_RADAR_MAP_MASKED_OVERVIEW;
                tile.mask = {{32, 32}, 24, 1.5f, shape, 0};
                tile.gamma = comparison % 2 ? 1.7f : 0;
                tile.fade = {{0.9f, 0.8f, 0.7f}, 0.3f};
                std::fill(tile.modulation, tile.modulation + 4, 0.8f);
                tile.vertexCount = 4;
                for (unsigned corner = 0; corner < 4; ++corner) {
                    std::copy(positions[corner], positions[corner] + 2, tile.vertices[corner].position);
                    tile.vertices[corner].uv[0] = positions[corner][0] / kSide;
                    tile.vertices[corner].uv[1] = positions[corner][1] / kSide;
                }
                glUniform2f(glGetUniformLocation(maskProgram, "uCenter"), 32, 32);
                glUniform1f(glGetUniformLocation(maskProgram, "uRadius"), 24);
                glUniform1f(glGetUniformLocation(maskProgram, "uFeather"), 1.5f);
                glUniform1f(glGetUniformLocation(maskProgram, "uSquare"), shape == CSE_RADAR_SHAPE_SQUARE);
                glUniform1f(glGetUniformLocation(maskProgram, "uGamma"), tile.gamma);
                glUniform1f(glGetUniformLocation(maskProgram, "uPremultiplied"), 0);
                glUniform1f(glGetUniformLocation(maskProgram, "uAlphaAa"), 0);
                glUniform1f(glGetUniformLocation(maskProgram, "uMode"), float(ringMode));
                glUniform1f(glGetUniformLocation(maskProgram, "uRingInner"), 10);
                glUniform4fv(glGetUniformLocation(maskProgram, "uRingColor"), 1, tile.modulation);
                glUniform3fv(glGetUniformLocation(maskProgram, "uFadeColor"), 1, tile.fade.rgb);
                glUniform1f(glGetUniformLocation(maskProgram, "uFadeAmt"), tile.fade.amount);
                glEnable(GL_ALPHA_TEST);
                glAlphaFunc(GL_NEVER + comparison, 0.4f);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glClear(GL_COLOR_BUFFER_BIT);
                glColor4fv(tile.modulation);
                glBegin(GL_QUADS);
                for (unsigned corner = 0; corner < tile.vertexCount; ++corner) {
                    glTexCoord2fv(tile.vertices[corner].uv);
                    glVertex2fv(tile.vertices[corner].position);
                }
                glEnd();
                raster.enabled[static_cast<std::size_t>(Capability::AlphaTest)] = true;
                raster.alphaFunction = static_cast<cse::render::Compare>(comparison);
                raster.alphaReference = 0.4f;
                Require(session.Render(frame, error), error);
                if (ringMode) {
                    CseRadarMaskRingDraw ring{};
                    ring.size = sizeof(ring);
                    ring.rendererEpoch = kEpoch;
                    ring.hud = tile.hud;
                    ring.outer = tile.mask;
                    ring.innerRadius = 10;
                    ring.fade = tile.fade;
                    std::copy(tile.modulation, tile.modulation + 4, ring.color);
                    Require(effects.DrawMaskRing(ring, raster) == CSE_RADAR_OK, "Metal ring");
                } else {
                    Require(effects.DrawMapTexture(tile, raster) == CSE_RADAR_OK, "Metal mask");
                }
                ComparePixels(session, "mask-" + std::to_string(shape) + "-alpha-" +
                    std::to_string(comparison) + "-ring-" + std::to_string(ringMode));
                ++cases;
            }
        }
    }
    // A hand-constructed RGBA8 map oracle: opaque geometry in opposite quadrants and transparent gaps.
    // At the diagonal intersection, rotated AA samples have equal alpha but different colours.
    CseRadarMapVertex mapVertices[8]{};
    const float mapPositions[8][2] = {{0, 0}, {0.5f, 0}, {0.5f, 0.5f}, {0, 0.5f},
        {0.5f, 0.5f}, {1, 0.5f}, {1, 1}, {0.5f, 1}};
    for (unsigned index = 0; index < 8; ++index) {
        std::copy(mapPositions[index], mapPositions[index] + 2, mapVertices[index].position);
        mapVertices[index].position[2] = 1;
        mapVertices[index].color[index < 4 ? 0 : 1] = 1;
        mapVertices[index].color[3] = 0.5f;
    }
    const std::uint32_t mapIndices[] = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};
    CseRadarMapDraw mapDraw{};
    mapDraw.indexCount = 12;
    CseRadarMapPass mapPass{0, 0, 1, 1, 0, 2, 0, 1, CSE_RADAR_DEPTH_TOPMOST, 1};
    CseRadarMapImageBuild build{};
    build.size = sizeof(build);
    build.rendererEpoch = kEpoch;
    build.vertices = mapVertices;
    build.vertexCount = 8;
    build.indices = mapIndices;
    build.indexCount = 12;
    build.draws = &mapDraw;
    build.drawCount = 1;
    build.passes = &mapPass;
    build.passCount = 1;
    constexpr unsigned kMapSide = CSE_RADAR_MAP_IMAGE_SIZE;
    std::vector<unsigned char> mapBytes(kMapSide * kMapSide * 4);
    for (unsigned tone = 0; tone < 2; ++tone) {
        build.tone = tone;
        CseRadarTexture generated{};
        Require(effects.BuildMapImage(build, generated) == CSE_RADAR_OK, "generated map");
        std::fill(mapBytes.begin(), mapBytes.end(), 0);
        for (unsigned y = 0; y < kMapSide; ++y) {
            for (unsigned x = 0; x < kMapSide; ++x) {
                if ((x < kMapSide / 2) != (y < kMapSide / 2)) {
                    continue;
                }
                auto* pixel = mapBytes.data() + (y * kMapSide + x) * 4;
                pixel[0] = pixel[1] = pixel[2] = tone ? 22 : 0;
                pixel[x < kMapSide / 2 ? 0 : 1] = 128;
                pixel[3] = 128;
            }
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kMapSide, kMapSide, 0, GL_RGBA, GL_UNSIGNED_BYTE, mapBytes.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        for (unsigned aa = 0; aa < 2; ++aa) {
            for (float shift : {-0.0078125f, -0.005859375f, 0.0f, 0.005859375f, 0.0078125f}) {
                CseRadarMapTextureDraw tile{};
                tile.size = sizeof(tile);
                tile.rendererEpoch = kEpoch;
                tile.hud = {kSide, kSide};
                tile.texture = generated;
                tile.mode = CSE_RADAR_MAP_PREMULTIPLIED_IMAGE;
                tile.mask = {{32, 32}, 40, 1.5f, CSE_RADAR_SHAPE_SQUARE, 0};
                tile.gamma = tone ? 1.3f : 0;
                tile.alphaAa = aa;
                tile.fade = {{0.9f, 0.8f, 0.7f}, 0.2f};
                std::fill(tile.modulation, tile.modulation + 4, 1);
                tile.vertexCount = 4;
                for (unsigned corner = 0; corner < 4; ++corner) {
                    std::copy(positions[corner], positions[corner] + 2, tile.vertices[corner].position);
                    tile.vertices[corner].uv[0] = positions[corner][0] / kSide + shift;
                    tile.vertices[corner].uv[1] = positions[corner][1] / kSide - shift;
                }
                glUniform1f(glGetUniformLocation(maskProgram, "uRadius"), 40);
                glUniform1f(glGetUniformLocation(maskProgram, "uSquare"), 1);
                glUniform1f(glGetUniformLocation(maskProgram, "uGamma"), tile.gamma);
                glUniform1f(glGetUniformLocation(maskProgram, "uPremultiplied"), 1);
                glUniform1f(glGetUniformLocation(maskProgram, "uAlphaAa"), float(aa));
                glUniform1f(glGetUniformLocation(maskProgram, "uMode"), 0);
                glUniform1f(glGetUniformLocation(maskProgram, "uFadeAmt"), tile.fade.amount);
                glDisable(GL_ALPHA_TEST);
                glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                glClear(GL_COLOR_BUFFER_BIT);
                glColor4f(1, 1, 1, 1);
                glBegin(GL_QUADS);
                for (unsigned corner = 0; corner < tile.vertexCount; ++corner) {
                    glTexCoord2fv(tile.vertices[corner].uv);
                    glVertex2fv(tile.vertices[corner].position);
                }
                glEnd();
                raster.enabled[static_cast<std::size_t>(Capability::AlphaTest)] = false;
                Require(session.Render(frame, error), error);
                Require(effects.DrawMapTexture(tile, raster) == CSE_RADAR_OK, "generated mask");
                ComparePixels(session, "generated-tone-" + std::to_string(tone) + "-aa-" +
                    std::to_string(aa) + "-shift-" + std::to_string(shift));
                ++cases;
            }
        }
        Require(effects.ReleaseImage(generated) == CSE_RADAR_OK, "generated release");
    }
    glDeleteProgram(maskProgram);
    glDeleteTextures(1, &glAtlas);
    glDeleteProgram(program);
    glDeleteRenderbuffersEXT(1, &color);
    glDeleteFramebuffersEXT(1, &target);
    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    std::cout << cases << " actual GL/Metal radar cases passed\n";
}
