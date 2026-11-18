#include "metal_draw.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace cse::render;

#define CHECK(x) do { \
    if (!(x)) { \
        std::cerr << __LINE__ << ": " #x << " " << "\n"; \
        std::exit(1); \
    } \
} while (false)

DrawPacket Quad() {
    DrawPacket draw;
    draw.primitive = Primitive::Quads;
    draw.shade = Shade::Flat;
    draw.raster.shade = Shade::Flat;
    draw.raster.viewport = {{0, 0, 8, 8}};
    draw.vertices.resize(4);
    draw.vertices[0].position = {{-1, -1, 0, 1}};
    draw.vertices[1].position = {{1, -1, 0, 1}};
    draw.vertices[2].position = {{1, 1, 0, 1}};
    draw.vertices[3].position = {{-1, 1, 0, 1}};
    draw.vertices[3].color = {{0, 1, 0, 1}};
    for (auto& matrix : draw.textureMatrices) {
        matrix = Matrix4::Identity();
    }
    return draw;
}

void BorrowedQuads() {
    std::string error;
    DrawPacket quad;
    quad.primitive = Primitive::Quads;
    quad.vertices.resize(4);
    quad.shade = Shade::Flat;
    MetalDrawData prepared;
    CHECK(PrepareMetalDraw(quad, prepared, error, MetalGeometryOwnership::Borrow));
    CHECK(prepared.borrowedVertices == &quad.vertices);
    CHECK(prepared.IndexCount() == 6);

    quad.vertices.back().color = {{0.25f, 0.5f, 0.75f, 1.0f}};
    CHECK(PrepareMetalDraw(quad, prepared, error, MetalGeometryOwnership::Borrow));
    CHECK(!prepared.borrowedVertices);
    for (const auto& vertex : prepared.Vertices()) {
        CHECK(vertex.color == quad.vertices.back().color);
    }
    quad.shade = Shade::Smooth;
    CHECK(PrepareMetalDraw(quad, prepared, error, MetalGeometryOwnership::Borrow));
    CHECK(prepared.borrowedVertices == &quad.vertices);
    CHECK(prepared.IndexCount() == 6);

    auto& negativeZero = quad.vertices.front().color[0];
    negativeZero = -0.0f;
    CHECK(PrepareMetalDraw(quad, prepared, error, MetalGeometryOwnership::Borrow));
    CHECK(!prepared.borrowedVertices);
    CHECK(!std::signbit(prepared.Vertices().front().color[0]));

    negativeZero = 0.0f;
    quad.vertices.emplace_back();
    CHECK(PrepareMetalDraw(quad, prepared, error, MetalGeometryOwnership::Borrow));
    CHECK(!prepared.borrowedVertices && prepared.Vertices().size() == 4);
    CHECK(prepared.IndexCount() == 6);
}

void BorrowedTriangles() {
    using Ownership = MetalGeometryOwnership;
    auto indexed = Quad();
    indexed.primitive = Primitive::Triangles;
    indexed.shade = Shade::Smooth;
    indexed.raster.shade = Shade::Smooth;
    indexed.indices = {0, 1, 2, 0, 2, 3};
    MetalDrawData prepared;
    std::string error;
    CHECK(PrepareMetalDraw(indexed, prepared, error, Ownership::Borrow));
    CHECK(&prepared.Vertices() == &indexed.vertices);
    CHECK(&prepared.Indices() == &indexed.indices);
    CHECK(PrepareMetalDraw(indexed, prepared, error));
    CHECK(&prepared.Vertices() != &indexed.vertices);
    CHECK(&prepared.Indices() != &indexed.indices);
    CHECK(prepared.Vertices().size() == indexed.vertices.size());
    auto invalid = indexed;
    invalid.indices[0] = std::numeric_limits<std::uint32_t>::max();
    CHECK(!PrepareMetalDraw(invalid, prepared, error, Ownership::Borrow));
    CHECK(prepared.Indices() == indexed.indices);
    indexed.vertices[0].color = {{-0.0f, 1.25f, 0.5f, 1}};
    CHECK(PrepareMetalDraw(indexed, prepared, error, Ownership::Borrow));
    CHECK(&prepared.Vertices() != &indexed.vertices);
    CHECK(prepared.Vertices()[0].color[0] == 0.0f && !std::signbit(prepared.Vertices()[0].color[0]));
    CHECK(prepared.Vertices()[0].color[1] == 1);
    indexed.vertices[1].position[0] = std::numeric_limits<float>::infinity();
    CHECK(PrepareMetalDraw(indexed, prepared, error, Ownership::Borrow));
    CHECK(&prepared.Vertices() != &indexed.vertices);
    CHECK(&prepared.Indices() != &indexed.indices);
    CHECK(prepared.discardedNonfinite && prepared.Indices().size() == 3);
    CHECK(prepared.Vertices()[prepared.Indices()[2]].position == indexed.vertices[3].position);
    // Ignored texture units still sanitize their own invalid attributes before upload.
    indexed.vertices[1] = Quad().vertices[1];
    indexed.vertices[2].texcoords[1][0] = std::numeric_limits<float>::quiet_NaN();
    CHECK(PrepareMetalDraw(indexed, prepared, error, Ownership::Borrow));
    CHECK(&prepared.Vertices() != &indexed.vertices);
    CHECK(!prepared.discardedNonfinite && prepared.Indices().size() == 6);
    CHECK((prepared.Vertices()[2].texcoords[1] == std::array<float, 4>{{0, 0, 0, 1}}));
}

