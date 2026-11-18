#define GL_GLEXT_PROTOTYPES
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include "metal_session.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace cse::render;
constexpr int Side = 64;
const std::array<float, 4> Background{{0.19f, 0.31f, 0.47f, 0.67f}};

void Require(bool result, const std::string& message) {
    if (!result) {
        std::cerr << message << " SDL: " << SDL_GetError() << '\n';
        std::exit(2);
    }
}

DrawPacket Quad() {
    DrawPacket draw;
    draw.primitive = Primitive::Quads;
    draw.raster.viewport = {{0, 0, Side, Side}};
    draw.vertices.resize(4);
    draw.vertices[0].position = {{-1, -1, -0.6f, 1}};
    draw.vertices[1].position = {{1, -1, -0.6f, 1}};
    draw.vertices[2].position = {{1, 1, -0.6f, 1}};
    draw.vertices[3].position = {{-1, 1, -0.6f, 1}};
    for (std::size_t index = 0; index < draw.vertices.size(); ++index) {
        draw.vertices[index].color = {{0.81f, 0.61f, 0.41f, 0.51f}};
        draw.vertices[index].texcoords[0] = {{(index == 1 || index == 2) ? 1.f : 0.f,
            index >= 2 ? 1.f : 0.f, 0, 1}};
    }
    for (auto& matrix : draw.textureMatrices) {
        matrix = Matrix4::Identity();
    }
    return draw;
}

GLenum Blend(BlendFactor value) {
    constexpr GLenum factors[]{GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR,
        GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA,
        GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR};
    return factors[static_cast<unsigned>(value)];
}

GLenum Format(InternalFormat value) {
    switch (value) {
    case InternalFormat::RGB: return GL_RGB;
    case InternalFormat::RGBA: return GL_RGBA;
    case InternalFormat::Luminance: return GL_LUMINANCE;
    case InternalFormat::Alpha: return GL_ALPHA;
    case InternalFormat::Intensity: return GL_INTENSITY;
    default: std::abort();
    }
}

void DrawGL(const DrawPacket& draw) {
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(Background[0], Background[1], Background[2], Background[3]);
    glClearDepth(1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(draw.raster.viewport[0], draw.raster.viewport[1], draw.raster.viewport[2], draw.raster.viewport[3]);
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(draw.projection.values.data());
    glMatrixMode(GL_TEXTURE);
    glLoadMatrixf(draw.textureMatrices[0].values.data());
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(draw.modelView.values.data());
    glDisable(GL_DITHER);
    constexpr GLenum capabilities[]{GL_BLEND, GL_DEPTH_TEST, GL_ALPHA_TEST, GL_CULL_FACE, GL_SCISSOR_TEST, GL_FOG};
    for (std::size_t index = 0; index < std::size(capabilities); ++index) {
        if (draw.raster.enabled[index]) {
            glEnable(capabilities[index]);
        } else {
            glDisable(capabilities[index]);
        }
    }
    glBlendFunc(Blend(draw.raster.blendSource), Blend(draw.raster.blendDestination));
    glAlphaFunc(GL_NEVER + static_cast<unsigned>(draw.raster.alphaFunction), draw.raster.alphaReference);
    glDepthFunc(GL_NEVER + static_cast<unsigned>(draw.raster.depthFunction));
    glDepthRange(draw.raster.depthRange[0], draw.raster.depthRange[1]);
    glDepthMask(draw.raster.depthWrite);
    glColorMask(draw.raster.colorWrite[0], draw.raster.colorWrite[1], draw.raster.colorWrite[2],
        draw.raster.colorWrite[3]);
    constexpr GLenum fogModes[]{GL_EXP, GL_EXP2, GL_LINEAR};
    glFogi(GL_FOG_MODE, fogModes[static_cast<unsigned>(draw.raster.fogMode)]);
    glFogfv(GL_FOG_COLOR, draw.raster.fogColor.data());
    glFogf(GL_FOG_DENSITY, draw.raster.fogDensity);
    glFogf(GL_FOG_START, draw.raster.fogStart);
    glFogf(GL_FOG_END, draw.raster.fogEnd);
    glHint(GL_FOG_HINT, GL_NICEST);
    glShadeModel(draw.shade == Shade::Flat ? GL_FLAT : GL_SMOOTH);
    GLuint texture = 0;
    if (draw.textures[0].enabled2D) {
        const auto& object = *draw.resources[0].texture2D;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        constexpr GLenum filters[]{GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_NEAREST,
            GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR};
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filters[static_cast<unsigned>(object.sampler.minification)]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
            object.sampler.magnification == Filter::Nearest ? GL_NEAREST : GL_LINEAR);
        constexpr GLenum wraps[]{GL_REPEAT, GL_CLAMP, GL_CLAMP_TO_EDGE};
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wraps[static_cast<unsigned>(object.sampler.wrapS)]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wraps[static_cast<unsigned>(object.sampler.wrapT)]);
        for (const auto& level : object.levels) {
            const auto& mip = *level.second;
            glTexImage2D(GL_TEXTURE_2D, level.first, Format(mip.internal), mip.width, mip.height, 0, GL_RGBA,
                GL_UNSIGNED_BYTE, mip.pixels.data());
        }
        constexpr GLenum modes[]{GL_MODULATE, GL_REPLACE, GL_COMBINE};
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, modes[static_cast<unsigned>(draw.textures[0].environment)]);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
        glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, draw.textures[0].rgbScale);
        glEnable(GL_TEXTURE_2D);
    } else {
        glDisable(GL_TEXTURE_2D);
    }
    constexpr GLenum primitives[]{GL_TRIANGLES, GL_TRIANGLE_FAN, GL_QUADS, GL_POLYGON,
        GL_LINES, GL_TRIANGLE_STRIP, GL_QUAD_STRIP, GL_LINE_LOOP};
    glBegin(primitives[static_cast<unsigned>(draw.primitive)]);
    for (const auto& vertex : draw.vertices) {
        glColor4fv(vertex.color.data());
        glTexCoord4fv(vertex.texcoords[0].data());
        glVertex4fv(vertex.position.data());
    }
    glEnd();
    if (texture) {
        glDeleteTextures(1, &texture);
    }
    Require(glGetError() == GL_NO_ERROR, "GL draw error");
}

