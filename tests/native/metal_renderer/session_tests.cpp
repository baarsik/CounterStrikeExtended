#include "metal_session.h"
#include <SDL3/SDL.h>
#include <cstdlib>
#include <dlfcn.h>
#include <iostream>
#include <vector>
using namespace cse::render;

#define CHECK(x) do { \
    if (!(x)) { \
        std::cerr << __LINE__ << ": " #x << " " << error << "\n"; \
        std::exit(1); \
    } \
} while (false)

namespace {
constexpr int kTargetSize = 16;
unsigned sceneDrawCalls = 0;
// Narrow link seam around presentation allocation and command retirement. All successful calls use real SDL.
struct PresentationProbe {
    bool active = false;
    bool mismatch = false;
    unsigned allocationsUntilFailure = 0;
    unsigned acquisitions = 0;
    unsigned cancellations = 0;
    unsigned submissions = 0;
} presentationProbe;

template<typename Function> Function Original(const char* name) {
    auto function = reinterpret_cast<Function>(dlsym(RTLD_NEXT, name));
    if (!function) {
        std::cerr << "Cannot resolve real SDL function " << name << "\n";
        std::exit(1);
    }
    return function;
}
}

extern "C" void SDL_DrawGPUPrimitives(SDL_GPURenderPass* pass, Uint32 vertices, Uint32 instances,
    Uint32 firstVertex, Uint32 firstInstance) {
    ++sceneDrawCalls;
    Original<decltype(&SDL_DrawGPUPrimitives)>("SDL_DrawGPUPrimitives")(
        pass, vertices, instances, firstVertex, firstInstance);
}

extern "C" SDL_GPUTexture* SDL_CreateGPUTexture(SDL_GPUDevice* device, const SDL_GPUTextureCreateInfo* info) {
    if (presentationProbe.active && presentationProbe.allocationsUntilFailure > 0 &&
        --presentationProbe.allocationsUntilFailure == 0) {
        SDL_SetError("fixture final-color allocation failure");
        return nullptr;
    }
    return Original<decltype(&SDL_CreateGPUTexture)>("SDL_CreateGPUTexture")(device, info);
}

extern "C" bool SDL_WaitAndAcquireGPUSwapchainTexture(SDL_GPUCommandBuffer* command, SDL_Window* window,
    SDL_GPUTexture** texture, Uint32* width, Uint32* height) {
    const bool result = Original<decltype(&SDL_WaitAndAcquireGPUSwapchainTexture)>(
        "SDL_WaitAndAcquireGPUSwapchainTexture")(command, window, texture, width, height);
    if (result && *texture && presentationProbe.active) {
        ++presentationProbe.acquisitions;
        if (presentationProbe.mismatch) {
            ++*width; // Model a drawable-size change between preflight and acquisition; no blit may follow failure.
        }
    }
    return result;
}

extern "C" bool SDL_CancelGPUCommandBuffer(SDL_GPUCommandBuffer* command) {
    if (presentationProbe.active) {
        ++presentationProbe.cancellations;
    }
    return Original<decltype(&SDL_CancelGPUCommandBuffer)>("SDL_CancelGPUCommandBuffer")(command);
}

extern "C" bool SDL_SubmitGPUCommandBuffer(SDL_GPUCommandBuffer* command) {
    if (presentationProbe.active) {
        ++presentationProbe.submissions;
    }
    return Original<decltype(&SDL_SubmitGPUCommandBuffer)>("SDL_SubmitGPUCommandBuffer")(command);
}

DrawPacket Solid(float red, float green, float blue) {
    DrawPacket draw;
    draw.primitive = Primitive::Quads;
    draw.raster.viewport = {{0, 0, kTargetSize, kTargetSize}};
    draw.vertices.resize(4);
    draw.vertices[0].position = {{-1, -1, 0, 1}};
    draw.vertices[1].position = {{1, -1, 0, 1}};
    draw.vertices[2].position = {{1, 1, 0, 1}};
    draw.vertices[3].position = {{-1, 1, 0, 1}};
    for (auto& vertex : draw.vertices) {
        vertex.color = {{red, green, blue, 1}};
    }
    for (auto& matrix : draw.textureMatrices) {
        matrix = Matrix4::Identity();
    }
    return draw;
}

