#include "studio_mesh_gl.h"
#include <SDL3/SDL_opengl_glext.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

namespace {
using namespace cse::studio;
void Check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}
std::map<GLenum, bool> arrays;
std::map<GLenum, bool> savedArrays;
const void* vertexPointer = nullptr;
const void* savedPointer = nullptr;
const void* colorPointer = nullptr;
std::array<float, 4> color{{0.2f, 0.4f, 0.6f, 0.8f}};
std::array<float, 2> uv{};
int draws = 0;
int pushes = 0;
int buffer = 0;
int program = 0;
int list = 0;
void APIENTRY GetIntegerv(GLenum name, GLint* value) {
    *value = 0;
    if (name == GL_ARRAY_BUFFER_BINDING || name == GL_ELEMENT_ARRAY_BUFFER_BINDING) {
        *value = buffer;
    } else if (name == GL_CURRENT_PROGRAM) {
        *value = program;
    } else if (name == GL_LIST_INDEX) {
        *value = list;
    } else if (name == GL_MAX_CLIENT_ATTRIB_STACK_DEPTH) {
        *value = 16;
    }
}
void APIENTRY Push(GLbitfield mask) {
    Check(mask == GL_CLIENT_VERTEX_ARRAY_BIT, "save client arrays only");
    ++pushes;
    savedArrays = arrays;
    savedPointer = vertexPointer;
}
void APIENTRY Pop() {
    arrays = savedArrays;
    vertexPointer = savedPointer;
}
void APIENTRY Disable(GLenum name) { arrays[name] = false; }
void APIENTRY Enable(GLenum name) { arrays[name] = true; }
void APIENTRY VertexPointer(GLint size, GLenum type, GLsizei stride, const void* pointer) {
    Check(size == 3 && type == GL_FLOAT && stride == sizeof(DrawVertex), "float XYZ layout");
    vertexPointer = pointer;
}
void APIENTRY TexCoordPointer(GLint size, GLenum type, GLsizei stride, const void*) {
    Check(size == 2 && type == GL_FLOAT && stride == sizeof(DrawVertex), "float UV layout");
}
void APIENTRY ColorPointer(GLint size, GLenum type, GLsizei stride, const void* pointer) {
    Check(size == 4 && type == GL_FLOAT && stride == sizeof(DrawVertex), "retain float color precision");
    colorPointer = pointer;
}
void APIENTRY Draw(GLenum mode, GLsizei count, GLenum type, const void* indices) {
    Check(mode == GL_TRIANGLES && count == 3 && type == GL_UNSIGNED_INT, "one indexed triangle draw");
    Check(static_cast<const std::uint32_t*>(indices)[2] == 2, "original provoking vertex");
    Check(arrays[GL_VERTEX_ARRAY] && arrays[GL_TEXTURE_COORD_ARRAY], "required arrays enabled");
    Check(!arrays[GL_NORMAL_ARRAY] && !arrays[GL_EDGE_FLAG_ARRAY] && !arrays[GL_INDEX_ARRAY],
        "stale unrelated arrays disabled during draw");
    Check(!arrays[GL_FOG_COORD_ARRAY] && !arrays[GL_SECONDARY_COLOR_ARRAY], "extended stale arrays disabled");
    if (arrays[GL_COLOR_ARRAY]) {
        Check(colorPointer != nullptr, "per-vertex colors supplied");
        color = {{-1, -1, -1, -1}};
    }
    uv = {{-1, -1}}; // Enabled array current attributes become unspecified after drawing.
    ++draws;
}
void APIENTRY TexCoord(GLfloat s, GLfloat t) { uv = {{s, t}}; }
void APIENTRY Color(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { color = {{r, g, b, a}}; }
}

int main() {
    GLFunctions gl{};
    gl.GetIntegerv = GetIntegerv;
    gl.PushClientAttrib = Push;
    gl.PopClientAttrib = Pop;
    gl.DisableClientState = Disable;
    gl.EnableClientState = Enable;
    gl.VertexPointer = VertexPointer;
    gl.TexCoordPointer = TexCoordPointer;
    gl.ColorPointer = ColorPointer;
    gl.DrawElements = Draw;
    gl.TexCoord2f = TexCoord;
    gl.Color4f = Color;
    gl.bufferObjects = true;
    gl.shaders = true;
    gl.extendedArrays = true;
    MeshTopology mesh;
    mesh.vertices.resize(3);
    mesh.indices = {0, 1, 2};
    mesh.lastVertex = 0; // Last source vertex can differ from last triangle index.
    DrawVertex vertices[3]{};
    vertices[0].texcoord[0] = 0.25f;
    vertices[0].texcoord[1] = 0.75f;
    vertices[0].color[0] = 0.123456f;
    vertices[0].color[3] = 0.5f;
    arrays = {{GL_VERTEX_ARRAY, false}, {GL_TEXTURE_COORD_ARRAY, true}, {GL_COLOR_ARRAY, true},
        {GL_NORMAL_ARRAY, true}, {GL_INDEX_ARRAY, true}, {GL_EDGE_FLAG_ARRAY, true},
        {GL_FOG_COORD_ARRAY, true}, {GL_SECONDARY_COLOR_ARRAY, true}};
    const auto originalArrays = arrays;
    vertexPointer = reinterpret_cast<const void*>(static_cast<std::uintptr_t>(1234));
    const auto originalPointer = vertexPointer;
    Check(SubmitIndexed(gl, mesh, vertices, true), "supported mesh submitted");
    Check(draws == 1 && pushes == 1, "single batch");
    Check(arrays == originalArrays && vertexPointer == originalPointer, "all client state restored");
    Check(uv == std::array<float, 2>{{0.25f, 0.75f}}, "terminal source UV restored");
    Check(color[0] == 0.123456f && color[3] == 0.5f, "terminal source float color restored");
    const auto shellColor = color;
    Check(SubmitIndexed(gl, mesh, vertices, false), "forced chrome submitted");
    Check(color == shellColor, "forced chrome preserves caller color");
    mesh.indices.clear();
    vertices[0].texcoord[0] = 0.5f;
    Check(SubmitIndexed(gl, mesh, vertices, true) && draws == 2, "zero triangles update attributes without draw");
    Check(uv[0] == 0.5f, "short source primitive terminal UV");
    const int previousPushes = pushes;
    buffer = 7;
    Check(!SubmitIndexed(gl, mesh, vertices, true), "bound VBO/EBO uses immediate fallback");
    buffer = 0;
    program = 3;
    Check(!SubmitIndexed(gl, mesh, vertices, true), "custom shader uses immediate fallback");
    program = 0;
    list = 1;
    Check(!SubmitIndexed(gl, mesh, vertices, true), "display-list compilation uses immediate fallback");
    list = 0;
    gl.DrawElements = nullptr;
    Check(!SubmitIndexed(gl, mesh, vertices, true), "missing driver slot uses immediate fallback");
    Check(pushes == previousPushes, "refused paths have no state mutation");
    std::puts("studio submission passed");
}
