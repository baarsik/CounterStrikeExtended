// Real fixed-function oracle: the reference submits the original command stream.
// This catches winding/provoking-vertex changes, attribute seams and leaked GL state.
#define GL_SILENCE_DEPRECATION
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include "studio_mesh_batch.h"
#include "studio_mesh_gl.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr int ImageSize = 128;
constexpr int Components = 4;
constexpr int WordsPerVertex = 4;
constexpr int MaximumModelTableEntries = 256;
constexpr int BenchmarkRepeats = 30;
constexpr int BenchmarkSamples = 5;
using cse::studio::DrawVertex;
using cse::studio::MeshTopology;
using Commands = std::vector<std::int16_t>;
using TextureFunction = void (APIENTRY*)(GLenum);
using SecondaryPointerFunction = void (APIENTRY*)(GLint, GLenum, GLsizei, const void*);
using FogPointerFunction = void (APIENTRY*)(GLenum, GLsizei, const void*);
TextureFunction ClientTexture = nullptr;
TextureFunction ServerTexture = nullptr;
SecondaryPointerFunction SecondaryPointer = nullptr;
FogPointerFunction FogPointer = nullptr;

void Require(bool value, const std::string& message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

DrawVertex Attributes(const cse::studio::MeshVertex& vertex) {
    // Every field participates: position-only indexing visibly loses normals/UV seams.
    const int column = vertex.position % 8;
    const int row = vertex.position / 8;
    DrawVertex result{};
    result.position[0] = -0.9f + static_cast<float>(column) * 0.25f;
    result.position[1] = -0.8f + static_cast<float>(row % 8) * 0.22f;
    result.position[2] = static_cast<float>(vertex.position % 3) * 0.03f;
    result.texcoord[0] = static_cast<float>(vertex.s) / 64.0f;
    result.texcoord[1] = static_cast<float>(vertex.t) / 64.0f;
    result.color[0] = 0.13f + static_cast<float>(vertex.normal % 7) * 0.12f;
    result.color[1] = 0.2f + static_cast<float>(vertex.position % 5) * 0.15f;
    result.color[2] = 0.23f + static_cast<float>(vertex.normal % 4) * 0.17f;
    result.color[3] = 0.61f;
    return result;
}

void Immediate(const Commands& commands, bool color) {
    std::size_t cursor = 0;
    while (commands[cursor] != 0) {
        const int command = commands[cursor++];
        const int count = command < 0 ? -command : command;
        glBegin(command < 0 ? GL_TRIANGLE_FAN : GL_TRIANGLE_STRIP);
        for (int index = 0; index < count; ++index, cursor += WordsPerVertex) {
            const auto vertex = Attributes({commands[cursor], commands[cursor + 1],
                commands[cursor + 2], commands[cursor + 3]});
            if (color) {
                glColor4fv(vertex.color);
            }
            glTexCoord2fv(vertex.texcoord);
            glVertex3fv(vertex.position);
        }
        glEnd();
    }
}

std::vector<DrawVertex> BuildAttributes(const MeshTopology& topology) {
    std::vector<DrawVertex> result;
    result.reserve(topology.vertices.size());
    for (const auto& vertex : topology.vertices) {
        result.push_back(Attributes(vertex));
    }
    return result;
}

struct ClientState {
    GLint active = 0;
    std::array<GLboolean, 7> enabled{};
    std::array<void*, 7> pointers{};
    std::array<GLint, 17> descriptors{};
    std::array<GLboolean, 2> textureEnabled{};
    std::array<void*, 2> texturePointers{};
    std::array<GLint, 6> textureDescriptors{};

    bool operator==(const ClientState& other) const {
        return active == other.active && enabled == other.enabled && pointers == other.pointers &&
            descriptors == other.descriptors && textureEnabled == other.textureEnabled &&
            texturePointers == other.texturePointers && textureDescriptors == other.textureDescriptors;
    }
};

ClientState CaptureClientState() {
    ClientState result;
    glGetIntegerv(GL_CLIENT_ACTIVE_TEXTURE, &result.active);
    constexpr GLenum Arrays[] = {GL_VERTEX_ARRAY, GL_COLOR_ARRAY, GL_NORMAL_ARRAY,
        GL_EDGE_FLAG_ARRAY, GL_INDEX_ARRAY, GL_SECONDARY_COLOR_ARRAY, GL_FOG_COORD_ARRAY};
    constexpr GLenum Pointers[] = {GL_VERTEX_ARRAY_POINTER, GL_COLOR_ARRAY_POINTER, GL_NORMAL_ARRAY_POINTER,
        GL_EDGE_FLAG_ARRAY_POINTER, GL_INDEX_ARRAY_POINTER, GL_SECONDARY_COLOR_ARRAY_POINTER,
        GL_FOG_COORD_ARRAY_POINTER};
    constexpr GLenum Descriptors[] = {GL_VERTEX_ARRAY_SIZE, GL_VERTEX_ARRAY_TYPE, GL_VERTEX_ARRAY_STRIDE,
        GL_COLOR_ARRAY_SIZE, GL_COLOR_ARRAY_TYPE, GL_COLOR_ARRAY_STRIDE, GL_NORMAL_ARRAY_TYPE,
        GL_NORMAL_ARRAY_STRIDE, GL_EDGE_FLAG_ARRAY_STRIDE, GL_INDEX_ARRAY_TYPE, GL_INDEX_ARRAY_STRIDE,
        GL_ARRAY_BUFFER_BINDING, GL_SECONDARY_COLOR_ARRAY_SIZE, GL_SECONDARY_COLOR_ARRAY_TYPE,
        GL_SECONDARY_COLOR_ARRAY_STRIDE, GL_FOG_COORD_ARRAY_TYPE, GL_FOG_COORD_ARRAY_STRIDE};
    for (std::size_t index = 0; index < result.enabled.size(); ++index) {
        result.enabled[index] = glIsEnabled(Arrays[index]);
        glGetPointerv(Pointers[index], &result.pointers[index]);
    }
    for (std::size_t index = 0; index < result.descriptors.size(); ++index) {
        glGetIntegerv(Descriptors[index], &result.descriptors[index]);
    }
    for (int unit = 0; unit < 2; ++unit) {
        ClientTexture(GL_TEXTURE0 + unit);
        result.textureEnabled[unit] = glIsEnabled(GL_TEXTURE_COORD_ARRAY);
        glGetPointerv(GL_TEXTURE_COORD_ARRAY_POINTER, &result.texturePointers[unit]);
        glGetIntegerv(GL_TEXTURE_COORD_ARRAY_SIZE, &result.textureDescriptors[unit * 3]);
        glGetIntegerv(GL_TEXTURE_COORD_ARRAY_TYPE, &result.textureDescriptors[unit * 3 + 1]);
        glGetIntegerv(GL_TEXTURE_COORD_ARRAY_STRIDE, &result.textureDescriptors[unit * 3 + 2]);
    }
    ClientTexture(static_cast<GLenum>(result.active));
    return result;
}

void EstablishForeignClientState() {
    static const GLfloat data[16][12] = {};
    static const GLboolean edges[16] = {};
    glVertexPointer(2, GL_FLOAT, sizeof(data[0]), data);
    glColorPointer(3, GL_FLOAT, sizeof(data[0]), data[0] + 2);
    glNormalPointer(GL_FLOAT, sizeof(data[0]), data[0] + 5);
    glIndexPointer(GL_FLOAT, sizeof(data[0]), data[0] + 8);
    glEdgeFlagPointer(0, edges);
    SecondaryPointer(3, GL_FLOAT, sizeof(data[0]), data[0] + 2);
    FogPointer(GL_FLOAT, sizeof(data[0]), data[0] + 11);
    glEnableClientState(GL_SECONDARY_COLOR_ARRAY);
    glEnableClientState(GL_FOG_COORD_ARRAY);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_INDEX_ARRAY);
    glEnableClientState(GL_EDGE_FLAG_ARRAY);
    for (int unit = 0; unit < 2; ++unit) {
        ClientTexture(GL_TEXTURE0 + unit);
        glTexCoordPointer(3, GL_FLOAT, sizeof(data[0]), data[0] + unit);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    }
    ClientTexture(GL_TEXTURE1);
}

