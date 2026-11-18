#include "studio_mesh_gl.h"
#include "render/native/legacy_state.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

using byte = unsigned char;
using int32 = std::int32_t;
using vec3_t = float[3];
#include "studio.h"

#ifndef CSE_STUDIO_ENGINE_EXTRACT
#define CSE_STUDIO_ENGINE_EXTRACT "engine_attributes_extracted.inc"
#endif
#define CSE_STUDIO_EXTRACT_AUXVERT
#include CSE_STUDIO_ENGINE_EXTRACT
#undef CSE_STUDIO_EXTRACT_AUXVERT

namespace cse::render {
bool UsingMetal();
Status SubmitNativeIndexed(const std::vector<Vertex>& vertices,
    const std::vector<std::uint32_t>& indices, std::size_t lastVertex, bool perVertexColor);
}

namespace {
using cse::studio::DrawVertex;
using cse::studio::GLFunctions;

void Check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

void Equal(float actual, float expected, const char* message) {
    if (std::fabs(actual - expected) > 0.00001f) {
        std::fprintf(stderr, "FAIL: %s: actual %.9g, expected %.9g\n", message, actual, expected);
        std::exit(EXIT_FAILURE);
    }
}

cse::studio::MeshCache studioMeshCache;
std::vector<DrawVertex> studioDrawVertices;
std::vector<cse::render::Vertex> studioNativeVertices;
bool useMetal = false;
studiohdr_t* pstudiohdr = nullptr;
mstudiomodel_t submodel{};
mstudiomodel_t* psubmodel = &submodel;
int pvlightvalues_size = 3;
auxvert_t auxiliary[4]{};
auxvert_t* pauxverts = auxiliary;
vec3_t lighting[3] = {{0.1f, 0.2f, 0.3f}, {0.4f, 0.5f, 0.6f}, {0.7f, 0.8f, 0.9f}};
vec3_t* pvlightvalues = lighting;
int g_NormalIndex[4] = {2, 0, 1, 2};
int chrome[3][2] = {{10, 20}, {30, 40}, {50, 60}};
float lightpos[4][3][4]{};
float r_blend = 0.625f;
int lambertCalls = 0;

// A controlled math dependency with independent sensitivity to position,
// normal X/Y/Z and source light RGB. The production helper selects its inputs.
void R_LightLambert(float light[3][4], float* normal, float* source, float* result) {
    ++lambertCalls;
    result[0] = light[0][0] + normal[0] * 2.0f + source[0] * 3.0f;
    result[1] = light[1][1] + normal[1] * 5.0f + source[1] * 7.0f;
    result[2] = light[2][2] + normal[2] * 11.0f + source[2] * 13.0f;
}

#include CSE_STUDIO_ENGINE_EXTRACT

struct Array {
    const byte* pointer = nullptr;
    GLsizei stride = 0;
};
Array positions;
Array coordinates;
Array colors;
bool colorEnabled = false;
std::array<float, 4> currentColor{{0.13f, 0.27f, 0.41f, 0.55f}};
std::array<float, 2> currentUV{};
std::vector<DrawVertex> emitted;
int draws = 0;
int mutations = 0;

void APIENTRY Get(GLenum name, GLint* result) {
    *result = name == GL_MAX_CLIENT_ATTRIB_STACK_DEPTH ? 16 : 0;
}
void APIENTRY Push(GLbitfield mask) {
    Check(mask == GL_CLIENT_VERTEX_ARRAY_BIT, "client attrib mask");
    ++mutations;
}
void APIENTRY Pop() {}
void APIENTRY Enable(GLenum name) {
    if (name == GL_COLOR_ARRAY) {
        colorEnabled = true;
    }
}
void APIENTRY Disable(GLenum name) {
    if (name == GL_COLOR_ARRAY) {
        colorEnabled = false;
    }
}
void APIENTRY Position(GLint size, GLenum type, GLsizei stride, const void* pointer) {
    Check(size == 3 && type == GL_FLOAT && stride > 0, "position array descriptor");
    positions = {static_cast<const byte*>(pointer), stride};
}
void APIENTRY Coordinate(GLint size, GLenum type, GLsizei stride, const void* pointer) {
    Check(size == 2 && type == GL_FLOAT && stride > 0, "UV array descriptor");
    coordinates = {static_cast<const byte*>(pointer), stride};
}
void APIENTRY ColorArray(GLint size, GLenum type, GLsizei stride, const void* pointer) {
    Check(size == 4 && type == GL_FLOAT && stride > 0, "color array descriptor");
    colors = {static_cast<const byte*>(pointer), stride};
}
void APIENTRY Draw(GLenum mode, GLsizei count, GLenum type, const void* source) {
    Check(mode == GL_TRIANGLES && count == 9 && type == GL_UNSIGNED_INT, "one complete mesh draw");
    const auto* indices = static_cast<const std::uint32_t*>(source);
    for (GLsizei index = 0; index < count; ++index) {
        DrawVertex vertex{};
        const auto vertexIndex = indices[index];
        std::memcpy(vertex.position, positions.pointer + vertexIndex * positions.stride, sizeof(vertex.position));
        std::memcpy(vertex.texcoord, coordinates.pointer + vertexIndex * coordinates.stride, sizeof(vertex.texcoord));
        if (colorEnabled) {
            std::memcpy(vertex.color, colors.pointer + vertexIndex * colors.stride, sizeof(vertex.color));
        } else {
            std::memcpy(vertex.color, currentColor.data(), sizeof(vertex.color));
        }
        emitted.push_back(vertex);
    }
    // Exercise restoration after the GL specification's unspecified attributes.
    currentUV = {{-100, -100}};
    if (colorEnabled) {
        currentColor = {{-100, -100, -100, -100}};
    }
    ++draws;
}
void APIENTRY TexCoord(GLfloat s, GLfloat t) {
    currentUV = {{s, t}};
}
void APIENTRY Color(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    currentColor = {{r, g, b, a}};
}

GLFunctions Functions() {
    GLFunctions gl{};
    gl.GetIntegerv = Get;
    gl.PushClientAttrib = Push;
    gl.PopClientAttrib = Pop;
    gl.EnableClientState = Enable;
    gl.DisableClientState = Disable;
    gl.VertexPointer = Position;
    gl.TexCoordPointer = Coordinate;
    gl.ColorPointer = ColorArray;
    gl.DrawElements = Draw;
    gl.TexCoord2f = TexCoord;
    gl.Color4f = Color;
    return gl;
}

// These literal triangle corners are independent of MeshCache's strip/fan parser:
// strip A B C D becomes A B C, C B D; fan A E B becomes A E B.
constexpr std::array<int, 9> ExpectedCorners{{0, 1, 2, 2, 1, 3, 0, 4, 1}};
constexpr float ExpectedPositions[5][3] = {{11, 21, 31}, {12, 22, 32}, {13, 23, 33},
    {11, 21, 31}, {11, 21, 31}};
constexpr float ExpectedOrdinaryUV[5][2] = {{2, 4}, {6, 8}, {-10, 12}, {14, -16}, {18, 20}};
constexpr float ExpectedChromeUV[5][2] = {{10, 20}, {30, 40}, {50, 60}, {30, 40}, {10, 20}};
constexpr float ExpectedForcedUV[5][2] = {{50, 60}, {10, 20}, {30, 40}, {50, 60}, {50, 60}};
constexpr float ExpectedColors[5][3] = {{1.7f, 7.9f, 17.3f}, {4.2f, 6.5f, 25.5f},
    {3.5f, 17.1f, 11.7f}, {3.2f, 5.5f, 24.5f}, {1.7f, 7.9f, 17.3f}};
constexpr float ExpectedFlippedGreen[5] = {4.9f, 12.5f, 8.1f, 11.5f, 4.9f};

void RunAttributes(const GLFunctions& gl, const mstudiomesh_t& mesh, const vec3_t* normals,
    float s, float t, bool chromeTexture, bool forcedChrome, bool flipped, bool glowPositions) {
    emitted.clear();
    lambertCalls = 0;
    const auto initialColor = currentColor;
    const int priorDraws = draws;
    Check(R_StudioDrawIndexedMesh(gl, mesh, normals, s, t, chromeTexture, forcedChrome, flipped),
        "actual production helper accepts source mesh");
    Check(draws == priorDraws + 1 && emitted.size() == ExpectedCorners.size(), "batched complete output");
    constexpr std::size_t kUniqueVertices = 5;
    Check((useMetal ? studioNativeVertices.size() : studioDrawVertices.size()) == kUniqueVertices,
        "full tuple dedup preserves normal and UV seams");
    Check(lambertCalls == (forcedChrome ? 0 : 5), "light once per unique tuple and inherit forced color");
    const auto& expectedUV = forcedChrome ? ExpectedForcedUV : chromeTexture ? ExpectedChromeUV : ExpectedOrdinaryUV;
    for (std::size_t index = 0; index < emitted.size(); ++index) {
        const auto corner = ExpectedCorners[index];
        const auto& actual = emitted[index];
        for (int axis = 0; axis < 3; ++axis) {
            Equal(actual.position[axis], ExpectedPositions[corner][axis] + (glowPositions ? 100.0f : 0.0f),
                "transformed/glow output position");
            const float expectedColor = forcedChrome ? initialColor[axis] :
                axis == 1 && flipped && !chromeTexture ? ExpectedFlippedGreen[corner] : ExpectedColors[corner][axis];
            Equal(actual.color[axis], expectedColor, "independent position/normal light selection");
        }
        Equal(actual.texcoord[0], expectedUV[corner][0] * s, "runtime S/chrome selection");
        Equal(actual.texcoord[1], expectedUV[corner][1] * t, "runtime T/chrome selection");
        Equal(actual.color[3], forcedChrome ? initialColor[3] : r_blend, "runtime blend alpha");
    }
    // The final one-vertex primitive emits A, changing terminal attributes even
    // though the last actual triangle ends at B.
    Equal(currentUV[0], expectedUV[0][0] * s, "terminal source S");
    Equal(currentUV[1], expectedUV[0][1] * t, "terminal source T");
    for (int channel = 0; channel < 4; ++channel) {
        const float expected = forcedChrome ? initialColor[channel] : channel == 3 ? r_blend :
            channel == 1 && flipped && !chromeTexture ? ExpectedFlippedGreen[0] : ExpectedColors[0][channel];
        Equal(currentColor[channel], expected, "terminal source/inherited color");
    }
}
}

