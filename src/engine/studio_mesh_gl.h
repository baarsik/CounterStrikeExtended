#pragma once

#include "studio_mesh_batch.h"

#if defined(_WIN32)
#include <Windows.h>
#include <GL/gl.h>
#else
#include <SDL3/SDL_opengl.h>
#endif

namespace cse::studio {

struct DrawVertex {
    float position[3];
    float texcoord[2];
    float color[4];
};

struct GLFunctions {
    decltype(&glGetIntegerv) GetIntegerv = nullptr;
    decltype(&glPushClientAttrib) PushClientAttrib = nullptr;
    decltype(&glPopClientAttrib) PopClientAttrib = nullptr;
    decltype(&glDisableClientState) DisableClientState = nullptr;
    decltype(&glEnableClientState) EnableClientState = nullptr;
    decltype(&glVertexPointer) VertexPointer = nullptr;
    decltype(&glTexCoordPointer) TexCoordPointer = nullptr;
    decltype(&glColorPointer) ColorPointer = nullptr;
    decltype(&glDrawElements) DrawElements = nullptr;
    decltype(&glTexCoord2f) TexCoord2f = nullptr;
    decltype(&glColor4f) Color4f = nullptr;
    void (APIENTRY* ClientActiveTexture)(GLenum) = nullptr;
    bool bufferObjects = false;
    bool shaders = false;
    bool extendedArrays = false;
    int textureUnits = 1;
};

inline bool SubmitIndexed(const GLFunctions& gl, const MeshTopology& mesh,
    const DrawVertex* vertices, bool perVertexColor) {
    if (!gl.GetIntegerv || !gl.PushClientAttrib || !gl.PopClientAttrib || !gl.DisableClientState ||
        !gl.EnableClientState || !gl.VertexPointer || !gl.TexCoordPointer || !gl.ColorPointer ||
        !gl.DrawElements || !gl.TexCoord2f || !gl.Color4f ||
        (gl.textureUnits > 1 && !gl.ClientActiveTexture)) {
        return false;
    }
    if (mesh.vertices.empty()) {
        return true;
    }
    if (!vertices || mesh.lastVertex >= mesh.vertices.size() ||
        mesh.indices.size() > static_cast<std::size_t>((std::numeric_limits<GLsizei>::max)())) {
        return false;
    }
    // Client addresses must never be interpreted as offsets into a mod's VBO/EBO.
    // These enums are core since GL1.5/2.0; query only when that capability exists.
    constexpr GLenum ArrayBufferBinding = 0x8894;
    constexpr GLenum ElementArrayBufferBinding = 0x8895;
    constexpr GLenum CurrentProgram = 0x8B8D;
    GLint state = 0;
    if (gl.bufferObjects) {
        gl.GetIntegerv(ArrayBufferBinding, &state);
        if (state != 0) {
            return false;
        }
        gl.GetIntegerv(ElementArrayBufferBinding, &state);
        if (state != 0) {
            return false;
        }
    }
    if (gl.shaders) {
        gl.GetIntegerv(CurrentProgram, &state);
        if (state != 0) {
            return false;
        }
    }
    gl.GetIntegerv(GL_LIST_INDEX, &state);
    if (state != 0) {
        return false;
    }
    GLint maximumDepth = 0;
    gl.GetIntegerv(GL_CLIENT_ATTRIB_STACK_DEPTH, &state);
    gl.GetIntegerv(GL_MAX_CLIENT_ATTRIB_STACK_DEPTH, &maximumDepth);
    if (state >= maximumDepth) {
        return false;
    }

    gl.PushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    gl.DisableClientState(GL_NORMAL_ARRAY);
    gl.DisableClientState(GL_INDEX_ARRAY);
    gl.DisableClientState(GL_EDGE_FLAG_ARRAY);
    if (gl.extendedArrays) {
        // Core GL1.4 secondary color/fog arrays must not fetch stale client memory.
        constexpr GLenum FogCoordinateArray = 0x8457;
        constexpr GLenum SecondaryColorArray = 0x845E;
        gl.DisableClientState(FogCoordinateArray);
        gl.DisableClientState(SecondaryColorArray);
    }
    if (gl.ClientActiveTexture) {
        constexpr GLenum TextureUnitZero = 0x84C0; // GL_TEXTURE0 / GL_TEXTURE0_ARB
        for (int unit = gl.textureUnits - 1; unit > 0; --unit) {
            gl.ClientActiveTexture(TextureUnitZero + unit);
            gl.DisableClientState(GL_TEXTURE_COORD_ARRAY);
        }
        gl.ClientActiveTexture(TextureUnitZero);
    }
    gl.EnableClientState(GL_VERTEX_ARRAY);
    gl.EnableClientState(GL_TEXTURE_COORD_ARRAY);
    gl.VertexPointer(3, GL_FLOAT, sizeof(DrawVertex), vertices[0].position);
    gl.TexCoordPointer(2, GL_FLOAT, sizeof(DrawVertex), vertices[0].texcoord);
    if (perVertexColor) {
        gl.EnableClientState(GL_COLOR_ARRAY);
        gl.ColorPointer(4, GL_FLOAT, sizeof(DrawVertex), vertices[0].color);
    } else {
        // Forced chrome deliberately inherits the caller's shell color.
        gl.DisableClientState(GL_COLOR_ARRAY);
    }
    if (!mesh.indices.empty()) {
        gl.DrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.indices.size()), GL_UNSIGNED_INT,
            mesh.indices.data());
    }
    gl.PopClientAttrib();

    // Array draws leave enabled current attributes unspecified. Immediate mode
    // left the last source vertex's attributes, even for primitives with <3 verts.
    const auto& last = vertices[mesh.lastVertex];
    gl.TexCoord2f(last.texcoord[0], last.texcoord[1]);
    if (perVertexColor) {
        gl.Color4f(last.color[0], last.color[1], last.color[2], last.color[3]);
    }
    return true;
}

}