struct RenderCase {
    const char* name;
    bool flat;
    bool cull;
    bool additive;
    bool masked;
    bool color;
};

void Configure(const RenderCase& test) {
    glDisable(GL_DITHER);
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_FOG);
    glDisable(GL_SCISSOR_TEST);
    glShadeModel(test.flat ? GL_FLAT : GL_SMOOTH);
    if (test.cull) {
        glEnable(GL_CULL_FACE);
    } else {
        glDisable(GL_CULL_FACE);
    }
    glCullFace(GL_FRONT);
    if (test.additive) {
        glEnable(GL_BLEND);
    } else {
        glDisable(GL_BLEND);
    }
    glBlendFunc(GL_ONE, GL_ONE);
    if (test.masked) {
        glEnable(GL_ALPHA_TEST);
    } else {
        glDisable(GL_ALPHA_TEST);
    }
    glAlphaFunc(GL_GREATER, 0.5f);
    ServerTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glColor4f(0.31f, 0.57f, 0.73f, 0.81f);
    glTexCoord2f(0.17f, 0.29f);
    glClearColor(0.07f, 0.11f, 0.16f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

std::vector<unsigned char> Pixels() {
    std::vector<unsigned char> result(ImageSize * ImageSize * Components);
    glReadPixels(0, 0, ImageSize, ImageSize, GL_RGBA, GL_UNSIGNED_BYTE, result.data());
    return result;
}

cse::studio::GLFunctions Functions() {
    cse::studio::GLFunctions result{};
    result.GetIntegerv = glGetIntegerv;
    result.PushClientAttrib = glPushClientAttrib;
    result.PopClientAttrib = glPopClientAttrib;
    result.DisableClientState = glDisableClientState;
    result.EnableClientState = glEnableClientState;
    result.VertexPointer = glVertexPointer;
    result.TexCoordPointer = glTexCoordPointer;
    result.ColorPointer = glColorPointer;
    result.DrawElements = glDrawElements;
    result.TexCoord2f = glTexCoord2f;
    result.Color4f = glColor4f;
    result.ClientActiveTexture = ClientTexture;
    result.bufferObjects = true;
    result.shaders = true;
    result.extendedArrays = true;
    result.textureUnits = 2;
    return result;
}

void CheckCase(const Commands& commands, const MeshTopology& topology,
    const cse::studio::GLFunctions& functions, const RenderCase& test) {
    Configure(test);
    EstablishForeignClientState();
    const auto before = CaptureClientState();
    Immediate(commands, test.color);
    const auto expected = Pixels();
    std::array<float, 4> expectedColor{};
    std::array<float, 4> expectedUV{};
    glGetFloatv(GL_CURRENT_COLOR, expectedColor.data());
    glGetFloatv(GL_CURRENT_TEXTURE_COORDS, expectedUV.data());
    const auto attributes = BuildAttributes(topology);
    Configure(test);
    EstablishForeignClientState();
    Require(cse::studio::SubmitIndexed(functions, topology, attributes.data(), test.color),
        std::string(test.name) + ": indexed submission rejected");
    const auto actual = Pixels();
    std::array<float, 4> actualColor{};
    std::array<float, 4> actualUV{};
    glGetFloatv(GL_CURRENT_COLOR, actualColor.data());
    glGetFloatv(GL_CURRENT_TEXTURE_COORDS, actualUV.data());
    std::size_t differences = 0;
    for (std::size_t index = 0; index < actual.size(); ++index) {
        differences += actual[index] != expected[index];
    }
    Require(differences == 0, std::string(test.name) + ": " + std::to_string(differences) +
        " differing pixel components");
    Require(CaptureClientState() == before, std::string(test.name) + ": client arrays leaked");
    Require(actualColor == expectedColor, std::string(test.name) + ": terminal color changed");
    Require(actualUV == expectedUV, std::string(test.name) + ": terminal UV changed");
    Require(glGetError() == GL_NO_ERROR, std::string(test.name) + ": OpenGL error");
    std::printf("PASS pixels/state %s\n", test.name);
}

Commands Fixture() {
    return {6, 0, 0, 0, 0, 8, 1, 0, 64, 1, 2, 64, 0, 9, 3, 64, 64,
        2, 4, 0, 0, 10, 5, 0, 64,
        -6, 28, 6, 0, 0, 19, 7, 64, 0, 21, 8, 0, 64, 37, 9, 64, 64,
        35, 10, 64, 0, 19, 11, 0, 64,
        3, 0, 12, 64, 64, 8, 13, 64, 0, 1, 14, 0, 64,
        // An incomplete final primitive still changes current attributes.
        2, 4, 15, 64, 0, 5, 16, 0, 64, 0};
}

Commands BenchmarkFixture() {
    constexpr int Strips = 128;
    constexpr int StripVertices = 16;
    Commands result;
    for (int strip = 0; strip < Strips; ++strip) {
        result.push_back(StripVertices);
        for (int vertex = 0; vertex < StripVertices; ++vertex) {
            result.push_back(static_cast<std::int16_t>((vertex / 2) + (vertex % 2) * 8));
            result.push_back(static_cast<std::int16_t>(vertex % 7));
            result.push_back(static_cast<std::int16_t>((vertex % 2) * 64));
            result.push_back(static_cast<std::int16_t>((vertex / 2 % 2) * 64));
        }
    }
    result.push_back(0);
    return result;
}

struct BenchmarkMesh {
    Commands commands;
    int vertexCount = 64;
    int normalCount = 64;
};

void Benchmark(const cse::studio::GLFunctions& functions, const std::string& name,
    const std::vector<BenchmarkMesh>& meshes) {
    cse::studio::MeshCache cache;
    std::vector<std::vector<DrawVertex>> attributes;
    std::size_t triangleCount = 0;
    std::size_t uniqueCount = 0;
    std::size_t commandCount = 0;
    for (const auto& mesh : meshes) {
        const auto* topology = cache.Get(mesh.commands.data(), mesh.commands.size(),
            mesh.vertexCount, mesh.normalCount);
        Require(topology != nullptr, "benchmark topology rejected: " + name);
        attributes.push_back(BuildAttributes(*topology));
        triangleCount += topology->indices.size() / 3;
        uniqueCount += topology->vertices.size();
        std::size_t cursor = 0;
        while (mesh.commands[cursor] != 0) {
            const int count = mesh.commands[cursor++];
            cursor += static_cast<std::size_t>(count < 0 ? -count : count) * WordsPerVertex;
            ++commandCount;
        }
    }
    constexpr RenderCase State{"benchmark", false, false, false, false, true};
    Configure(State);
    std::array<double, 2> times{};
    // Include cache validation and per-draw attribute construction in indexed timing.
    for (int sample = 0; sample < BenchmarkSamples; ++sample) {
        for (int order = 0; order < 2; ++order) {
            const int mode = (order + sample) % 2;
            glFinish();
            const auto start = std::chrono::steady_clock::now();
            for (int repeat = 0; repeat < BenchmarkRepeats; ++repeat) {
                for (std::size_t meshIndex = 0; meshIndex < meshes.size(); ++meshIndex) {
                    const auto& mesh = meshes[meshIndex];
                    if (mode == 0) {
                        Immediate(mesh.commands, true);
                    } else {
                        const auto* warm = cache.Get(mesh.commands.data(), mesh.commands.size(),
                            mesh.vertexCount, mesh.normalCount);
                        Require(warm != nullptr, "warm benchmark topology rejected");
                        auto& output = attributes[meshIndex];
                        for (std::size_t index = 0; index < warm->vertices.size(); ++index) {
                            output[index] = Attributes(warm->vertices[index]);
                        }
                        Require(cse::studio::SubmitIndexed(functions, *warm, output.data(), true),
                            "benchmark submission rejected");
                    }
                }
            }
            glFinish();
            times[mode] += std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - start).count();
        }
    }
    const double count = BenchmarkSamples * BenchmarkRepeats;
    std::printf("BENCH %s: %zu meshes, %zu commands, %zu triangles, %zu unique tuples; "
        "immediate %.2f us/model, indexed %.2f us/model, ratio %.2fx\n", name.c_str(),
        meshes.size(), commandCount, triangleCount, uniqueCount, times[0] / count, times[1] / count,
        times[0] / times[1]);
}