void CanonicalTextureLevels() {
    TextureObject source;
    source.sampler.minification = Filter::LinearMipmapLinear;
    auto base = std::make_shared<TextureImage>();
    base->defined = true;
    base->width = 2;
    base->height = 2;
    base->internal = InternalFormat::RGBA;
    base->pixels = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    auto mip = std::make_shared<TextureImage>(*base);
    mip->width = 1;
    mip->height = 1;
    mip->pixels.assign(4, 64);
    source.levels[0] = base;
    source.levels[1] = mip;
    MetalTextureData prepared;
    std::string error;
    CHECK(PrepareMetalTexture(source, {}, prepared, error));
    CHECK(!prepared.incomplete && prepared.levels.size() == 2);
    CHECK(prepared.levels[0] == base && prepared.levels[1] == mip);
    const std::weak_ptr<const TextureImage> level = source.levels[0];
    base.reset();
    mip.reset();
    source.levels.clear();
    CHECK(!level.expired() && prepared.levels[0]->pixels.back() == 16);
    prepared.levels.clear();
    CHECK(level.expired());
}

int main() {
    CanonicalTextureLevels();
    BorrowedTriangles();
    BorrowedQuads();
    // Missing quad lowering or the wrong provoking vertex makes this fail.
    auto draw = Quad();
    MetalDrawData prepared;
    std::string error;
    CHECK(PrepareMetalDraw(draw, prepared, error));
    CHECK(prepared.vertices.size() == 6);
    for (const auto& vertex : prepared.vertices) {
        CHECK(vertex.color[1] == 1 && vertex.color[0] == 0);
    }
    CHECK(prepared.vertices[3].position == draw.vertices[0].position);
    auto indexed = Quad();
    indexed.primitive = Primitive::Triangles;
    indexed.shade = Shade::Smooth;
    indexed.raster.shade = Shade::Smooth;
    indexed.indices = {0, 1, 2, 0, 2, 3};
    CHECK(PrepareMetalDraw(indexed, prepared, error));
    CHECK(prepared.vertices.size() == indexed.vertices.size() && prepared.indices == indexed.indices);
    CHECK(prepared.vertices[3].color == indexed.vertices[3].color);
    indexed.indices[0] = static_cast<std::uint32_t>(indexed.vertices.size());
    CHECK(!PrepareMetalDraw(indexed, prepared, error));
    CHECK(prepared.indices == std::vector<std::uint32_t>({0, 1, 2, 0, 2, 3}));
    indexed.indices = {0, 1, 3, 0, 3, 2};
    indexed.shade = Shade::Flat;
    indexed.raster.shade = Shade::Flat;
    indexed.vertices[2].color = {{1, 0, 0, 1}};
    CHECK(PrepareMetalDraw(indexed, prepared, error));
    CHECK(prepared.indices.empty() && prepared.vertices.size() == indexed.indices.size());
    CHECK(prepared.vertices[0].color == indexed.vertices[3].color);
    CHECK(prepared.vertices[3].color == indexed.vertices[2].color);
    indexed.raster.polygonMode = {{PolygonMode::Line, PolygonMode::Line}};
    CHECK(PrepareMetalDraw(indexed, prepared, error));
    CHECK(prepared.indices.empty() && prepared.topology == Primitive::Lines && prepared.vertices.size() == 12);
    CHECK(PrepareMetalDraw(draw, prepared, error));
    draw.primitive = Primitive::Polygon;
    draw.vertices[0].color = {{1, 0, 0, 1}};
    CHECK(PrepareMetalDraw(draw, prepared, error));
    for (const auto& vertex : prepared.vertices) {
        CHECK(vertex.color[0] == 1 && vertex.color[1] == 0);
    }
    draw.primitive = Primitive::QuadStrip;
    CHECK(PrepareMetalDraw(draw, prepared, error));
    CHECK(prepared.vertices[2].position == draw.vertices[3].position);
    draw.primitive = Primitive::LineLoop;
    CHECK(PrepareMetalDraw(draw, prepared, error));
    CHECK(prepared.topology == Primitive::Lines && prepared.vertices.size() == 8);
    draw = Quad();
    draw.primitive = Primitive::Triangles;
    draw.shade = Shade::Smooth;
    draw.vertices[0].color = {{-0.5f, 0.25f, 1.5f, 0.75f}};
    CHECK(PrepareMetalDraw(draw, prepared, error));
    CHECK(prepared.vertices.size() == 3);
    CHECK(prepared.vertices[0].color == (std::array<float, 4>{{0, 0.25f, 1, 0.75f}}));
    CHECK(prepared.vertices[2].position == draw.vertices[2].position);
    // An incomplete trailing primitive has no geometry to reject.
    draw.vertices[3].position[0] = std::numeric_limits<float>::infinity();
    CHECK(PrepareMetalDraw(draw, prepared, error));
    CHECK(prepared.vertices.size() == 3);
    draw = Quad();
    draw.raster.polygonMode = {{PolygonMode::Line, PolygonMode::Line}};
    CHECK(PrepareMetalDraw(draw, prepared, error));
    CHECK(prepared.topology == Primitive::Lines && prepared.vertices.size() == 8);
    auto image = std::make_shared<TextureImage>();
    image->defined = true;
    image->width = 1;
    image->height = 1;
    image->internal = InternalFormat::RGB5A1;
    image->pixels = {128, 65, 250, 127};
    auto object = std::make_shared<TextureObject>();
    object->levels[0] = image;
    object->sampler.minification = Filter::Linear;
    MetalTextureData texture;
    CHECK(PrepareMetalTexture(*object, {}, texture, error));
    CHECK(texture.levels[0]->pixels == std::vector<std::uint8_t>({132, 66, 247, 0}));
    // Legal GL requests above the device maximum must still prepare a drawable texture.
    const std::array<std::array<float, 2>, 5> anisotropyCases{{
        {{1, 1}}, {{4, 4}}, {{16, 16}}, {{32, 16}}, {{std::numeric_limits<float>::max(), 16}}
    }};
    auto texturedDraw = Quad();
    texturedDraw.textures[0].enabled2D = true;
    texturedDraw.resources[0].texture2D = object;
    for (const auto& anisotropy : anisotropyCases) {
        object->sampler.requestedAnisotropy = anisotropy[0];
        CHECK(PrepareMetalTexture(*object, {}, texture, error));
        CHECK(PrepareMetalDraw(texturedDraw, prepared, error));
        CHECK(prepared.fragment.texture[0][3] == anisotropy[1]);
        CHECK(object->sampler.requestedAnisotropy == anisotropy[0]);
    }
    for (float invalid : {0.0f, 0.5f, std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::quiet_NaN()}) {
        object->sampler.requestedAnisotropy = invalid;
        CHECK(!PrepareMetalTexture(*object, {}, texture, error));
    }
    object->sampler.requestedAnisotropy = 1;
    image->internal = InternalFormat::RGBA4;
    CHECK(PrepareMetalTexture(*object, {}, texture, error));
    CHECK(texture.levels[0]->pixels == std::vector<std::uint8_t>({136, 68, 255, 119}));
    image->internal = InternalFormat::RGB;
    CHECK(PrepareMetalTexture(*object, {}, texture, error));
    CHECK(texture.levels[0]->pixels[3] == 255 && texture.components[1] == 0);
    image->internal = InternalFormat::Alpha;
    CHECK(PrepareMetalTexture(*object, {}, texture, error));
    CHECK(texture.levels[0]->pixels[0] == 255 && texture.components[0] == 0);
    image->internal = InternalFormat::Luminance;
    CHECK(PrepareMetalTexture(*object, {}, texture, error));
    CHECK(texture.levels[0]->pixels == std::vector<std::uint8_t>({128, 128, 128, 255}));
    image->internal = InternalFormat::ColorIndex8;
    image->pixels = {1};
    auto palette = std::make_shared<SharedPalette>();
    (*palette)[3] = 255;
    CHECK(PrepareMetalTexture(*object, palette, texture, error));
    CHECK(texture.levels[0]->pixels == std::vector<std::uint8_t>({255, 0, 0, 255}));
    (*palette)[3] = 0;
    (*palette)[4] = 255;
    CHECK(PrepareMetalTexture(*object, palette, texture, error));
    CHECK(texture.levels[0]->pixels == std::vector<std::uint8_t>({0, 255, 0, 255}));
    image->width = 2;
    image->height = 2;
    image->internal = InternalFormat::RGBA;
    image->pixels.assign(16, 255);
    object->sampler.minification = Filter::LinearMipmapLinear;
    CHECK(PrepareMetalTexture(*object, palette, texture, error));
    CHECK(texture.incomplete);
    auto mip = std::make_shared<TextureImage>();
    mip->defined = true;
    mip->width = 1;
    mip->height = 1;
    mip->internal = InternalFormat::RGBA;
    mip->pixels = {0, 0, 255, 255};
    object->levels[1] = mip;
    CHECK(PrepareMetalTexture(*object, palette, texture, error));
    CHECK(!texture.incomplete && texture.levels.size() == 2);
    image->pixels.pop_back();
    CHECK(!PrepareMetalTexture(*object, palette, texture, error));
    draw.vertices[0].position[0] = std::numeric_limits<float>::infinity();
    CHECK(PrepareMetalDraw(draw, prepared, error));
    CHECK(prepared.vertices.empty() && prepared.discardedNonfinite);
    draw = Quad();
    draw.primitive = Primitive::Polygon;
    draw.vertices.resize(600000);
    CHECK(!PrepareMetalDraw(draw, prepared, error));
    CHECK(error.find("budget") != std::string::npos);
    image->width = kTextureDimension;
    image->height = kTextureDimension;
    CHECK(!PrepareMetalTexture(*object, palette, texture, error));
    std::cout << "Metal preparation passed\n";
}