void ConsecutiveDraws(MetalSession& session, std::string& error) {
    constexpr int kHalfSize = kTargetSize / 2;
    constexpr int kLeftSample = kTargetSize / 4;
    constexpr int kRightSample = kTargetSize - kLeftSample;
    constexpr unsigned char kOpaqueChannel = 255;
    FrameSnapshot frame;
    frame.info.width = kTargetSize;
    frame.info.height = kTargetSize;
    frame.draws = {Solid(1, 0, 0), Solid(0, 1, 0)};
    FrameEvent first;
    FrameEvent second;
    second.drawIndex = 1;
    frame.events = {first, second};
    for (auto& vertex : frame.draws[0].vertices) {
        vertex.position[0] = (vertex.position[0] - 1) * 0.5f;
    }
    for (auto& vertex : frame.draws[1].vertices) {
        vertex.position[0] = (vertex.position[0] + 1) * 0.5f;
    }
    sceneDrawCalls = 0;
    CHECK(session.Render(frame, error));
    CHECK(sceneDrawCalls == 1);
    std::array<unsigned char, 4> pixel{};
    CHECK(session.ReadPixels(kLeftSample, kHalfSize, 1, 1, 4, 1, pixel.data(), error));
    CHECK(pixel[0] == kOpaqueChannel && pixel[1] == 0);
    CHECK(session.ReadPixels(kRightSample, kHalfSize, 1, 1, 4, 1, pixel.data(), error));
    CHECK(pixel[0] == 0 && pixel[1] == kOpaqueChannel);
    // Uniform changes must break a batch even when the graphics pipeline is identical.
    frame.draws = {Solid(1, 0, 0), Solid(0, 1, 0)};
    frame.draws[1].raster.enabled[static_cast<std::size_t>(Capability::AlphaTest)] = true;
    frame.draws[1].raster.alphaFunction = Compare::Greater;
    frame.draws[1].raster.alphaReference = 0.5f;
    for (auto& vertex : frame.draws[1].vertices) {
        vertex.color[3] = 0.25f;
    }
    sceneDrawCalls = 0;
    CHECK(session.Render(frame, error));
    CHECK(sceneDrawCalls == 2);
    CHECK(session.ReadPixels(kHalfSize, kHalfSize, 1, 1, 4, 1, pixel.data(), error));
    CHECK(pixel[0] == kOpaqueChannel && pixel[1] == 0);
    // Projection and dynamic scissor state are independent of the pipeline cache key too.
    for (bool scissor : {false, true}) {
        frame.draws[1] = Solid(0, 1, 0);
        if (scissor) {
            frame.draws[1].raster.enabled[static_cast<std::size_t>(Capability::ScissorTest)] = true;
            frame.draws[1].raster.scissor = {{kHalfSize, 0, kHalfSize, kTargetSize}};
        } else {
            frame.draws[1].projection.values[12] = 1;
        }
        sceneDrawCalls = 0;
        CHECK(session.Render(frame, error));
        CHECK(sceneDrawCalls == 2);
        CHECK(session.ReadPixels(kLeftSample, kHalfSize, 1, 1, 4, 1, pixel.data(), error));
        CHECK(pixel[0] == kOpaqueChannel && pixel[1] == 0);
        CHECK(session.ReadPixels(kRightSample, kHalfSize, 1, 1, 4, 1, pixel.data(), error));
        CHECK(pixel[0] == 0 && pixel[1] == kOpaqueChannel);
    }
    // Equal uniforms do not permit merging distinct texture bindings.
    for (std::size_t index = 0; index < frame.draws.size(); ++index) {
        auto image = std::make_shared<TextureImage>();
        image->width = 1;
        image->height = 1;
        image->defined = true;
        image->pixels.assign(pixel.size(), 0);
        image->pixels[index] = kOpaqueChannel;
        image->pixels.back() = kOpaqueChannel;
        auto object = std::make_shared<TextureObject>();
        object->levels[0] = image;
        frame.draws[index] = Solid(1, 1, 1);
        frame.draws[index].textures[0].enabled2D = true;
        frame.draws[index].resources[0].texture2D = object;
    }
    sceneDrawCalls = 0;
    CHECK(session.Render(frame, error));
    CHECK(sceneDrawCalls == 2);
    CHECK(session.ReadPixels(kHalfSize, kHalfSize, 1, 1, 4, 1, pixel.data(), error));
    CHECK(pixel[0] == 0 && pixel[1] == kOpaqueChannel);
    // Merging translucent primitives must retain their original compositing order.
    FrameEvent clear;
    clear.kind = FrameEventKind::Clear;
    clear.clear.mask = kClearColor;
    clear.clear.color = {{0, 0, 1, 1}};
    frame.events = {clear, first, second};
    frame.draws = {Solid(1, 0, 0), Solid(0, 1, 0)};
    for (auto& draw : frame.draws) {
        draw.raster.enabled[static_cast<std::size_t>(Capability::Blend)] = true;
        draw.raster.blendSource = BlendFactor::SourceAlpha;
        draw.raster.blendDestination = BlendFactor::InverseSourceAlpha;
        for (auto& vertex : draw.vertices) {
            vertex.color[3] = 0.5f;
        }
    }
    sceneDrawCalls = 0;
    CHECK(session.Render(frame, error));
    CHECK(sceneDrawCalls == 2);
    CHECK(session.ReadPixels(kHalfSize, kHalfSize, 1, 1, 4, 1, pixel.data(), error));
    constexpr int kQuarterChannel = 64;
    constexpr int kHalfChannel = 128;
    CHECK(std::abs(int(pixel[0]) - kQuarterChannel) <= 1);
    CHECK(std::abs(int(pixel[1]) - kHalfChannel) <= 1);
    CHECK(std::abs(int(pixel[2]) - kQuarterChannel) <= 1);
}