// The MDL reader uses the version-10 on-disk layout, never host pointer-sized structs.
std::vector<BenchmarkMesh> ReadModel(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    Require(static_cast<bool>(file), "cannot open model: " + path);
    const std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()};
    const auto word = [&](std::size_t offset) {
        Require(offset <= bytes.size() && bytes.size() - offset >= sizeof(std::int32_t),
            "truncated MDL integer");
        const auto value = static_cast<std::uint32_t>(bytes[offset]) |
            static_cast<std::uint32_t>(bytes[offset + 1]) << 8 |
            static_cast<std::uint32_t>(bytes[offset + 2]) << 16 |
            static_cast<std::uint32_t>(bytes[offset + 3]) << 24;
        return static_cast<std::int32_t>(value);
    };
    constexpr std::int32_t StudioMagic = 0x54534449; // IDST
    constexpr std::int32_t StudioVersion = 10;
    constexpr std::size_t BodyCountOffset = 204;
    constexpr std::size_t BodyIndexOffset = 208;
    constexpr std::size_t BodySize = 76;
    constexpr std::size_t BodyModelsOffset = 64;
    constexpr std::size_t BodyModelIndexOffset = 72;
    constexpr std::size_t MeshCountOffset = 72;
    constexpr std::size_t MeshIndexOffset = 76;
    constexpr std::size_t VertexCountOffset = 80;
    constexpr std::size_t NormalCountOffset = 92;
    constexpr std::size_t MeshSize = 20;
    constexpr std::size_t CommandIndexOffset = 4;
    Require(word(0) == StudioMagic && word(4) == StudioVersion, "unsupported MDL version");
    const int bodyCount = word(BodyCountOffset);
    const int bodyIndex = word(BodyIndexOffset);
    Require(bodyCount >= 0 && bodyCount <= MaximumModelTableEntries && bodyIndex >= 0, "invalid MDL body table");
    std::vector<BenchmarkMesh> result;
    for (int body = 0; body < bodyCount; ++body) {
        const auto part = static_cast<std::size_t>(bodyIndex) + body * BodySize;
        if (word(part + BodyModelsOffset) == 0) {
            continue;
        }
        const int model = word(part + BodyModelIndexOffset);
        Require(model >= 0, "invalid MDL model offset");
        const int meshCount = word(model + MeshCountOffset);
        const int meshIndex = word(model + MeshIndexOffset);
        Require(meshCount >= 0 && meshCount <= MaximumModelTableEntries && meshIndex >= 0, "invalid MDL mesh table");
        for (int index = 0; index < meshCount; ++index) {
            const auto mesh = static_cast<std::size_t>(meshIndex) + index * MeshSize;
            const int commandIndex = word(mesh + CommandIndexOffset);
            Require(commandIndex >= 0, "invalid MDL command offset");
            BenchmarkMesh output;
            output.vertexCount = word(model + VertexCountOffset);
            output.normalCount = word(model + NormalCountOffset);
            std::size_t cursor = static_cast<std::size_t>(commandIndex);
            while (true) {
                Require(cursor <= bytes.size() && bytes.size() - cursor >= sizeof(std::int16_t),
                    "truncated MDL command");
                const auto value = static_cast<std::int16_t>(bytes[cursor] |
                    static_cast<unsigned int>(bytes[cursor + 1]) << 8);
                output.commands.push_back(value);
                cursor += sizeof(std::int16_t);
                if (value == 0) {
                    break;
                }
                const int vertices = value < 0 ? -static_cast<int>(value) : value;
                const auto words = static_cast<std::size_t>(vertices) * WordsPerVertex;
                Require(words <= (bytes.size() - cursor) / sizeof(std::int16_t), "truncated MDL tuple");
                for (std::size_t tupleWord = 0; tupleWord < words; ++tupleWord) {
                    output.commands.push_back(static_cast<std::int16_t>(bytes[cursor] |
                        static_cast<unsigned int>(bytes[cursor + 1]) << 8));
                    cursor += sizeof(std::int16_t);
                }
            }
            result.push_back(std::move(output));
        }
    }
    return result;
}
}