namespace cse::render {
bool UsingMetal() {
    return useMetal;
}

Status SubmitNativeIndexed(const std::vector<Vertex>& vertices, const std::vector<std::uint32_t>& indices,
    std::size_t lastVertex, bool perVertexColor) {
    Check(lastVertex < vertices.size(), "Metal terminal source vertex is in the batch");
    for (auto index : indices) {
        Check(index < vertices.size(), "Metal triangle index is in the batch");
        const auto& source = vertices[index];
        constexpr std::size_t kTexturePositionComponents = 2;
        Check(source.position[3] == 1 && source.texcoords[0][2] == 0 && source.texcoords[0][3] == 1,
            "Metal receives homogeneous position and UVs");
        DrawVertex vertex{};
        std::copy_n(source.position.data(), std::size(vertex.position), vertex.position);
        std::copy_n(source.texcoords[0].data(), kTexturePositionComponents, vertex.texcoord);
        const auto& color = perVertexColor ? source.color : currentColor;
        std::copy(color.begin(), color.end(), vertex.color);
        emitted.push_back(vertex);
    }
    const auto& last = vertices[lastVertex];
    TexCoord(last.texcoords[0][0], last.texcoords[0][1]);
    if (perVertexColor) {
        Color(last.color[0], last.color[1], last.color[2], last.color[3]);
    }
    ++draws;
    return Status::Ok;
}
}

