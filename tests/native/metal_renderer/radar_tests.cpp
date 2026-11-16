#include "radar_renderer_gpu.h"
#include "metal_session.h"
#include "../../../src/public/vgui_backdrop.h"
#include <SDL3/SDL.h>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <vector>

using namespace cse::render;
#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << __LINE__ << ": " #condition << " " << error << '\n'; std::exit(1); \
} } while (false)

int main() {
    std::string error;
    CHECK(SDL_Init(SDL_INIT_VIDEO));
    SDL_SetAssertionHandler([](const SDL_AssertData* assertion, void*) {
        std::cerr << assertion->condition << '\n';
        return SDL_ASSERTION_ABORT;
    }, nullptr);
    MetalSession session;
    CHECK(session.Initialize(nullptr, true, error));
    CHECK(session.Resize(64, 64, 4, error));
    FrameSnapshot frame;
    frame.info.width = 64;
    frame.info.height = 64;
    FrameEvent clear;
    clear.kind = FrameEventKind::Clear;
    clear.clear.mask = kClearColor | kClearDepth;
    clear.clear.color = {{0, 0, 0, 1}};
    frame.events.push_back(clear);
    CHECK(session.Render(frame, error));
    RadarRendererGpu effects(session, 7);
    RasterState raster;
    raster.viewport = {{0, 0, 64, 64}};
    raster.enabled[static_cast<std::size_t>(Capability::AlphaTest)] = true;
    raster.alphaFunction = Compare::Greater;
    raster.alphaReference = 0.9f;
    CseRadarPrimitiveVertex vertices[4]{};
    const float positions[4][2] = {{0, 0}, {64, 0}, {64, 64}, {0, 64}};
    for (unsigned index = 0; index < 4; ++index) {
        vertices[index].position[0] = positions[index][0];
        vertices[index].position[1] = positions[index][1];
        vertices[index].shape[0] = CSE_RADAR_PRIMITIVE_DISC;
        vertices[index].shape[1] = 1.2f;
        vertices[index].shape[2] = 20;
        vertices[index].center[0] = 32;
        vertices[index].center[1] = 32;
        vertices[index].color[0] = 1;
        vertices[index].color[3] = 0.5f;
    }
    CseRadarPrimitiveBatch batch{};
    batch.size = sizeof(batch);
    batch.rendererEpoch = 7;
    batch.hud = {64, 64};
    batch.quadCount = 1;
    batch.vertices = vertices;
    CHECK(effects.DrawPrimitiveQuads(batch, raster) == CSE_RADAR_OK);
    std::vector<unsigned char> pixels(64 * 64 * 4);
    CHECK(session.ReadPixels(0, 0, 64, 64, 4, 4, pixels.data(), error));
    CHECK(pixels[(32 * 64 + 32) * 4] >= 127);
    CHECK(pixels[0] == 0);
    batch.rendererEpoch = 6;
    CHECK(effects.DrawPrimitiveQuads(batch, raster) == CSE_RADAR_STALE_EPOCH);
    batch.rendererEpoch = 7;
    batch.reserved = 1;
    CHECK(effects.DrawPrimitiveQuads(batch, raster) == CSE_RADAR_INVALID_ARGUMENT);
    CHECK(session.Render(frame, error));
    CseRadarMaskRingDraw ring{};
    ring.size = sizeof(ring);
    ring.rendererEpoch = 7;
    ring.hud = {64, 64};
    ring.outer = {{32, 32}, 20, 1, CSE_RADAR_SHAPE_CIRCLE, 0};
    ring.innerRadius = 10;
    ring.color[1] = 1;
    ring.color[3] = 0.5f;
    CHECK(effects.DrawMaskRing(ring, raster) == CSE_RADAR_OK);
    CHECK(session.ReadPixels(0, 0, 64, 64, 4, 4, pixels.data(), error));
    CHECK(pixels[(32 * 64 + 47) * 4 + 1] == 0); // inherited alpha threshold rejects ring
    raster.enabled[static_cast<std::size_t>(Capability::AlphaTest)] = false;
    CHECK(effects.DrawMaskRing(ring, raster) == CSE_RADAR_OK);
    CHECK(session.ReadPixels(0, 0, 64, 64, 4, 4, pixels.data(), error));
    CHECK(pixels[(32 * 64 + 47) * 4 + 1] >= 127);
    CHECK(pixels[(32 * 64 + 32) * 4 + 1] == 0);

    auto image = std::make_shared<TextureImage>();
    image->width = 2;
    image->height = 2;
    image->defined = true;
    image->pixels = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
    auto texture = std::make_shared<TextureObject>();
    texture->levels[0] = image;
    texture->sampler.minification = Filter::Nearest;
    texture->sampler.magnification = Filter::Nearest;
    const auto sprite = effects.RegisterTexture(1, RadarRendererGpu::TextureRole::Sprite, texture, nullptr);
    CseRadarMapTextureDraw tile{};
    tile.size = sizeof(tile);
    tile.rendererEpoch = 7;
    tile.hud = {64, 64};
    tile.texture = sprite;
    tile.mode = CSE_RADAR_MAP_MASKED_OVERVIEW;
    tile.mask = {{32, 32}, 30, 1.5f, CSE_RADAR_SHAPE_CIRCLE, 0};
    std::fill(tile.modulation, tile.modulation + 4, 1);
    tile.vertexCount = 4;
    tile.vertices[0] = {{0, 0}, {0, 0}};
    tile.vertices[1] = {{64, 0}, {0, 1}};
    tile.vertices[2] = {{64, 64}, {1, 1}};
    tile.vertices[3] = {{0, 64}, {1, 0}};
    CHECK(session.Render(frame, error));
    CHECK(effects.DrawMapTexture(tile, raster) == CSE_RADAR_OK);
    CHECK(session.ReadPixels(0, 0, 64, 64, 4, 4, pixels.data(), error));
    CHECK(pixels[0] == 0);
    CHECK(pixels[(47 * 64 + 16) * 4] > 120); // top left = red
    CHECK(pixels[(47 * 64 + 48) * 4 + 2] > 120); // top right = blue, TriAPI UV order

    auto crossed = tile;
    crossed.vertexCount = 5;
    constexpr float kStarStep = 2.51327412287f;
    for (unsigned index = 0; index < crossed.vertexCount; ++index) {
        crossed.vertices[index].position[0] = 32 + 20 * std::cos(index * kStarStep);
        crossed.vertices[index].position[1] = 32 + 20 * std::sin(index * kStarStep);
    }
    CHECK(effects.DrawMapTexture(crossed, raster) == CSE_RADAR_INVALID_ARGUMENT);

    CseRadarMapVertex mesh[6]{};
    const float triangles[6][3] = {{0, 0, 1}, {1, 0, 1}, {0, 1, 1}, {0, 0, 2}, {1, 0, 2}, {0, 1, 2}};
    for (unsigned index = 0; index < 6; ++index) {
        std::copy(triangles[index], triangles[index] + 3, mesh[index].position);
        mesh[index].color[index < 3 ? 0 : 1] = 1;
        mesh[index].color[3] = 0.5f;
    }
    const uint32_t indices[] = {0, 1, 2, 3, 4, 5};
    CseRadarMapDraw meshDraw{};
    meshDraw.indexCount = 6;
    CseRadarMapPass meshPass{0, 0, 1, 1, 0, 3, 0, 1, CSE_RADAR_DEPTH_TOPMOST, 1};
    CseRadarMapImageBuild build{};
    build.size = sizeof(build);
    build.rendererEpoch = 7;
    build.vertexCount = 6;
    build.indexCount = 6;
    build.drawCount = 1;
    build.passCount = 1;
    build.vertices = mesh;
    build.indices = indices;
    build.draws = &meshDraw;
    build.passes = &meshPass;
    CseRadarTexture generated{};
    CHECK(effects.BuildMapImage(build, generated) == CSE_RADAR_OK);
    tile.texture = generated;
    tile.mode = CSE_RADAR_MAP_PREMULTIPLIED_IMAGE;
    tile.mask.shape = CSE_RADAR_SHAPE_SQUARE;
    tile.mask.radius = 64;
    CHECK(session.Render(frame, error));
    CHECK(effects.DrawMapTexture(tile, raster) == CSE_RADAR_OK);
    CHECK(session.ReadPixels(0, 0, 64, 64, 4, 4, pixels.data(), error));
    CHECK(pixels[(47 * 64 + 16) * 4] == 0);
    CHECK(pixels[(47 * 64 + 16) * 4 + 1] > 90); // topmost green, premultiplied once
    CHECK(effects.ReleaseImage(sprite) == CSE_RADAR_INVALID_TEXTURE);
    CHECK(effects.ReleaseImage(generated) == CSE_RADAR_OK);
    CHECK(effects.ReleaseImage(generated) == CSE_RADAR_INVALID_TEXTURE);
    CHECK(effects.DrawMapTexture(tile, raster) == CSE_RADAR_INVALID_TEXTURE);
    cse::ui::BackdropRect panel{0, 0, 64, 64};
    cse::ui::BackdropStyle glass{1, 1, 0};
    effects.BeginSmokedFrame();
    CHECK(!effects.DrawSmoked(panel, panel, 64, 64, glass, 8, raster));
    frame.events[0].clear.color = {{1, 0, 0, 1}};
    auto half = frame.events[0];
    half.clear.color = {{0, 0, 1, 1}};
    half.clear.scissorEnabled = true;
    half.clear.scissor = {{32, 0, 32, 64}};
    frame.events.push_back(half);
    CHECK(session.Render(frame, error));
    CHECK(effects.CaptureSmoked(0, 0, 64, 64));
    frame.events.resize(1);
    frame.events[0].clear.color = {{0, 1, 0, 1}};
    CHECK(session.Render(frame, error));
    CHECK(effects.DrawSmoked(panel, panel, 64, 64, glass, 8, raster));
    CHECK(session.ReadPixels(0, 0, 64, 64, 4, 4, pixels.data(), error));
    CHECK(pixels[(32 * 64 + 32) * 4] > 50);
    CHECK(pixels[(32 * 64 + 32) * 4 + 2] > 50);
    CHECK(pixels[(32 * 64 + 32) * 4 + 1] == 0); // captured pre-HUD scene, not green foreground
    CHECK(pixels[1] == 255); // rounded corner preserves foreground
    CseRadarBackdropDraw backdrop{};
    backdrop.size = sizeof(backdrop);
    backdrop.rendererEpoch = 7;
    backdrop.hud = {64, 64};
    backdrop.extent = 64;
    backdrop.shape = CSE_RADAR_SHAPE_SQUARE;
    backdrop.darken = 0.35f;
    backdrop.opacity = 1;
    CHECK(session.Render(frame, error));
    CHECK(effects.DrawRadarBackdrop(backdrop, raster) == CSE_RADAR_OK);
    CHECK(session.ReadPixels(0, 0, 64, 64, 4, 4, pixels.data(), error));
    CHECK(pixels[(32 * 64 + 32) * 4 + 1] >= 88 && pixels[(32 * 64 + 32) * 4 + 1] <= 90);
    // Recycled engine names must never recycle the service identity.
    auto firstGeneration = std::make_shared<TextureObject>(*texture);
    firstGeneration->generation = 10;
    const auto oldSprite = effects.RegisterTexture(99, RadarRendererGpu::TextureRole::Sprite, firstGeneration, nullptr);
    auto nextGeneration = std::make_shared<TextureObject>(*firstGeneration);
    nextGeneration->generation = 11;
    const auto newSprite = effects.RegisterTexture(99, RadarRendererGpu::TextureRole::Sprite, nextGeneration, nullptr);
    CHECK(oldSprite.serial != newSprite.serial);
    tile.texture = oldSprite;
    tile.mode = CSE_RADAR_MAP_MASKED_OVERVIEW;
    CHECK(effects.DrawMapTexture(tile, raster) == CSE_RADAR_INVALID_TEXTURE);
    batch.reserved = 0;
    vertices[0].shape[2] = -1;
    CHECK(effects.DrawPrimitiveQuads(batch, raster) == CSE_RADAR_INVALID_ARGUMENT);
    vertices[0].shape[2] = 20;
    vertices[1].shape[2] = 21;
    CHECK(effects.DrawPrimitiveQuads(batch, raster) == CSE_RADAR_INVALID_ARGUMENT);
    vertices[1].shape[2] = 20;

    // Glyph coverage and black baked shadows use atlas alpha and red independently.
    auto glyphImage = std::make_shared<TextureImage>(*image);
    glyphImage->pixels = {255, 255, 255, 128, 0, 0, 0, 128, 255, 255, 255, 128, 0, 0, 0, 128};
    auto glyphTexture = std::make_shared<TextureObject>(*texture);
    glyphTexture->levels[0] = glyphImage;
    batch.atlas = effects.RegisterTexture(100, RadarRendererGpu::TextureRole::Glyph, glyphTexture, nullptr);
    for (unsigned index = 0; index < 4; ++index) {
        vertices[index].shape[0] = CSE_RADAR_PRIMITIVE_GLYPH;
        vertices[index].shape[1] = 0;
        vertices[index].shape[2] = 0;
        vertices[index].color[3] = 1;
        vertices[index].triangle[0] = positions[index][0] / 64;
        vertices[index].triangle[1] = positions[index][1] / 64;
    }
    frame.events[0].clear.color = {{0, 1, 0, 1}};
    CHECK(session.Render(frame, error));
    CHECK(effects.DrawPrimitiveQuads(batch, raster) == CSE_RADAR_OK);
    CHECK(session.ReadPixels(0, 0, 64, 64, 4, 4, pixels.data(), error));
    CHECK(pixels[(32 * 64 + 16) * 4] >= 127 && pixels[(32 * 64 + 16) * 4 + 1] >= 126);
    CHECK(pixels[(32 * 64 + 48) * 4] == 0 && pixels[(32 * 64 + 48) * 4 + 1] >= 126);

    // A later floor clears depth while preserving previous color, with final tone applied only once.
    CseRadarMapDraw floorDraws[2]{};
    floorDraws[0].firstIndex = 3;
    floorDraws[0].indexCount = 3;
    floorDraws[1].indexCount = 3;
    CseRadarMapPass floorPasses[2] = {meshPass, meshPass};
    floorPasses[1].firstDraw = 1;
    floorPasses[1].clearColor = 0;
    build.draws = floorDraws;
    build.drawCount = 2;
    build.passes = floorPasses;
    build.passCount = 2;
    build.tone = CSE_RADAR_TONE_TEXTURED;
    CHECK(effects.BuildMapImage(build, generated) == CSE_RADAR_OK);
    tile.texture = generated;
    tile.mode = CSE_RADAR_MAP_PREMULTIPLIED_IMAGE;
    tile.alphaAa = 1;
    frame.events[0].clear.color = {{0, 0, 0, 1}};
    CHECK(session.Render(frame, error));
    CHECK(effects.DrawMapTexture(tile, raster) == CSE_RADAR_OK);
    CHECK(session.ReadPixels(0, 0, 64, 64, 4, 4, pixels.data(), error));
    CHECK(pixels[(47 * 64 + 16) * 4] > 95); // later lower red layer wins
    CHECK(pixels[(47 * 64 + 16) * 4 + 1] < 30); // only final shadow lift, no earlier green
    CHECK(effects.ReleaseImage(generated) == CSE_RADAR_OK);
    build.passes = &meshPass;
    build.passCount = 1;
    build.draws = &meshDraw;
    build.drawCount = 1;
    meshPass.depthOrder = CSE_RADAR_DEPTH_BOTTOMMOST;
    CHECK(effects.BuildMapImage(build, generated) == CSE_RADAR_OK);
    CHECK(effects.ReleaseImage(generated) == CSE_RADAR_OK);

    // Shape 10 combines overlapping marker images before one straight-alpha composite.
    batch.atlas = {};
    for (auto& vertex : vertices) {
        std::fill(vertex.triangle, vertex.triangle + 4, 0);
        std::fill(vertex.triangle2, vertex.triangle2 + 4, 0);
        vertex.shape[0] = CSE_RADAR_PRIMITIVE_PLAYER_FADE;
        vertex.shape[1] = 1.2f;
        vertex.shape[2] = 12;
        vertex.shape[3] = 0.5f;
        vertex.center[0] = 32;
        vertex.center[1] = 32;
        vertex.center[2] = 32;
        vertex.center[3] = 32;
        vertex.color[3] = 0.5f;
        vertex.triangle2[1] = 0.5f;
        vertex.triangle2[2] = 0.5f;
    }
    frame.events[0].clear.color = {{0, 0, 0, 1}};
    CHECK(session.Render(frame, error));
    CHECK(effects.DrawPrimitiveQuads(batch, raster) == CSE_RADAR_OK);
    CHECK(session.ReadPixels(0, 0, 64, 64, 4, 4, pixels.data(), error));
    CHECK(pixels[(32 * 64 + 32) * 4] >= 127 && pixels[(32 * 64 + 32) * 4] <= 128);

    // Cone forward decay uses SRC_ALPHA / ONE rather than source-over blending.
    for (auto& vertex : vertices) {
        vertex.shape[0] = CSE_RADAR_PRIMITIVE_CONE;
        vertex.shape[1] = 1.5f;
        vertex.shape[2] = 0;
        vertex.shape[3] = 40;
        vertex.center[0] = 12;
        vertex.center[1] = 32;
        vertex.center[2] = 1;
        vertex.center[3] = 0;
        vertex.color[3] = 1;
        vertex.triangle2[1] = 0;
        vertex.triangle2[2] = 0.6f;
    }
    batch.blend = CSE_RADAR_BLEND_ADDITIVE_ALPHA;
    frame.events[0].clear.color = {{0.2f, 0, 0, 1}};
    CHECK(session.Render(frame, error));
    CHECK(effects.DrawPrimitiveQuads(batch, raster) == CSE_RADAR_OK);
    CHECK(session.ReadPixels(0, 0, 64, 64, 4, 4, pixels.data(), error));
    const int expectedCone = int((0.2f + std::exp(-3.5f * 10.5f / 40.0f)) * 255);
    CHECK(std::abs(int(pixels[(31 * 64 + 22) * 4]) - expectedCone) <= 2);
    CHECK(pixels[(31 * 64 + 8) * 4] == 51);

    // Accepted draws keep their immutable version; subsequent calls resolve the current logical image.
    TextureStore store;
    LegacyState state;
    CHECK(store.Bind(state, TextureTarget::Texture2D, 101) == Status::Ok);
    TextureUpload upload;
    upload.width = 1;
    upload.height = 1;
    upload.bytes = {255, 0, 0, 255};
    CHECK(store.Upload(state, TextureTarget::Texture2D, upload, {}, false) == Status::Ok);
    CHECK(store.Parameter(state, TextureTarget::Texture2D, TextureParameter::MinFilter, int(Filter::Nearest)) == Status::Ok);
    effects.SetTextureSource(&store, &state, nullptr);
    tile.texture = effects.RegisterTexture(101, RadarRendererGpu::TextureRole::Sprite,
        store.Bound(state, TextureTarget::Texture2D), nullptr);
    tile.mode = CSE_RADAR_MAP_PLAIN_OVERVIEW;
    tile.mask = {};
    tile.alphaAa = 0;
    CHECK(session.Render(frame, error));
    CHECK(effects.DrawMapTexture(tile, raster) == CSE_RADAR_OK);
    upload.bytes = {0, 255, 0, 255};
    CHECK(store.Upload(state, TextureTarget::Texture2D, upload, {}, false) == Status::Ok);
    CHECK(session.ReadPixels(0, 0, 64, 64, 4, 4, pixels.data(), error));
    CHECK(pixels[(32 * 64 + 32) * 4] == 255);
    CHECK(effects.DrawMapTexture(tile, raster) == CSE_RADAR_OK);
    CHECK(session.ReadPixels(0, 0, 64, 64, 4, 4, pixels.data(), error));
    CHECK(pixels[(32 * 64 + 32) * 4 + 1] == 255);
    store.Delete(state, {101});
    CHECK(effects.DrawMapTexture(tile, raster) == CSE_RADAR_INVALID_TEXTURE);
    effects.SetTextureSource(nullptr, nullptr, nullptr);

    // Resize invalidates the pre-HUD cache; the next request/capture restores it at odd dimensions.
    CHECK(session.Resize(65, 63, 4, error));
    CHECK(!effects.DrawSmoked(panel, panel, 64, 64, glass, 8, raster));
    frame.info.width = 65;
    frame.info.height = 63;
    frame.events[0].clear.color = {{0, 0, 1, 0}};
    CHECK(session.Render(frame, error));
    CHECK(effects.CaptureSmoked(0, 0, 65, 63));
    frame.events[0].clear.color = {{0, 1, 0, 1}};
    CHECK(session.Render(frame, error));
    panel = {0, 0, 65, 63};
    const cse::ui::BackdropRect clippedPanel{8, 7, 40, 35};
    CHECK(effects.DrawSmoked(panel, clippedPanel, 65, 63, glass, 8, raster));
    pixels.resize(65 * 63 * 4);
    CHECK(session.ReadPixels(0, 0, 65, 63, 4, 4, pixels.data(), error));
    CHECK(pixels[(31 * 65 + 32) * 4 + 2] == 255); // captured alpha ignored
    CHECK(pixels[1] == 255); // explicit clip preserved
    std::cout << "Metal radar primitives, map layers, texture lifetime and blur pixels passed\n";
}