int main() {
    Require(SDL_Init(SDL_INIT_VIDEO), "SDL init");
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);
    auto* window = SDL_CreateWindow("Legacy pixel qualification", Side, Side, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    Require(window != nullptr, "GL window");
    auto context = SDL_GL_CreateContext(window);
    Require(context != nullptr, "GL context");
    std::cerr << "GL oracle: " << glGetString(GL_RENDERER) << '\n';
    GLuint framebuffer = 0;
    GLuint color = 0;
    GLuint depth = 0;
    glGenFramebuffersEXT(1, &framebuffer);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer);
    glGenRenderbuffersEXT(1, &color);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, color);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_RGBA8, Side, Side);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, color);
    glGenRenderbuffersEXT(1, &depth);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, depth);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24, Side, Side);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, depth);
    Require(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT, "GL target");
    MetalSession metal;
    std::string error;
    Require(metal.Initialize(nullptr, true, error), error);
    Require(metal.Resize(Side, Side, 1, error), error);
    std::vector<unsigned char> reference(Side * Side * 4);
    std::vector<unsigned char> candidate(reference.size());
    auto compare = [&](const std::string& name, const DrawPacket& draw) {
        DrawGL(draw);
        glReadPixels(0, 0, Side, Side, GL_RGBA, GL_UNSIGNED_BYTE, reference.data());
        Require(glGetError() == GL_NO_ERROR, "GL readback");
        FrameSnapshot frame;
        frame.info.width = Side;
        frame.info.height = Side;
        FrameEvent clear;
        clear.kind = FrameEventKind::Clear;
        clear.clear.mask = kClearColor | kClearDepth;
        clear.clear.color = Background;
        frame.events.push_back(clear);
        frame.draws.push_back(draw);
        frame.events.emplace_back();
        Require(metal.Render(frame, error), error);
        Require(metal.ReadPixels(0, 0, Side, Side, 4, 1, candidate.data(), error), error);
        int maximum = 0;
        unsigned differing = 0;
        unsigned total = 0;
        for (std::size_t index = 0; index < reference.size(); ++index) {
            const int delta = std::abs(static_cast<int>(reference[index]) - candidate[index]);
            maximum = std::max(maximum, delta);
            differing += delta > 1;
            total += delta;
        }
        std::cout << "{\"case\":\"" << name << "\",\"max_delta\":" << maximum
            << ",\"components_over_1\":" << differing << ",\"mean_delta\":"
            << static_cast<double>(total) / reference.size() << "}\n";
        Require(maximum <= 1, name + " exceeds one-byte GL/Metal tolerance");
    };
    compare("solid", Quad());
    {
        constexpr int textureSide = 2;
        TextureStore textures;
        LegacyState state;
        TextureUpload upload;
        upload.width = textureSide;
        upload.height = textureSide;
        upload.internal = InternalFormat::RGBA;
        upload.allocateOnly = true;
        constexpr auto target = TextureTarget::Texture2D;
        Require(textures.Upload(state, target, upload, {}, false) == Status::Ok, "allocate empty texture");
        Require(textures.Parameter(state, target, TextureParameter::MinFilter,
            static_cast<double>(Filter::Nearest)) == Status::Ok, "select base level");
        upload.allocateOnly = false;
        upload.height = 1;
        upload.bytes = {255, 0, 0, 255, 0, 255, 0, 255};
        Require(textures.Upload(state, target, upload, {}, true) == Status::Ok, "upload first row");
        upload.y = 1;
        upload.bytes = {0, 0, 255, 255, 255, 255, 0, 255};
        Require(textures.Upload(state, target, upload, {}, true) == Status::Ok, "upload second row");
        auto draw = Quad();
        draw.textures[0].enabled2D = true;
        draw.resources[0].texture2D = textures.Bound(state, target);
        compare("texture-filled-by-subimages", draw);
        upload.y = 0;
        upload.width = 0;
        Require(textures.Upload(state, target, upload, {}, false) == Status::Ok, "empty texture level");
        draw.resources[0].texture2D = textures.Bound(state, target);
        compare("incomplete-zero-width", draw);
    }
    constexpr std::array<int, 4> viewportCases[]{
        {{0, -1, Side, Side + 1}}, {{-1, 0, Side + 1, Side}},
        {{4, 5, 80, 60}}, {{-8, -5, 100, 86}}, {{Side, 0, Side, Side}}, {{0, 0, 0, 0}}
    };
    for (unsigned index = 0; index < std::size(viewportCases); ++index) {
        auto draw = Quad();
        draw.raster.viewport = viewportCases[index];
        for (auto& vertex : draw.vertices) {
            vertex.color = {{(vertex.position[0] + 1) * 0.5f,
                (vertex.position[1] + 1) * 0.5f, 0.3f, 0.6f}};
        }
        compare("viewport-gradient-" + std::to_string(index), draw);
        auto image = std::make_shared<TextureImage>();
        image->defined = true;
        image->width = image->height = 2;
        image->internal = InternalFormat::RGBA;
        image->pixels = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
        auto texture = std::make_shared<TextureObject>();
        texture->levels[0] = image;
        texture->sampler.minification = Filter::Linear;
        texture->sampler.magnification = Filter::Linear;
        texture->sampler.wrapS = texture->sampler.wrapT = Wrap::ClampToEdge;
        draw.textures[0].enabled2D = true;
        draw.resources[0].texture2D = texture;
        compare("viewport-texture-" + std::to_string(index), draw);
    }
    {
        auto draw = Quad();
        draw.shade = Shade::Flat;
        draw.vertices[0].color = {{1, 0, 0, 1}};
        draw.vertices[1].color = {{0, 1, 0, 1}};
        draw.vertices[2].color = {{0, 0, 1, 1}};
        draw.vertices[3].color = {{1, 1, 0, 1}};
        compare("flat-quad", draw);
        draw.primitive = Primitive::Polygon;
        compare("flat-polygon", draw);
        draw.primitive = Primitive::TriangleFan;
        compare("flat-fan", draw);
    }
    for (unsigned mode = 0; mode < 3; ++mode) {
        auto draw = Quad();
        draw.raster.enabled[5] = true;
        draw.raster.fogMode = static_cast<FogMode>(mode);
        draw.raster.fogColor = {{0.1f, 0.3f, 0.7f, 0.2f}};
        draw.raster.fogDensity = 1.7f;
        draw.raster.fogStart = 0.1f;
        draw.raster.fogEnd = 0.9f;
        compare("fog-" + std::to_string(mode), draw);
        draw.vertices[0].position[2] = draw.vertices[3].position[2] = -0.1f;
        draw.vertices[1].position[2] = draw.vertices[2].position[2] = -0.9f;
        compare("fog-interpolated-" + std::to_string(mode), draw);
    }
    for (unsigned mode = 0; mode < 8; ++mode) {
        auto draw = Quad();
        draw.raster.enabled[2] = true;
        draw.raster.alphaFunction = static_cast<Compare>(mode);
        draw.raster.alphaReference = 0.6f;
        compare("alpha-" + std::to_string(mode), draw);
    }
    for (unsigned factor = 0; factor < 10; ++factor) {
        auto draw = Quad();
        draw.raster.enabled[0] = true;
        draw.raster.blendSource = static_cast<BlendFactor>(factor);
        draw.raster.blendDestination = BlendFactor::InverseSourceAlpha;
        compare("blend-" + std::to_string(factor), draw);
    }
    constexpr InternalFormat formats[]{InternalFormat::RGBA, InternalFormat::RGB, InternalFormat::Luminance,
        InternalFormat::Alpha, InternalFormat::Intensity};
    for (unsigned format = 0; format < 5; ++format) {
        for (unsigned environment = 0; environment < 3; ++environment) {
            for (unsigned wrap = 0; wrap < 3; ++wrap) {
                auto draw = Quad();
                auto image = std::make_shared<TextureImage>();
                image->defined = true;
                image->width = 2;
                image->height = 2;
                image->internal = formats[format];
                image->pixels = {204, 102, 51, 64, 51, 204, 102, 128, 102, 51, 204, 192, 128, 128, 128, 255};
                auto texture = std::make_shared<TextureObject>();
                texture->levels[0] = image;
                texture->sampler.minification = Filter::Linear;
                texture->sampler.magnification = Filter::Linear;
                texture->sampler.wrapS = static_cast<Wrap>(wrap);
                texture->sampler.wrapT = static_cast<Wrap>(wrap);
                draw.textures[0].enabled2D = true;
                draw.textures[0].environment = static_cast<EnvironmentMode>(environment);
                draw.textures[0].rgbScale = environment == 2 ? 2 : 1;
                draw.resources[0].texture2D = texture;
                compare("texture-f" + std::to_string(format) + "-env" + std::to_string(environment)
                    + "-wrap" + std::to_string(wrap), draw);
            }
        }
    }
    {
        auto draw = Quad();
        auto texture = std::make_shared<TextureObject>();
        auto base = std::make_shared<TextureImage>();
        base->defined = true;
        base->width = base->height = 2;
        base->internal = InternalFormat::RGBA;
        base->pixels = {255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255};
        texture->levels[0] = base;
        texture->sampler.minification = Filter::NearestMipmapNearest;
        texture->sampler.magnification = Filter::Linear;
        draw.textures[0].enabled2D = true;
        draw.resources[0].texture2D = texture;
        compare("incomplete-missing-mip", draw);
        texture = std::make_shared<TextureObject>();
        draw.resources[0].texture2D = texture;
        compare("incomplete-no-base", draw);
    }
    for (unsigned filter = 0; filter < 6; ++filter) {
        for (unsigned wrap = 0; wrap < 3; ++wrap) {
            auto draw = Quad();
            auto texture = std::make_shared<TextureObject>();
            for (int level = 0; level <= 6; ++level) {
                auto mip = std::make_shared<TextureImage>();
                mip->defined = true;
                mip->width = mip->height = 64 >> level;
                mip->internal = InternalFormat::RGBA;
                for (int pixel = 0; pixel < mip->width * mip->height; ++pixel) {
                    const bool alternate = ((pixel / mip->width) + pixel) % 2;
                    mip->pixels.insert(mip->pixels.end(), {static_cast<unsigned char>(level * 40),
                        static_cast<unsigned char>(alternate ? 230 : 50), 150, 210});
                }
                texture->levels[level] = mip;
            }
            texture->sampler.minification = static_cast<Filter>(filter);
            texture->sampler.magnification = Filter::Linear;
            texture->sampler.wrapS = texture->sampler.wrapT = static_cast<Wrap>(wrap);
            draw.textures[0].enabled2D = true;
            draw.resources[0].texture2D = texture;
            for (float scale : {0.6f, 0.99f, 1.01f, 1.2f, 1.4f, 1.42f, 1.43f, 1.44f, 1.45f, 1.46f, 1.5f, 3.1f}) {
                for (auto& vertex : draw.vertices) {
                    vertex.texcoords[0][0] = (vertex.position[0] + 1) * 0.5f * scale;
                    vertex.texcoords[0][1] = (vertex.position[1] + 1) * 0.5f * scale;
                }
                compare("mip-f" + std::to_string(filter) + "-wrap" + std::to_string(wrap)
                    + "-scale" + std::to_string(scale), draw);
            }
        }
    }
    metal.Shutdown();
    glDeleteRenderbuffersEXT(1, &depth);
    glDeleteRenderbuffersEXT(1, &color);
    glDeleteFramebuffersEXT(1, &framebuffer);
    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