int main(int argc, char** argv) {
    // Real studio structs and an allocation ending exactly at retained geometry.
    const std::vector<std::int16_t> commands = {4, 0, 0, 2, 4, 1, 1, 6, 8, 2, 2, -10, 12, 0, 1, 14, -16,
        -3, 0, 0, 2, 4, 0, 0, 18, 20, 1, 1, 6, 8, 1, 0, 0, 2, 4, 0};
    const std::size_t bytes = sizeof(studiohdr_t) + commands.size() * sizeof(commands[0]);
    const bool alignmentCase = argc == 2 && std::strcmp(argv[1], "alignment") == 0;
    auto storage = std::make_unique<byte[]>(bytes + (alignmentCase ? 1 : 0));
    std::memset(storage.get(), 0, bytes);
    pstudiohdr = new (storage.get()) studiohdr_t{};
    pstudiohdr->textureindex = 1;
    pstudiohdr->texturedataindex = static_cast<int>(bytes);
    pstudiohdr->length = static_cast<int>(bytes + 4096);
    mstudiomesh_t mesh{};
    mesh.triindex = sizeof(studiohdr_t);
    std::memcpy(storage.get() + mesh.triindex, commands.data(), commands.size() * sizeof(commands[0]));
    submodel.numverts = 4;
    submodel.numnorms = 3;
    const vec3_t normals[3] = {{0.2f, 0.3f, 0.4f}, {0.5f, -0.6f, 0.7f}, {-0.8f, 0.9f, -1.0f}};
    for (int position = 0; position < 4; ++position) {
        auxiliary[position].fv[0] = 11.0f + position;
        auxiliary[position].fv[1] = 21.0f + position;
        auxiliary[position].fv[2] = 31.0f + position;
        lightpos[position][0][0] = 1.0f + position;
        lightpos[position][1][1] = 5.0f + position;
        lightpos[position][2][2] = 9.0f + position;
    }
    const auto gl = Functions();
    if (alignmentCase) {
        ++mesh.triindex;
        ++pstudiohdr->texturedataindex;
        std::memcpy(storage.get() + mesh.triindex, commands.data(), commands.size() * sizeof(commands[0]));
        Check(!R_StudioDrawIndexedMesh(gl, mesh, normals, 0.5f, 0.25f, false, false, false),
            "odd triangle command byte offset refused");
        Check(mutations == 0, "misaligned stream does not touch driver state");
        std::puts("studio engine alignment passed");
        return EXIT_SUCCESS;
    }
    for (bool metal : {false, true}) {
        useMetal = metal;
        r_blend = 0.625f;
        studioDrawVertices.clear();
        studioNativeVertices.clear();
        for (int position = 0; position < 4; ++position) {
            auxiliary[position].fv[0] = 11.0f + position;
            auxiliary[position].fv[1] = 21.0f + position;
            auxiliary[position].fv[2] = 31.0f + position;
        }
        RunAttributes(gl, mesh, normals, 0.5f, 0.25f, false, false, false, false);
        r_blend = 0.375f;
        RunAttributes(gl, mesh, normals, 0.125f, 0.0625f, false, false, true, false);
        RunAttributes(gl, mesh, normals, 0.015625f, 0.0078125f, true, false, false, false);
        RunAttributes(gl, mesh, normals, 0.03125f, 0.015625f, true, false, true, false);
        currentColor = {{0.13f, 0.27f, 0.41f, 0.55f}};
        RunAttributes(gl, mesh, normals, 0.0625f, 0.03125f, true, true, true, false);
        for (auto& vertex : auxiliary) {
            for (auto& component : vertex.fv) {
                component += 100.0f;
            }
        }
        RunAttributes(gl, mesh, normals, 0.25f, 0.125f, false, false, false, true);

        Check(useMetal ? studioDrawVertices.empty() : studioNativeVertices.empty(),
            "only the selected renderer needs a vertex staging vector");
    }
    const int previousDraws = draws;
    const int previousMutations = mutations;
    // The terminator remains in allocated memory but outside resident geometry.
    pstudiohdr->texturedataindex -= sizeof(std::int16_t);
    Check(!R_StudioDrawIndexedMesh(gl, mesh, normals, 1, 1, false, false, false),
        "length cannot expose bytes beyond retained geometry");
    Check(draws == previousDraws && mutations == previousMutations, "truncated retained stream has no GL effects");
    std::puts("studio engine attributes passed");
}