int main() {
    std::string error;
    CHECK(SDL_Init(SDL_INIT_VIDEO));
    SDL_SetAssertionHandler([](const SDL_AssertData* assertion, void*) {
        std::cerr << "SDL assertion: " << assertion->condition << "\n";
        return SDL_ASSERTION_ABORT;
    }, nullptr);
    MetalSession session;
    CHECK(session.Initialize(nullptr, true, error));
    CHECK(session.Resize(16, 16, 1, error));
    ConsecutiveDraws(session, error);
    FrameSnapshot frame;
    frame.info.width = 16;
    frame.info.height = 16;
    FrameEvent clear;
    clear.kind = FrameEventKind::Clear;
    clear.clear.mask = kClearColor | kClearDepth;
    clear.clear.color = {{0, 0, 0, 1}};
    frame.events.push_back(clear);
    frame.draws.push_back(Solid(1, 0, 0));
    FrameEvent draw;
    frame.events.push_back(draw);
    clear.clear.color = {{0, 1, 0, 1}};
    clear.clear.mask = kClearColor;
    clear.clear.scissorEnabled = true;
    clear.clear.scissor = {{0, 0, 16, 8}};
    frame.events.push_back(clear);
    CHECK(session.Render(frame, error));
    std::vector<unsigned char> rgba(16 * 16 * 4);
    CHECK(session.ReadPixels(0, 0, 16, 16, 4, 4, rgba.data(), error));
    CHECK(rgba[0] == 0 && rgba[1] == 255 && rgba[2] == 0);
    CHECK(rgba[16 * 15 * 4] == 255 && rgba[16 * 15 * 4 + 1] == 0);
    // Incremental calls preserve target and readback returns RGB with pack padding untouched.
    std::vector<unsigned char> rgb(24, 91);
    CHECK(session.ReadPixels(0, 0, 3, 2, 3, 4, rgb.data(), error));
    CHECK(rgb[0] == 0 && rgb[1] == 255 && rgb[9] == 91 && rgb[12 + 1] == 255);
    frame.events.clear();
    frame.draws[0] = Solid(0, 0, 1);
    frame.events.push_back(draw);
    CHECK(session.Render(frame, error));
    CHECK(session.ReadPixels(0, 0, 1, 1, 4, 1, rgba.data(), error));
    CHECK(rgba[0] == 0 && rgba[1] == 0 && rgba[2] == 255);
    CHECK(!session.ReadPixels(-1, 0, 1, 1, 4, 1, rgba.data(), error));
    CHECK(session.Finish(error));
    CHECK(session.Resize(16, 16, 4, error));
    CHECK(session.Render(frame, error));
    CHECK(session.ReadPixels(0, 0, 1, 1, 4, 1, rgba.data(), error));
    CHECK(rgba[2] == 255);
    CHECK(session.Resize(16, 16, 1, error));
    // Incomplete fixed-function units are disabled, including REPLACE (white is not a substitute).
    for (unsigned kind = 0; kind < 3; ++kind) {
        auto incomplete = std::make_shared<TextureObject>();
        if (kind == 2) {
            auto base = std::make_shared<TextureImage>();
            base->defined = true;
            base->width = 2;
            base->height = 2;
            base->internal = InternalFormat::RGBA;
            base->pixels.assign(16, 255);
            incomplete->levels[0] = base;
            incomplete->sampler.minification = Filter::NearestMipmapNearest;
        }
        frame.draws[0] = Solid(1, 0, 1);
        frame.draws[0].textures[0].enabled2D = true;
        frame.draws[0].textures[0].environment = EnvironmentMode::Replace;
        if (kind != 0) {
            frame.draws[0].resources[0].texture2D = incomplete;
        }
        CHECK(session.Render(frame, error));
        CHECK(session.ReadPixels(0, 0, 1, 1, 4, 1, rgba.data(), error));
        CHECK(rgba[0] == 255 && rgba[1] == 0 && rgba[2] == 255 && rgba[3] == 255);
    }
    // Explicit border anisotropy integrates four distinct nearest taps at source columns18/22/26/30.
    // The center-only sample at column24 is blue, whereas two of the four taps are red.
    for (unsigned anisotropy : {1u, 4u}) {
        auto source = std::make_shared<TextureImage>();
        source->defined = true;
        source->width = 64;
        source->height = 64;
        source->internal = InternalFormat::RGBA;
        for (int y = 0; y < source->height; ++y) {
            for (int x = 0; x < source->width; ++x) {
                const bool red = x == 18 || x == 22;
                source->pixels.insert(source->pixels.end(), {static_cast<unsigned char>(red ? 255 : 0), 0,
                    static_cast<unsigned char>(red ? 0 : 255), 255});
            }
        }
        auto anisotropic = std::make_shared<TextureObject>();
        anisotropic->levels[0] = source;
        anisotropic->sampler.minification = Filter::Nearest;
        anisotropic->sampler.magnification = Filter::Nearest;
        anisotropic->sampler.wrapS = Wrap::Clamp;
        anisotropic->sampler.wrapT = Wrap::Clamp;
        anisotropic->sampler.requestedAnisotropy = float(anisotropy);
        frame.draws[0] = Solid(1, 1, 1);
        auto& packet = frame.draws[0];
        packet.textures[0].enabled2D = true;
        packet.resources[0].texture2D = anisotropic;
        for (auto& vertex : packet.vertices) {
            vertex.texcoords[0] = {{(vertex.position[0] + 1) * 2, (vertex.position[1] + 1) * 0.5f, 0, 1}};
        }
        CHECK(session.Render(frame, error));
        CHECK(session.ReadPixels(1, 8, 1, 1, 4, 1, rgba.data(), error));
        CHECK(rgba[0] == (anisotropy == 1 ? 0 : 128));
        CHECK(rgba[2] == (anisotropy == 1 ? 255 : 128));
    }
    // Oversized requests must sample the same pixels as the supported maximum in
    // both the device sampler and the explicit border-sampling shader path.
    for (Wrap wrap : {Wrap::ClampToEdge, Wrap::Clamp}) {
        std::vector<unsigned char> maximumPixels;
        for (float requested : {16.0f, 32.0f}) {
            auto anisotropic = std::make_shared<TextureObject>(*frame.draws[0].resources[0].texture2D);
            anisotropic->sampler.wrapS = wrap;
            anisotropic->sampler.wrapT = wrap;
            anisotropic->sampler.requestedAnisotropy = requested;
            frame.draws[0].resources[0].texture2D = anisotropic;
            CHECK(session.Render(frame, error));
            CHECK(session.ReadPixels(0, 0, 16, 16, 4, 4, rgba.data(), error));
            if (requested == 16) {
                maximumPixels = rgba;
            } else {
                CHECK(rgba == maximumPixels);
            }
        }
    }
    // Owned versions remain immutable while successive command buffers are pending.
    auto image = std::make_shared<TextureImage>();
    image->defined = true;
    image->width = 2;
    image->height = 2;
    image->internal = InternalFormat::RGBA;
    image->pixels = {255, 0, 0, 255, 255, 0, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255};
    auto texture = std::make_shared<TextureObject>();
    texture->sampler.minification = Filter::Nearest;
    texture->sampler.magnification = Filter::Nearest;
    texture->sampler.wrapS = Wrap::ClampToEdge;
    texture->sampler.wrapT = Wrap::ClampToEdge;
    texture->levels[0] = image;
    auto textured = Solid(1, 1, 1);
    textured.textures[0].enabled2D = true;
    textured.textures[0].environment = EnvironmentMode::Replace;
    textured.resources[0].texture2D = texture;
    textured.vertices[0].texcoords[0] = {{0, 0, 0, 1}};
    textured.vertices[1].texcoords[0] = {{1, 0, 0, 1}};
    textured.vertices[2].texcoords[0] = {{1, 1, 0, 1}};
    textured.vertices[3].texcoords[0] = {{0, 1, 0, 1}};
    frame.draws[0] = textured;
    CHECK(session.Render(frame, error));
    CHECK(session.ReadPixels(0, 0, 16, 16, 4, 1, rgba.data(), error));
    CHECK(rgba[0] == 255 && rgba[1] == 0);
    CHECK(rgba[16 * 15 * 4] == 0 && rgba[16 * 15 * 4 + 1] == 255);
    auto edited = std::make_shared<TextureImage>(*image);
    edited->pixels.assign(16, 255);
    auto version = std::make_shared<TextureObject>(*texture);
    version->generation = 2;
    version->levels[0] = edited;
    frame.draws[0].resources[0].texture2D = version;
    CHECK(session.Render(frame, error));
    frame.draws[0] = textured;
    CHECK(session.Render(frame, error));
    CHECK(session.ReadPixels(0, 0, 1, 1, 4, 1, rgba.data(), error));
    CHECK(rgba[0] == 255 && rgba[1] == 0 && rgba[2] == 0);
    // Real TextureStore subimages fork versions retained by already captured packets.
    LegacyState capturedState;
    TextureStore store;
    CHECK(store.Bind(capturedState, TextureTarget::Texture2D, 1) == Status::Ok);
    CHECK(store.Parameter(capturedState, TextureTarget::Texture2D, TextureParameter::MinFilter,
        static_cast<double>(Filter::Nearest)) == Status::Ok);
    TextureUpload upload;
    upload.width = 1;
    upload.height = 1;
    upload.bytes = {255, 0, 0, 255};
    CHECK(store.Upload(capturedState, TextureTarget::Texture2D, upload, {}, false) == Status::Ok);
    auto beforeSubimage = store.Bound(capturedState, TextureTarget::Texture2D);
    upload.bytes = {0, 0, 255, 255};
    CHECK(store.Upload(capturedState, TextureTarget::Texture2D, upload, {}, true) == Status::Ok);
    frame.draws[0] = textured;
    frame.draws[0].resources[0].texture2D = store.Bound(capturedState, TextureTarget::Texture2D);
    CHECK(session.Render(frame, error));
    CHECK(session.ReadPixels(8, 8, 1, 1, 4, 1, rgba.data(), error));
    CHECK(rgba[0] == 0 && rgba[2] == 255);
    frame.draws[0].resources[0].texture2D = beforeSubimage;
    CHECK(session.Render(frame, error));
    CHECK(session.ReadPixels(8, 8, 1, 1, 4, 1, rgba.data(), error));
    CHECK(rgba[0] == 255 && rgba[2] == 0);
    // Clamp-to-border differs from edge clamp even with the engine's default anisotropy16.
    frame.draws[0] = textured;
    auto border = std::make_shared<TextureObject>(*texture);
    border->sampler.minification = Filter::Linear;
    border->sampler.magnification = Filter::Linear;
    border->sampler.requestedAnisotropy = 16;
    border->sampler.wrapS = Wrap::Clamp;
    for (auto& vertex : frame.draws[0].vertices) {
        vertex.texcoords[0] = {{0, 0.25f, 0, 1}};
    }
    frame.draws[0].resources[0].texture2D = border;
    CHECK(session.Render(frame, error));
    CHECK(session.ReadPixels(8, 8, 1, 1, 4, 1, rgba.data(), error));
    CHECK(rgba[0] >= 127 && rgba[0] <= 128 && rgba[1] == 0 && rgba[3] >= 127 && rgba[3] <= 128);
    // Nearest GL_CLAMP selects the final texel at and beyond the upper endpoint.
    auto nearestClamp = std::make_shared<TextureObject>(*texture);
    nearestClamp->sampler.wrapS = Wrap::Clamp;
    nearestClamp->sampler.wrapT = Wrap::Clamp;
    frame.draws[0] = textured;
    frame.draws[0].resources[0].texture2D = nearestClamp;
    for (float endpoint : {1.0f, 2.0f}) {
        for (auto& vertex : frame.draws[0].vertices) {
            vertex.texcoords[0] = {{endpoint, endpoint, 0, 1}};
        }
        CHECK(session.Render(frame, error));
        CHECK(session.ReadPixels(8, 8, 1, 1, 4, 1, rgba.data(), error));
        CHECK(rgba[0] == 0 && rgba[1] == 255 && rgba[3] == 255);
    }
    auto nearestRectangle = std::make_shared<TextureObject>(*nearestClamp);
    nearestRectangle->target = TextureTarget::Rectangle;
    frame.draws[0].textures[0].enabled2D = false;
    frame.draws[0].textures[0].enabledRectangle = true;
    frame.draws[0].resources[0].rectangle = nearestRectangle;
    for (float endpoint : {2.0f, 4.0f}) {
        for (auto& vertex : frame.draws[0].vertices) {
            vertex.texcoords[0] = {{endpoint, endpoint, 0, 1}};
        }
        CHECK(session.Render(frame, error));
        CHECK(session.ReadPixels(8, 8, 1, 1, 4, 1, rgba.data(), error));
        CHECK(rgba[0] == 0 && rgba[1] == 255 && rgba[3] == 255);
    }
    // Linear border filtering cannot introduce components absent from the legacy internal format.
    for (auto mode : {EnvironmentMode::Modulate, EnvironmentMode::Combine}) {
        for (auto format : {InternalFormat::RGB, InternalFormat::Alpha}) {
            auto componentImage = std::make_shared<TextureImage>();
            componentImage->defined = true;
            componentImage->width = 1;
            componentImage->height = 1;
            componentImage->internal = format;
            componentImage->pixels = format == InternalFormat::RGB ?
                std::vector<std::uint8_t>{255, 128, 64, 255} : std::vector<std::uint8_t>{255, 255, 255, 128};
            auto componentTexture = std::make_shared<TextureObject>();
            componentTexture->levels[0] = componentImage;
            componentTexture->sampler.minification = Filter::Linear;
            componentTexture->sampler.magnification = Filter::Linear;
            componentTexture->sampler.wrapS = Wrap::Clamp;
            componentTexture->sampler.wrapT = Wrap::ClampToEdge;
            frame.draws[0] = textured;
            frame.draws[0].textures[0].environment = mode;
            frame.draws[0].resources[0].texture2D = componentTexture;
            for (auto& vertex : frame.draws[0].vertices) {
                vertex.color = {{0.8f, 0.6f, 0.4f, 0.5f}};
                vertex.texcoords[0] = {{0, 0.5f, 0, 1}};
            }
            CHECK(session.Render(frame, error));
            CHECK(session.ReadPixels(8, 8, 1, 1, 4, 1, rgba.data(), error));
            if (format == InternalFormat::RGB) {
                CHECK(rgba[0] == 102 && rgba[1] == 38 && rgba[2] == 13 && rgba[3] == 128);
            } else if (mode == EnvironmentMode::Combine) {
                CHECK(rgba[0] == 0 && rgba[1] == 0 && rgba[2] == 0 && rgba[3] == 32);
            } else {
                CHECK(rgba[0] == 204 && rgba[1] == 153 && rgba[2] == 102 && rgba[3] == 32);
            }
        }
    }
    // A single physical level still uses MIN_NEAREST when its derivatives minify.
    auto singleImage = std::make_shared<TextureImage>();
    singleImage->defined = true;
    singleImage->width = 2;
    singleImage->height = 1;
    singleImage->internal = InternalFormat::RGBA;
    singleImage->pixels = {255, 0, 0, 255, 0, 255, 0, 255};
    auto singleTexture = std::make_shared<TextureObject>();
    singleTexture->levels[0] = singleImage;
    singleTexture->sampler.minification = Filter::Nearest;
    singleTexture->sampler.magnification = Filter::Linear;
    for (auto wrap : {Wrap::Repeat, Wrap::ClampToEdge, Wrap::Clamp}) {
        singleTexture->sampler.wrapS = wrap;
        singleTexture->sampler.wrapT = wrap;
        frame.draws[0] = textured;
        // Cross 32 texels in 16 pixels, centered on u=.4 at pixel (8,8).
        frame.draws[0].resources[0].texture2D = std::make_shared<TextureObject>(*singleTexture);
        for (auto& vertex : frame.draws[0].vertices) {
            vertex.texcoords[0] = {{vertex.position[0] * 8.0f - 0.1f, 0.5f, 0, 1}};
        }
        CHECK(session.Render(frame, error));
        CHECK(session.ReadPixels(8, 8, 1, 1, 4, 1, rgba.data(), error));
        CHECK(rgba[0] == 255 && rgba[1] == 0);
    }
    // R_SetupGL can extend the viewport one pixel below the scene during view-rectangle rounding.
    // Cropping must preserve the original projection/UV mapping, not stretch the visible portion.
    auto viewportImage = std::make_shared<TextureImage>();
    viewportImage->defined = true;
    viewportImage->width = 2;
    viewportImage->height = 2;
    viewportImage->internal = InternalFormat::RGBA;
    viewportImage->pixels = {255, 0, 0, 255, 255, 0, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255};
    auto viewportTexture = std::make_shared<TextureObject>();
    viewportTexture->levels[0] = viewportImage;
    viewportTexture->sampler.minification = Filter::Nearest;
    viewportTexture->sampler.magnification = Filter::Nearest;
    viewportTexture->sampler.wrapS = Wrap::ClampToEdge;
    viewportTexture->sampler.wrapT = Wrap::ClampToEdge;
    frame.draws[0] = textured;
    frame.draws[0].resources[0].texture2D = viewportTexture;
    frame.draws[0].raster.viewport = {{0, -2, 16, 18}};
    for (auto& vertex : frame.draws[0].vertices) {
        vertex.texcoords[0] = {{0.5f, vertex.position[1] * 0.5f + 0.5f, 0, 1}};
    }
    CHECK(session.Render(frame, error));
    CHECK(session.ReadPixels(8, 7, 1, 1, 4, 1, rgba.data(), error));
    CHECK(rgba[0] == 0 && rgba[1] == 255);
    // Smooth color interpolation also detects stretching at the exact one-pixel rounding margin.
    frame.draws[0] = Solid(0, 0, 0);
    frame.draws[0].shade = Shade::Smooth;
    frame.draws[0].raster.shade = Shade::Smooth;
    frame.draws[0].raster.viewport = {{0, -1, 16, 17}};
    for (auto& vertex : frame.draws[0].vertices) {
        vertex.color[1] = vertex.position[1] * 0.5f + 0.5f;
    }
    CHECK(session.Render(frame, error));
    CHECK(session.ReadPixels(8, 7, 1, 1, 4, 1, rgba.data(), error));
    CHECK(rgba[0] == 0 && rgba[1] >= 127 && rgba[1] <= 128);
    frame.draws[0] = textured;
    frame.draws[0].resources[0].texture2D = viewportTexture;
    // Horizontal overflow also retains the source texture scale.
    frame.draws[0].raster.viewport = {{4, 0, 16, 16}};
    for (auto& vertex : frame.draws[0].vertices) {
        vertex.texcoords[0] = {{0.5f, vertex.position[0] * 0.5f + 0.5f, 0, 1}};
    }
    CHECK(session.Render(frame, error));
    CHECK(session.ReadPixels(10, 8, 1, 1, 4, 1, rgba.data(), error));
    CHECK(rgba[0] == 255 && rgba[1] == 0);
    frame.draws[0] = Solid(0, 1, 0);
    frame.draws[0].raster.viewport = {{16, 0, 16, 16}};
    CHECK(session.Render(frame, error));
    CHECK(session.ReadPixels(10, 8, 1, 1, 4, 1, rgba.data(), error));
    CHECK(rgba[0] == 255 && rgba[1] == 0);
    // Reject events with no result destination rather than accepting lost readback bytes.
    FrameEvent readback;
    readback.kind = FrameEventKind::Readback;
    frame.events.push_back(readback);
    CHECK(!session.Render(frame, error));
    frame.events.pop_back();
    clear.clear.mask = kClearStencil;
    frame.events.push_back(clear);
    CHECK(!session.Render(frame, error));
    frame.events.pop_back();
    // Mips select owned level1, rather than silently falling back to the red base image.
    auto mipBase = std::make_shared<TextureImage>();
    mipBase->defined = true;
    mipBase->width = 4;
    mipBase->height = 4;
    mipBase->internal = InternalFormat::RGBA8;
    mipBase->pixels.resize(64);
    for (unsigned pixel = 0; pixel < 16; ++pixel) {
        mipBase->pixels[pixel * 4] = 255;
        mipBase->pixels[pixel * 4 + 3] = 255;
    }
    auto mipOne = std::make_shared<TextureImage>();
    mipOne->defined = true;
    mipOne->width = 2;
    mipOne->height = 2;
    mipOne->internal = InternalFormat::RGBA8;
    mipOne->pixels = {0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255};
    auto mipTexture = std::make_shared<TextureObject>();
    mipTexture->levels[0] = mipBase;
    mipTexture->levels[1] = mipOne;
    mipTexture->sampler.minification = Filter::NearestMipmapNearest;
    mipTexture->sampler.maximumLevel = 1;
    frame.draws[0] = textured;
    frame.draws[0].raster.viewport = {{0, 0, 2, 2}};
    frame.draws[0].resources[0].texture2D = mipTexture;
    CHECK(session.Render(frame, error));
    CHECK(session.ReadPixels(0, 0, 1, 1, 4, 1, rgba.data(), error));
    CHECK(rgba[0] == 0 && rgba[2] == 255);
    // Palette identity participates in the immutable GPU texture cache key.
    auto indexImage = std::make_shared<TextureImage>();
    indexImage->defined = true;
    indexImage->width = 1;
    indexImage->height = 1;
    indexImage->internal = InternalFormat::ColorIndex8;
    indexImage->pixels = {7};
    auto indexed = std::make_shared<TextureObject>();
    indexed->levels[0] = indexImage;
    indexed->sampler.minification = Filter::Nearest;
    auto paletteRed = std::make_shared<SharedPalette>();
    (*paletteRed)[21] = 255;
    auto paletteGreen = std::make_shared<SharedPalette>();
    (*paletteGreen)[22] = 255;
    frame.draws[0] = textured;
    frame.draws[0].resources[0].texture2D = indexed;
    frame.draws[0].palette = paletteRed;
    CHECK(!session.Render(frame, error));
    frame.draws[0].sharedTexturePalette = true;
    CHECK(session.Render(frame, error));
    frame.draws[0].palette = paletteGreen;
    CHECK(session.Render(frame, error));
    CHECK(session.ReadPixels(8, 8, 1, 1, 4, 1, rgba.data(), error));
    CHECK(rgba[0] == 0 && rgba[1] == 255);
    frame.draws[0].palette = paletteRed;
    CHECK(session.Render(frame, error));
    CHECK(session.ReadPixels(8, 8, 1, 1, 4, 1, rgba.data(), error));
    CHECK(rgba[0] == 255 && rgba[1] == 0);
    // Reversed depth range is exercised by the engine's z-trick owner.
    frame.draws[0] = Solid(1, 0, 0);
    frame.draws[0].raster.enabled[static_cast<unsigned>(Capability::DepthTest)] = true;
    frame.draws[0].raster.depthFunction = Compare::GreaterEqual;
    frame.draws[0].raster.depthRange = {{1, 0.5}};
    for (auto& vertex : frame.draws[0].vertices) {
        vertex.position[2] = -0.5f;
    }
    clear.clear.mask = kClearColor | kClearDepth;
    clear.clear.depth = 0;
    clear.clear.scissorEnabled = false;
    frame.events.insert(frame.events.begin(), clear);
    CHECK(session.Render(frame, error));
    frame.draws[0].vertices[0].position[2] = 0.5f;
    for (auto& vertex : frame.draws[0].vertices) {
        vertex.position[2] = 0.5f;
        vertex.color = {{0, 1, 0, 1}};
    }
    frame.events.erase(frame.events.begin());
    CHECK(session.Render(frame, error));
    CHECK(session.ReadPixels(8, 8, 1, 1, 4, 1, rgba.data(), error));
    CHECK(rgba[0] == 255 && rgba[1] == 0);
    // Masked clear preserves red while replacing green, and ignores the scene viewport.
    clear.clear.mask = kClearColor;
    clear.clear.color = {{0, 1, 1, 1}};
    clear.clear.colorWrite = {{false, true, false, false}};
    frame.events = {clear};
    CHECK(session.Render(frame, error));
    CHECK(session.ReadPixels(8, 8, 1, 1, 4, 1, rgba.data(), error));
    CHECK(rgba[0] == 255 && rgba[1] == 255 && rgba[2] == 0);
    // Back-face culling retains the engine's CCW front face; front culling removes it.
    frame.draws[0] = Solid(1, 0, 0);
    frame.draws[0].raster.enabled[static_cast<unsigned>(Capability::CullFace)] = true;
    frame.draws[0].raster.cull = Cull::Back;
    clear.clear.color = {{0, 0, 0, 1}};
    clear.clear.colorWrite = {{true, true, true, true}};
    frame.events = {clear, draw};
    CHECK(session.Render(frame, error));
    CHECK(session.ReadPixels(8, 8, 1, 1, 4, 1, rgba.data(), error));
    CHECK(rgba[0] == 255);
    frame.draws[0].raster.cull = Cull::Front;
    CHECK(session.Render(frame, error));
    CHECK(session.ReadPixels(8, 8, 1, 1, 4, 1, rgba.data(), error));
    CHECK(rgba[0] == 0);
    // Quad wireframe has no triangulation diagonal through the center.
    frame.draws[0].raster.enabled[static_cast<unsigned>(Capability::CullFace)] = false;
    frame.draws[0].raster.polygonMode = {{PolygonMode::Line, PolygonMode::Line}};
    CHECK(session.Render(frame, error));
    CHECK(session.ReadPixels(8, 8, 1, 1, 4, 1, rgba.data(), error));
    CHECK(rgba[0] == 0);
    // Rectangle coordinates remain homogeneous until fragment division by Q.
    auto rectangle = std::make_shared<TextureObject>(*texture);
    rectangle->target = TextureTarget::Rectangle;
    frame.draws[0] = textured;
    frame.draws[0].textures[0].enabled2D = false;
    frame.draws[0].textures[0].enabledRectangle = true;
    frame.draws[0].resources[0].rectangle = rectangle;
    for (auto& vertex : frame.draws[0].vertices) {
        vertex.texcoords[0] = {{1, 3, 0, 2}};
    }
    CHECK(session.Render(frame, error));
    CHECK(session.ReadPixels(8, 8, 1, 1, 4, 1, rgba.data(), error));
    CHECK(rgba[0] == 0 && rgba[1] == 255);
    session.Shutdown();
    // Presented readback retains drawable-sized letterboxing after swapchain submission.
    auto* window = SDL_CreateWindow("Metal presentation fixture", 32, 16, SDL_WINDOW_HIDDEN);
    CHECK(window);
    CHECK(session.Initialize(window, true, error));
    CHECK(session.Resize(16, 16, 1, error));
    frame.events = {draw};
    frame.draws[0] = Solid(1, 0, 0);
    CHECK(session.Render(frame, error));
    CHECK(session.Present(true, true, error));
    std::vector<unsigned char> presented(32 * 16 * 4);
    CHECK(session.ReadPresentedPixels(0, 0, 32, 16, 4, 1, presented.data(), error));
    CHECK(presented[0] == 0 && presented[3] == 255);
    CHECK(presented[16 * 4] == 255 && presented[16 * 4 + 1] == 0);
    CHECK(SDL_ShowWindow(window));
    CHECK(SDL_SetWindowSize(window, 40, 16));
    CHECK(SDL_SyncWindow(window));
    SDL_PumpEvents();
    // Feasible allocation failures happen before acquiring a swapchain image.
    presentationProbe = {};
    presentationProbe.active = true;
    presentationProbe.allocationsUntilFailure = 1;
    CHECK(!session.Present(true, true, error));
    CHECK(error.find("fixture final-color allocation failure") != std::string::npos);
    CHECK(presentationProbe.cancellations == 0 && presentationProbe.acquisitions == 0);
    presentationProbe.active = false;
    CHECK(session.Present(true, true, error));
    // A post-acquisition drawable change must retire by submission, preserving the allocation diagnostic.
    presentationProbe = {};
    presentationProbe.active = true;
    presentationProbe.mismatch = true;
    presentationProbe.allocationsUntilFailure = 1;
    CHECK(!session.Present(true, true, error));
    CHECK(error.find("fixture final-color allocation failure") != std::string::npos);
    CHECK(presentationProbe.acquisitions == 1);
    CHECK(presentationProbe.cancellations == 0 && presentationProbe.submissions == 1);
    presentationProbe.active = false;
    CHECK(session.Present(true, true, error));
    CHECK(session.Finish(error));
    session.Shutdown();
    session.Shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    std::cout << "Actual Metal session passed\n";
}
