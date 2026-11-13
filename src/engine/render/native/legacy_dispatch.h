#pragma once

#include "frame_stream.h"
#include <SDL3/SDL_opengl.h>
#include <string>

namespace cse {
namespace render {

// Private GL dispatch; CaptureOnly remains a CPU fixture mode until backend admission.
enum class DispatchMode { ReferenceGL, ShadowCapture, CaptureOnly };
using LegacyProcedure = void (APIENTRY*)();
class Session; // T3 owns the synchronous backend lifecycle; no backend is selected here.

// Borrowed typed bridge contract for T3. T2a does not bind a backend or implement a barrier.
// The Session and callback must outlive a future binding; no borrowed pixel output is retained.
struct LegacySessionBridge {
    Session* session = nullptr;
    Status (*finishSync)(Session&) = nullptr;
};

bool InstallLegacyDispatch(DispatchMode mode, FrameRecorder* recorder, std::string& error);
void RemoveLegacyDispatch();
// Rebind after qgl logging changes without resetting the current frame or capture failure.
void RefreshLegacyDispatchReferences();
Status LegacyDispatchStatus();
// Indexed studio arrays are outside the immediate-mode capture surface.
bool LegacyIndexedDrawingAllowed();
LegacyProcedure RouteLegacyExtension(const char* name, LegacyProcedure procedure);

}
}

void APIENTRY CseRenderAlphaFunc(GLenum function, GLclampf reference);
void APIENTRY CseRenderBegin(GLenum mode);
void APIENTRY CseRenderBlendFunc(GLenum source, GLenum destination);
void APIENTRY CseRenderClear(GLbitfield mask);
void APIENTRY CseRenderClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
void APIENTRY CseRenderColor3f(GLfloat red, GLfloat green, GLfloat blue);
void APIENTRY CseRenderColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void APIENTRY CseRenderColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha);
void APIENTRY CseRenderColor4ubv(const GLubyte* color);
void APIENTRY CseRenderCullFace(GLenum mode);
void APIENTRY CseRenderDepthFunc(GLenum function);
void APIENTRY CseRenderDepthMask(GLboolean enabled);
void APIENTRY CseRenderDepthRange(GLclampd nearDepth, GLclampd farDepth);
void APIENTRY CseRenderDisable(GLenum capability);
void APIENTRY CseRenderEnable(GLenum capability);
void APIENTRY CseRenderEnd();
void APIENTRY CseRenderFinish();
void APIENTRY CseRenderFogf(GLenum name, GLfloat value);
void APIENTRY CseRenderFogfv(GLenum name, const GLfloat* values);
void APIENTRY CseRenderFogi(GLenum name, GLint value);
void APIENTRY CseRenderFrustum(
    GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble nearDepth, GLdouble farDepth);
void APIENTRY CseRenderGetFloatv(GLenum name, GLfloat* values);
void APIENTRY CseRenderGetIntegerv(GLenum name, GLint* value);
const GLubyte* APIENTRY CseRenderGetString(GLenum name);
void APIENTRY CseRenderHint(GLenum target, GLenum mode);
GLboolean APIENTRY CseRenderIsEnabled(GLenum capability);
void APIENTRY CseRenderLoadIdentity();
void APIENTRY CseRenderMatrixMode(GLenum mode);
void APIENTRY CseRenderOrtho(
    GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble nearDepth, GLdouble farDepth);
void APIENTRY CseRenderPopMatrix();
void APIENTRY CseRenderPushMatrix();
void APIENTRY CseRenderRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void APIENTRY CseRenderScalef(GLfloat x, GLfloat y, GLfloat z);
void APIENTRY CseRenderShadeModel(GLenum mode);
void APIENTRY CseRenderTexCoord2f(GLfloat s, GLfloat t);
void APIENTRY CseRenderTexEnvf(GLenum target, GLenum name, GLfloat value);
void APIENTRY CseRenderTexEnvi(GLenum target, GLenum name, GLint value);
void APIENTRY CseRenderTexParameterf(GLenum target, GLenum name, GLfloat value);
void APIENTRY CseRenderTranslatef(GLfloat x, GLfloat y, GLfloat z);
void APIENTRY CseRenderVertex2f(GLfloat x, GLfloat y);
void APIENTRY CseRenderVertex3f(GLfloat x, GLfloat y, GLfloat z);
void APIENTRY CseRenderVertex3fv(const GLfloat* vertex);
void APIENTRY CseRenderViewport(GLint x, GLint y, GLsizei width, GLsizei height);
void APIENTRY CseRenderSelectTextureSGIS(GLenum texture);
void APIENTRY CseRenderMTexCoord2fSGIS(GLenum target, GLfloat s, GLfloat t);

// T2b private exact-typed texture/list/array surface.
void APIENTRY CseRenderBindTexture(GLenum target, GLuint texture);
void APIENTRY CseRenderDeleteTextures(GLsizei count, const GLuint* textures);
void APIENTRY CseRenderGenTextures(GLsizei count, GLuint* textures);
void APIENTRY CseRenderPixelStorei(GLenum name, GLint value);
void APIENTRY CseRenderTexImage2D(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height,
    GLint border, GLenum format, GLenum type, const GLvoid* pixels);
void APIENTRY CseRenderTexSubImage2D(GLenum target, GLint level, GLint x, GLint y, GLsizei width, GLsizei height,
    GLenum format, GLenum type, const GLvoid* pixels);
void APIENTRY CseRenderTexParameteri(GLenum target, GLenum name, GLint value);
void APIENTRY CseRenderColorTableEXT(GLenum target, GLenum internalFormat, GLsizei width, GLenum format,
    GLenum type, const void* table);
GLuint APIENTRY CseRenderGenLists(GLsizei range);
void APIENTRY CseRenderNewList(GLuint list, GLenum mode);
void APIENTRY CseRenderEndList();
void APIENTRY CseRenderCallList(GLuint list);
void APIENTRY CseRenderDeleteLists(GLuint list, GLsizei range);
void APIENTRY CseRenderEnableClientState(GLenum array);
void APIENTRY CseRenderTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer);
void APIENTRY CseRenderVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer);
void APIENTRY CseRenderDrawArrays(GLenum mode, GLint first, GLsizei count);
void APIENTRY CseRenderScissor(GLint x, GLint y, GLsizei width, GLsizei height);
void APIENTRY CseRenderPolygonOffset(GLfloat factor, GLfloat units);
