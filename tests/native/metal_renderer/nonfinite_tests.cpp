#include "metal_draw.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace cse::render;

namespace {
void Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

DrawPacket Draw(Primitive primitive, std::size_t count) {
    DrawPacket draw;
    draw.primitive = primitive;
    draw.raster.viewport = {{0, 0, 16, 16}};
    draw.vertices.resize(count);
    for (std::size_t index = 0; index < count; ++index) {
        draw.vertices[index].position[0] = static_cast<float>(index);
        draw.vertices[index].color = {{0.25f, 0.5f, 0.75f, 1}};
    }
    for (auto& matrix : draw.textureMatrices) {
        matrix = Matrix4::Identity();
    }
    return draw;
}

MetalDrawData Prepare(const DrawPacket& draw) {
    MetalDrawData output;
    std::string error;
    Check(PrepareMetalDraw(draw, output, error), error.c_str());
    Check(error.empty(), "Rejected geometry must not leave a fatal error");
    return output;
}

void Expect(const MetalDrawData& output, std::initializer_list<unsigned> expected) {
    const auto count = output.indices.empty() ? output.vertices.size() : output.indices.size();
    Check(count == expected.size(), "Only affected primitives may disappear");
    std::size_t entry = 0;
    for (const auto index : expected) {
        const auto& vertex = output.vertices[output.indices.empty() ? entry : output.indices[entry]];
        Check(vertex.position[0] == index, "Surviving geometry must retain connectivity and order");
        for (const auto value : vertex.position) {
            Check(std::isfinite(value), "GPU positions must be finite");
        }
        ++entry;
    }
}

void Topologies(float invalid) {
    auto draw = Draw(Primitive::Triangles, 9);
    draw.vertices[4].position[2] = invalid;
    Expect(Prepare(draw), {0, 1, 2, 6, 7, 8});
    draw = Draw(Primitive::TriangleStrip, 7);
    draw.vertices[3].position[2] = invalid;
    Expect(Prepare(draw), {0, 1, 2, 4, 5, 6});
    draw = Draw(Primitive::TriangleFan, 7);
    draw.vertices[3].position[2] = invalid;
    Expect(Prepare(draw), {0, 1, 2, 0, 4, 5, 0, 5, 6});
    draw = Draw(Primitive::Quads, 12);
    draw.vertices[5].position[2] = invalid;
    Expect(Prepare(draw), {0, 1, 2, 0, 2, 3, 8, 9, 10, 8, 10, 11});
    draw = Draw(Primitive::QuadStrip, 8);
    draw.vertices[3].position[2] = invalid;
    Expect(Prepare(draw), {4, 5, 7, 4, 7, 6});
    draw = Draw(Primitive::Lines, 6);
    draw.vertices[2].position[2] = invalid;
    Expect(Prepare(draw), {0, 1, 4, 5});
    draw = Draw(Primitive::LineLoop, 6);
    draw.vertices[2].position[2] = invalid;
    Expect(Prepare(draw), {0, 1, 3, 4, 4, 5, 5, 0});
    draw = Draw(Primitive::Polygon, 5);
    draw.vertices[2].position[2] = invalid;
    Expect(Prepare(draw), {});
}

void Indexed() {
    auto draw = Draw(Primitive::Triangles, 9);
    draw.indices = {0, 1, 2, 2, 3, 4, 4, 5, 6, 6, 7, 0};
    draw.vertices[3].position[1] = std::numeric_limits<float>::infinity();
    draw.vertices[8].position[1] = std::numeric_limits<float>::quiet_NaN(); // Never referenced.
    Expect(Prepare(draw), {0, 1, 2, 4, 5, 6, 6, 7, 0});
    draw.shade = Shade::Flat;
    draw.raster.shade = Shade::Flat;
    draw.vertices[2].color = {{1, 0, 0, 1}};
    draw.vertices[6].color = {{0, 1, 0, 1}};
    auto output = Prepare(draw);
    Expect(output, {0, 1, 2, 4, 5, 6, 6, 7, 0});
    Check(output.vertices[0].color == draw.vertices[2].color &&
        output.vertices[3].color == draw.vertices[6].color, "Retain indexed flat provoking colors");
    draw.raster.polygonMode = {{PolygonMode::Line, PolygonMode::Line}};
    Expect(Prepare(draw), {0, 1, 1, 2, 2, 0, 4, 5, 5, 6, 6, 4, 6, 7, 7, 0, 0, 6});
    draw.indices[0] = 100;
    std::string error;
    Check(!PrepareMetalDraw(draw, output, error), "Out-of-bounds indices remain structural failures");
}

void Attributes() {
    auto draw = Draw(Primitive::Triangles, 6);
    const float invalid = std::numeric_limits<float>::quiet_NaN();
    draw.shade = Shade::Flat;
    draw.raster.shade = Shade::Flat;
    draw.vertices[0].color[0] = invalid; // Does not provoke.
    draw.vertices[0].texcoords[2][0] = invalid; // Disabled texture unit.
    Expect(Prepare(draw), {0, 1, 2, 3, 4, 5});
    draw.textures[2].enabled2D = true;
    Expect(Prepare(draw), {3, 4, 5});
    draw.textures[2].enabled2D = false;
    draw.shade = Shade::Smooth;
    Expect(Prepare(draw), {3, 4, 5});
    draw = Draw(Primitive::Triangles, 4);
    draw.vertices.back().position[0] = invalid; // Incomplete trailing primitive.
    Expect(Prepare(draw), {0, 1, 2});
    draw = Draw(Primitive::Quads, 8);
    draw.vertices[1].position[2] = invalid;
    draw.raster.polygonMode = {{PolygonMode::Line, PolygonMode::Line}};
    Expect(Prepare(draw), {4, 5, 5, 6, 6, 7, 7, 4});
}

void Transforms() {
    auto draw = Draw(Primitive::Triangles, 3);
    draw.modelView.values[12] = std::numeric_limits<float>::infinity();
    Expect(Prepare(draw), {});
    draw.modelView = Matrix4::Identity();
    draw.textureMatrices[1].values[0] = std::numeric_limits<float>::quiet_NaN();
    Expect(Prepare(draw), {0, 1, 2});
    draw.textures[1].enabled2D = true;
    Expect(Prepare(draw), {});
}
}

int main() {
    for (const float invalid : {std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        Topologies(invalid);
    }
    Indexed();
    Attributes();
    Transforms();
    std::cout << "PASS nonfinite primitives preserve neighboring geometry, topology and attributes\n";
}