int main(int argc, char** argv) {
    SDL_Window* window = nullptr;
    SDL_GLContext context = nullptr;
    int status = 1;
    try {
        Require(SDL_Init(SDL_INIT_VIDEO), SDL_GetError());
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);
        SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
        window = SDL_CreateWindow("CSE bounded studio oracle", ImageSize, ImageSize,
            SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
        Require(window != nullptr, SDL_GetError());
        context = SDL_GL_CreateContext(window);
        Require(context != nullptr, SDL_GetError());
        ClientTexture = reinterpret_cast<TextureFunction>(SDL_GL_GetProcAddress("glClientActiveTexture"));
        ServerTexture = reinterpret_cast<TextureFunction>(SDL_GL_GetProcAddress("glActiveTexture"));
        SecondaryPointer = reinterpret_cast<SecondaryPointerFunction>(
            SDL_GL_GetProcAddress("glSecondaryColorPointer"));
        FogPointer = reinterpret_cast<FogPointerFunction>(SDL_GL_GetProcAddress("glFogCoordPointer"));
        Require(ClientTexture && ServerTexture && SecondaryPointer && FogPointer,
            "OpenGL2.1 array entry points unavailable");
        std::printf("GL %s / %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));
        glViewport(0, 0, ImageSize, ImageSize);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        GLuint texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        constexpr unsigned char Checker[] = {255, 40, 80, 255, 20, 255, 90, 0,
            60, 110, 255, 255, 255, 240, 50, 255};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, Checker);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        const auto commands = Fixture();
        cse::studio::MeshCache cache;
        const auto* topology = cache.Get(commands.data(), commands.size(), 64, 64);
        Require(topology != nullptr, "fixture topology rejected");
        const auto functions = Functions();
        constexpr RenderCase Cases[] = {
            {"smooth strip/fan/seams", false, false, false, false, true},
            {"flat provoking vertex", true, false, false, false, true},
            {"front culling", false, true, false, false, true},
            {"flat with culling", true, true, false, false, true},
            {"additive", false, false, true, false, true},
            {"masked", false, false, false, true, true},
            {"masked additive flat", true, false, true, true, true},
            {"forced chrome constant color", false, false, true, false, false}
        };
        for (const auto& test : Cases) {
            CheckCase(commands, *topology, functions, test);
        }
        Benchmark(functions, "synthetic repeated strips", {{BenchmarkFixture(), 64, 64}});
        for (int argument = 1; argument < argc; ++argument) {
            Benchmark(functions, argv[argument], ReadModel(argv[argument]));
        }
        std::puts("Timings include CPU synthetic attributes, warm cache verification, submission and glFinish. "
            "MDLs supply default-body topology only; no game animation, textures, lighting or gameplay.");
        glDeleteTextures(1, &texture);
        Require(glGetError() == GL_NO_ERROR, "final OpenGL error");
        status = 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL %s\n", error.what());
    }
    if (context) {
        SDL_GL_DestroyContext(context);
    }
    if (window) {
        SDL_DestroyWindow(window);
    }
    SDL_Quit();
    return status;
}
