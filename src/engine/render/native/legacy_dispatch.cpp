#include "legacy_dispatch.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>

// qgl storage belongs to the engine (the CPU fixture supplies those same linkage slots).
extern decltype(&CseRenderAlphaFunc) qglAlphaFunc;
extern decltype(&CseRenderBegin) qglBegin;
extern decltype(&CseRenderBlendFunc) qglBlendFunc;
extern decltype(&CseRenderClear) qglClear;
extern decltype(&CseRenderClearColor) qglClearColor;
extern decltype(&CseRenderColor3f) qglColor3f;
extern decltype(&CseRenderColor4f) qglColor4f;
extern decltype(&CseRenderColor4ub) qglColor4ub;
extern decltype(&CseRenderColor4ubv) qglColor4ubv;
extern decltype(&CseRenderCullFace) qglCullFace;
extern decltype(&CseRenderDepthFunc) qglDepthFunc;
extern decltype(&CseRenderDepthMask) qglDepthMask;
extern decltype(&CseRenderDepthRange) qglDepthRange;
extern decltype(&CseRenderDisable) qglDisable;
extern decltype(&CseRenderEnable) qglEnable;
extern decltype(&CseRenderEnd) qglEnd;
extern decltype(&CseRenderFinish) qglFinish;
extern decltype(&CseRenderFogf) qglFogf;
extern decltype(&CseRenderFogfv) qglFogfv;
extern decltype(&CseRenderFogi) qglFogi;
extern decltype(&CseRenderFrustum) qglFrustum;
extern decltype(&CseRenderGetFloatv) qglGetFloatv;
extern decltype(&CseRenderGetIntegerv) qglGetIntegerv;
extern decltype(&CseRenderGetString) qglGetString;
extern decltype(&CseRenderHint) qglHint;
extern decltype(&CseRenderIsEnabled) qglIsEnabled;
extern decltype(&CseRenderLoadIdentity) qglLoadIdentity;
extern decltype(&CseRenderMatrixMode) qglMatrixMode;
extern decltype(&CseRenderOrtho) qglOrtho;
extern decltype(&CseRenderPopMatrix) qglPopMatrix;
extern decltype(&CseRenderPushMatrix) qglPushMatrix;
extern decltype(&CseRenderRotatef) qglRotatef;
extern decltype(&CseRenderScalef) qglScalef;
extern decltype(&CseRenderShadeModel) qglShadeModel;
extern decltype(&CseRenderTexCoord2f) qglTexCoord2f;
extern decltype(&CseRenderTexEnvf) qglTexEnvf;
extern decltype(&CseRenderTexEnvi) qglTexEnvi;
extern decltype(&CseRenderTexParameterf) qglTexParameterf;
extern decltype(&CseRenderTranslatef) qglTranslatef;
extern decltype(&CseRenderVertex2f) qglVertex2f;
extern decltype(&CseRenderVertex3f) qglVertex3f;
extern decltype(&CseRenderVertex3fv) qglVertex3fv;
extern decltype(&CseRenderViewport) qglViewport;
extern decltype(&CseRenderSelectTextureSGIS) qglSelectTextureSGIS;
extern decltype(&CseRenderMTexCoord2fSGIS) qglMTexCoord2fSGIS;

extern decltype(&CseRenderBindTexture) qglBindTexture;
extern decltype(&CseRenderDeleteTextures) qglDeleteTextures;
extern decltype(&CseRenderGenTextures) qglGenTextures;
extern decltype(&CseRenderPixelStorei) qglPixelStorei;
extern decltype(&CseRenderTexImage2D) qglTexImage2D;
extern decltype(&CseRenderTexSubImage2D) qglTexSubImage2D;
extern decltype(&CseRenderTexParameteri) qglTexParameteri;
extern decltype(&CseRenderColorTableEXT) qglColorTableEXT;
extern decltype(&CseRenderGenLists) qglGenLists;
extern decltype(&CseRenderNewList) qglNewList;
extern decltype(&CseRenderEndList) qglEndList;
extern decltype(&CseRenderCallList) qglCallList;
extern decltype(&CseRenderDeleteLists) qglDeleteLists;
extern decltype(&CseRenderEnableClientState) qglEnableClientState;
extern decltype(&CseRenderTexCoordPointer) qglTexCoordPointer;
extern decltype(&CseRenderVertexPointer) qglVertexPointer;
extern decltype(&CseRenderDrawArrays) qglDrawArrays;
extern decltype(&CseRenderScissor) qglScissor;
extern decltype(&CseRenderPolygonOffset) qglPolygonOffset;

extern decltype(&CseRenderReadPixels) qglReadPixels;
extern decltype(&CseRenderReadBuffer) qglReadBuffer;
extern decltype(&CseRenderDrawBuffer) qglDrawBuffer;
extern decltype(&CseRenderPolygonMode) qglPolygonMode;
extern decltype(&CseRenderFrontFace) qglFrontFace;
extern decltype(&CseRenderClearDepth) qglClearDepth;
extern decltype(&CseRenderClearStencil) qglClearStencil;
extern decltype(&CseRenderColorMask) qglColorMask;
extern decltype(&CseRenderLoadMatrixf) qglLoadMatrixf;
extern decltype(&CseRenderMultMatrixf) qglMultMatrixf;

namespace {
using cse::render::DispatchMode;
using cse::render::FrameRecorder;
using cse::render::Status;
using cse::render::Command;
using cse::render::ScalarCommand;
using cse::render::Op;
using cse::render::MatrixMode;
using cse::render::Capability;

constexpr std::uint32_t kInvalidArgument = std::numeric_limits<std::uint32_t>::max();
constexpr GLfloat kByteColorMaximum = 255.0f;
// SGIS uses a different enum range from ARB; the actual driver keeps its original enum.
constexpr GLenum kTexture0Sgis = 0x835E;
constexpr GLenum kSharedTexturePalette = 0x81fb;

struct ReferenceTable {
    decltype(&CseRenderReadPixels) ReadPixels = nullptr;
    decltype(&CseRenderReadBuffer) ReadBuffer = nullptr;
    decltype(&CseRenderDrawBuffer) DrawBuffer = nullptr;
    decltype(&CseRenderPolygonMode) PolygonMode = nullptr;
    decltype(&CseRenderFrontFace) FrontFace = nullptr;
    decltype(&CseRenderClearDepth) ClearDepth = nullptr;
    decltype(&CseRenderClearStencil) ClearStencil = nullptr;
    decltype(&CseRenderColorMask) ColorMask = nullptr;
    decltype(&CseRenderLoadMatrixf) LoadMatrixf = nullptr;
    decltype(&CseRenderMultMatrixf) MultMatrixf = nullptr;

    decltype(&CseRenderBindTexture) BindTexture = nullptr;
    decltype(&CseRenderDeleteTextures) DeleteTextures = nullptr;
    decltype(&CseRenderGenTextures) GenTextures = nullptr;
    decltype(&CseRenderPixelStorei) PixelStorei = nullptr;
    decltype(&CseRenderTexImage2D) TexImage2D = nullptr;
    decltype(&CseRenderTexSubImage2D) TexSubImage2D = nullptr;
    decltype(&CseRenderTexParameteri) TexParameteri = nullptr;
    decltype(&CseRenderColorTableEXT) ColorTableEXT = nullptr;
    decltype(&CseRenderGenLists) GenLists = nullptr;
    decltype(&CseRenderNewList) NewList = nullptr;
    decltype(&CseRenderEndList) EndList = nullptr;
    decltype(&CseRenderCallList) CallList = nullptr;
    decltype(&CseRenderDeleteLists) DeleteLists = nullptr;
    decltype(&CseRenderEnableClientState) EnableClientState = nullptr;
    decltype(&CseRenderTexCoordPointer) TexCoordPointer = nullptr;
    decltype(&CseRenderVertexPointer) VertexPointer = nullptr;
    decltype(&CseRenderDrawArrays) DrawArrays = nullptr;
    decltype(&CseRenderScissor) Scissor = nullptr;
    decltype(&CseRenderPolygonOffset) PolygonOffset = nullptr;

    decltype(&CseRenderAlphaFunc) AlphaFunc = nullptr;
    decltype(&CseRenderBegin) Begin = nullptr;
    decltype(&CseRenderBlendFunc) BlendFunc = nullptr;
    decltype(&CseRenderClear) Clear = nullptr;
    decltype(&CseRenderClearColor) ClearColor = nullptr;
    decltype(&CseRenderColor3f) Color3f = nullptr;
    decltype(&CseRenderColor4f) Color4f = nullptr;
    decltype(&CseRenderColor4ub) Color4ub = nullptr;
    decltype(&CseRenderColor4ubv) Color4ubv = nullptr;
    decltype(&CseRenderCullFace) CullFace = nullptr;
    decltype(&CseRenderDepthFunc) DepthFunc = nullptr;
    decltype(&CseRenderDepthMask) DepthMask = nullptr;
    decltype(&CseRenderDepthRange) DepthRange = nullptr;
    decltype(&CseRenderDisable) Disable = nullptr;
    decltype(&CseRenderEnable) Enable = nullptr;
    decltype(&CseRenderEnd) End = nullptr;
    decltype(&CseRenderFinish) Finish = nullptr;
    decltype(&CseRenderFogf) Fogf = nullptr;
    decltype(&CseRenderFogfv) Fogfv = nullptr;
    decltype(&CseRenderFogi) Fogi = nullptr;
    decltype(&CseRenderFrustum) Frustum = nullptr;
    decltype(&CseRenderGetFloatv) GetFloatv = nullptr;
    decltype(&CseRenderGetIntegerv) GetIntegerv = nullptr;
    decltype(&CseRenderGetString) GetString = nullptr;
    decltype(&CseRenderHint) Hint = nullptr;
    decltype(&CseRenderIsEnabled) IsEnabled = nullptr;
    decltype(&CseRenderLoadIdentity) LoadIdentity = nullptr;
    decltype(&CseRenderMatrixMode) MatrixMode = nullptr;
    decltype(&CseRenderOrtho) Ortho = nullptr;
    decltype(&CseRenderPopMatrix) PopMatrix = nullptr;
    decltype(&CseRenderPushMatrix) PushMatrix = nullptr;
    decltype(&CseRenderRotatef) Rotatef = nullptr;
    decltype(&CseRenderScalef) Scalef = nullptr;
    decltype(&CseRenderShadeModel) ShadeModel = nullptr;
    decltype(&CseRenderTexCoord2f) TexCoord2f = nullptr;
    decltype(&CseRenderTexEnvf) TexEnvf = nullptr;
    decltype(&CseRenderTexEnvi) TexEnvi = nullptr;
    decltype(&CseRenderTexParameterf) TexParameterf = nullptr;
    decltype(&CseRenderTranslatef) Translatef = nullptr;
    decltype(&CseRenderVertex2f) Vertex2f = nullptr;
    decltype(&CseRenderVertex3f) Vertex3f = nullptr;
    decltype(&CseRenderVertex3fv) Vertex3fv = nullptr;
    decltype(&CseRenderViewport) Viewport = nullptr;
    decltype(&CseRenderSelectTextureSGIS) SelectTextureSGIS = nullptr;
    decltype(&CseRenderMTexCoord2fSGIS) MTexCoord2fSGIS = nullptr;
};

ReferenceTable driverReferences;
FrameRecorder* recorder = nullptr;
DispatchMode currentDispatchMode = DispatchMode::ReferenceGL;
Status failure = Status::InvalidOperation;
bool installed = false;
bool referencesCaptured = false;
cse::render::LegacyNativeBridge nativeBridge;
cse::render::ClearEvent nativeClear;
cse::render::ReadbackStage readTarget = cse::render::ReadbackStage::SceneColor;
const char* operationContext = "Native dispatch";
const char* failedOperation = nullptr;
bool nativeDither = true;

bool Native() {
    return installed && currentDispatchMode == DispatchMode::Native;
}

GLenum selectTextureBase = kTexture0Sgis;
GLenum textureCoordinateBase = kTexture0Sgis;

void Refuse(Status reason = Status::InvalidOperation) {
    if (failure == Status::Ok) {
        failure = reason;
        failedOperation = operationContext;
    }
    if (recorder) {
        const auto recorded = recorder->Refuse(failure);
        if (recorded != Status::Ok) {
            failure = recorded;
        }
    }
}

bool EnsureNativeRecording() {
    if (!Native() || failure != Status::Ok) {
        return failure == Status::Ok;
    }
    if (!nativeBridge.ensureRecording(nativeBridge.user)) {
        Refuse();
        return false;
    }
    return true;
}

bool Capturing() {
    return installed && currentDispatchMode != DispatchMode::ReferenceGL;
}

bool CanForward(bool available) {
    if (currentDispatchMode == DispatchMode::CaptureOnly || Native()) {
        return false;
    }
    if (!installed || !available) {
        Refuse();
        return false;
    }
    // A failed shadow recording must never suppress an original driver call.
    return true;
}

void Capture(const Command& command) {
    if (!Capturing() || failure != Status::Ok) {
        return;
    }
    if (!EnsureNativeRecording()) {
        return;
    }
    const auto status = recorder->Submit(command);
    if (status != Status::Ok) {
        Refuse(status);
    }
}

void CaptureUnsupported() {
    if (Capturing()) {
        Refuse();
    }
}

void CaptureValues(Op operation, std::array<double, 6> values = {}, std::uint32_t argument = 0,
    std::uint32_t second = 0) {
    if (!Capturing() || failure != Status::Ok || !EnsureNativeRecording()) {
        return;
    }
    const ScalarCommand command{operation, argument, second, values};
    const auto status = recorder->SubmitScalar(command);
    if (status != Status::Ok) {
        Refuse(status);
    }
}

// Explicit lookup order matches the private semantic enums, never numeric GL ordinal assumptions.
template<std::size_t Count>
std::uint32_t Argument(GLenum value, const GLenum (&choices)[Count]) {
    for (std::size_t index = 0; index < Count; ++index) {
        if (value == choices[index]) {
            return static_cast<std::uint32_t>(index);
        }
    }
    return kInvalidArgument;
}

constexpr GLenum kMatrices[] = {GL_MODELVIEW, GL_PROJECTION, GL_TEXTURE};
constexpr GLenum kPrimitives[] = {GL_TRIANGLES, GL_TRIANGLE_FAN, GL_QUADS, GL_POLYGON, GL_LINES,
    GL_TRIANGLE_STRIP, GL_QUAD_STRIP, GL_LINE_LOOP};
constexpr GLenum kComparisons[] = {GL_NEVER, GL_LESS, GL_EQUAL, GL_LEQUAL, GL_GREATER, GL_NOTEQUAL,
    GL_GEQUAL, GL_ALWAYS};
constexpr GLenum kBlendFactors[] = {GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_SRC_ALPHA,
    GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR};
constexpr GLenum kCullModes[] = {GL_FRONT, GL_BACK, GL_FRONT_AND_BACK};
constexpr GLenum kShades[] = {GL_FLAT, GL_SMOOTH};
constexpr GLenum kCapabilities[] = {GL_BLEND, GL_DEPTH_TEST, GL_ALPHA_TEST, GL_CULL_FACE, GL_SCISSOR_TEST,
    GL_FOG, GL_TEXTURE_2D, GL_TEXTURE_RECTANGLE_ARB, GL_POLYGON_OFFSET_FILL};
constexpr GLenum kFogModes[] = {GL_EXP, GL_EXP2, GL_LINEAR};
constexpr GLenum kHints[] = {GL_DONT_CARE, GL_FASTEST, GL_NICEST};
constexpr GLenum kEnvironmentModes[] = {GL_MODULATE, GL_REPLACE, GL_COMBINE_ARB};

std::uint32_t TextureUnit(GLenum texture, GLenum base) {
    if (texture < base || texture - base >= cse::render::kTextureUnits) {
        return kInvalidArgument;
    }
    return texture - base;
}

// Avoid undefined float-to-enum conversion for NaN, infinities and out-of-range input.
GLenum EnumValue(double value) {
    if (!std::isfinite(value) || value < 0 || value > std::numeric_limits<GLenum>::max() ||
        value != std::floor(value)) {
        return std::numeric_limits<GLenum>::max();
    }
    return static_cast<GLenum>(value);
}

// Adapter-owned allocations are inside this boundary; Shadow must always reach its driver call.
template<typename Operation>
void CaptureOwned(Operation operation) {
    if (!Capturing() || failure != Status::Ok) {
        return;
    }
    if (!EnsureNativeRecording()) {
        return;
    }
    const auto status = recorder->CaptureStatus();
    if (status != Status::Ok) {
        Refuse(status);
        return;
    }
    try {
        operation();
    } catch (const std::bad_alloc&) {
        Refuse(Status::LimitExceeded);
    } catch (const std::length_error&) {
        Refuse(Status::LimitExceeded);
    }
}

constexpr GLenum kTextureTargets[] = {GL_TEXTURE_2D, GL_TEXTURE_RECTANGLE_ARB};
constexpr GLenum kPixelFormats[] = {GL_RGBA, GL_BGRA, GL_RGB, GL_LUMINANCE, GL_ALPHA,
    GL_LUMINANCE_ALPHA, GL_COLOR_INDEX};
constexpr GLenum kInternalFormats[] = {GL_RGBA8, GL_RGB8, GL_RGB5_A1, GL_RGB5, GL_RGBA4, GL_LUMINANCE8,
    GL_ALPHA8, GL_INTENSITY8, GL_COLOR_INDEX8_EXT, GL_RGBA, GL_RGB, GL_LUMINANCE, GL_ALPHA, GL_INTENSITY};
constexpr GLenum kTextureParameters[] = {GL_TEXTURE_MIN_FILTER, GL_TEXTURE_MAG_FILTER, GL_TEXTURE_WRAP_S,
    GL_TEXTURE_WRAP_T, GL_TEXTURE_MAX_LEVEL, GL_TEXTURE_MAX_ANISOTROPY_EXT};
constexpr GLenum kFilters[] = {GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_NEAREST,
    GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR};
constexpr GLenum kWraps[] = {GL_REPEAT, GL_CLAMP, GL_CLAMP_TO_EDGE};
constexpr GLenum kPixelStores[] = {GL_UNPACK_ALIGNMENT, GL_UNPACK_ROW_LENGTH, GL_PACK_ALIGNMENT};
constexpr GLint kSourceArrayComponents = 2;
constexpr GLsizei kSourceArrayStride = 4 * sizeof(GLfloat);
constexpr std::size_t kSourceAttributeBytes = kSourceArrayComponents * sizeof(GLfloat);

struct ClientArray {
    const GLvoid* pointer = nullptr;
    bool defined = false;
    bool enabled = false;
};

// Borrowed live descriptors only. They never enter Command or FrameSnapshot.
ClientArray vertexArray;
ClientArray textureCoordinateArray;

bool PointerExtent(const void* pointer, std::size_t bytes) {
    if (!bytes) {
        return true;
    }
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    return pointer && bytes <= std::numeric_limits<std::uintptr_t>::max() - address;
}

void CaptureTextureParameter(GLenum target, GLenum name, double value) {
    CaptureOwned([&] {
        const auto mappedTarget = Argument(target, kTextureTargets);
        const auto parameter = Argument(name, kTextureParameters);
        if (mappedTarget == kInvalidArgument || parameter == kInvalidArgument) {
            Refuse(Status::InvalidValue);
            return;
        }
        using cse::render::TextureParameter;
        if (parameter == static_cast<std::uint32_t>(TextureParameter::MinFilter) ||
            parameter == static_cast<std::uint32_t>(TextureParameter::MagFilter)) {
            const auto filter = Argument(EnumValue(value), kFilters);
            if (filter == kInvalidArgument) {
                Refuse(Status::InvalidValue);
                return;
            }
            value = filter;
        } else if (parameter == static_cast<std::uint32_t>(TextureParameter::WrapS) ||
            parameter == static_cast<std::uint32_t>(TextureParameter::WrapT)) {
            const auto wrap = Argument(EnumValue(value), kWraps);
            if (wrap == kInvalidArgument) {
                Refuse(Status::InvalidValue);
                return;
            }
            value = wrap;
        }
        CaptureValues(Op::TextureParameter, {{value}}, mappedTarget, parameter);
    });
}

void CaptureUpload(GLenum target, GLint level, GLint internalFormat, GLint x, GLint y,
    GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid* pixels, bool subimage) {
    CaptureOwned([&] {
        const auto mappedTarget = Argument(target, kTextureTargets);
        const auto mappedFormat = Argument(format, kPixelFormats);
        if (recorder->InPrimitive()) {
            Refuse(Status::InvalidOperation);
            return;
        }
        if (mappedTarget == kInvalidArgument || mappedFormat == kInvalidArgument || level < 0 ||
            level >= cse::render::kTextureLevels || x < 0 || y < 0 || border < 0) {
            Refuse(Status::InvalidValue);
            return;
        }
        if (type != GL_UNSIGNED_BYTE || border != 0) {
            Refuse(Status::InvalidOperation);
            return;
        }
        if (target == GL_TEXTURE_RECTANGLE_ARB && level != 0) {
            Refuse(Status::InvalidValue);
            return;
        }
        auto mappedInternal = Argument(static_cast<GLenum>(internalFormat), kInternalFormats);
        if (!subimage && mappedInternal == kInvalidArgument) {
            // Legacy numeric internal formats used by GL 1.x callers; two components have no CPU internal enum.
            constexpr GLint kLegacyRgbComponents = 3;
            constexpr GLint kLegacyRgbaComponents = 4;
            if (internalFormat == kLegacyRgbComponents) {
                mappedInternal = static_cast<std::uint32_t>(cse::render::InternalFormat::RGB);
            } else if (internalFormat == kLegacyRgbaComponents) {
                mappedInternal = static_cast<std::uint32_t>(cse::render::InternalFormat::RGBA);
            } else {
                Refuse(Status::InvalidOperation);
                return;
            }
        }
        Command command;
        command.op = subimage ? Op::TextureSubImage : Op::TextureImage;
        command.argument = mappedTarget;
        auto& upload = command.upload;
        upload.level = level;
        upload.x = x;
        upload.y = y;
        upload.width = width;
        upload.height = height;
        upload.format = static_cast<cse::render::PixelFormat>(mappedFormat);
        upload.internal = subimage ? cse::render::InternalFormat::RGBA8 :
            static_cast<cse::render::InternalFormat>(mappedInternal);
        upload.allocateOnly = !subimage && !pixels;
        if (!subimage && ((upload.internal == cse::render::InternalFormat::ColorIndex8) !=
            (upload.format == cse::render::PixelFormat::ColorIndex))) {
            Refuse(Status::InvalidOperation);
            return;
        }
        std::size_t extent = 0;
        const auto status = cse::render::TextureStore::UploadByteExtent(upload, recorder->Textures().Unpack(), extent);
        if (status != Status::Ok) {
            Refuse(status);
            return;
        }
        if (!upload.allocateOnly && extent) {
            if (!PointerExtent(pixels, extent)) {
                Refuse(Status::InvalidValue);
                return;
            }
            upload.bytes.resize(extent);
            std::memcpy(upload.bytes.data(), pixels, extent);
        }
        Capture(command);
    });
}

void CaptureArrayPointer(ClientArray& array, GLint size, GLenum type, GLsizei stride, const GLvoid* pointer) {
    CaptureOwned([&] {
        if (recorder->InPrimitive()) {
            Refuse(Status::InvalidOperation);
        } else if (size < 0 || stride < 0) {
            Refuse(Status::InvalidValue);
        } else if (size != kSourceArrayComponents || type != GL_FLOAT || stride != kSourceArrayStride) {
            Refuse(Status::InvalidOperation);
        } else {
            array.pointer = pointer;
            array.defined = true;
        }
    });
}

void CaptureArrays(GLenum mode, GLint first, GLsizei count) {
    CaptureOwned([&] {
        if (first < 0 || count < 0) {
            Refuse(Status::InvalidValue);
            return;
        }
        if (mode != GL_QUADS || first != 0 || recorder->InPrimitive() ||
            !vertexArray.enabled || !textureCoordinateArray.enabled ||
            !vertexArray.defined || !textureCoordinateArray.defined) {
            Refuse(Status::InvalidOperation);
            return;
        }
        if (!count) {
            return;
        }
        const auto vertices = static_cast<std::size_t>(count);
        const auto& limits = recorder->Limits().capture;
        if (vertices > cse::render::kMaximumVertices || vertices > limits.verticesPerDraw ||
            vertices > limits.frameVertices) {
            Refuse(Status::LimitExceeded);
            return;
        }
        if (vertices - 1 > (std::numeric_limits<std::size_t>::max() - kSourceAttributeBytes) /
            static_cast<std::size_t>(kSourceArrayStride)) {
            Refuse(Status::InvalidValue);
            return;
        }
        const auto extent = (vertices - 1) * static_cast<std::size_t>(kSourceArrayStride) + kSourceAttributeBytes;
        if (!PointerExtent(vertexArray.pointer, extent) || !PointerExtent(textureCoordinateArray.pointer, extent)) {
            Refuse(Status::InvalidValue);
            return;
        }
        const auto uvAddress = reinterpret_cast<std::uintptr_t>(textureCoordinateArray.pointer);
        const auto positionAddress = reinterpret_cast<std::uintptr_t>(vertexArray.pointer);
        if (uvAddress > std::numeric_limits<std::uintptr_t>::max() - kSourceAttributeBytes ||
            positionAddress != uvAddress + kSourceAttributeBytes) {
            Refuse(Status::InvalidOperation);
            return;
        }
        Command command;
        command.op = Op::DrawArrays;
        command.argument = static_cast<std::uint32_t>(cse::render::Primitive::Quads);
        command.vertices.resize(vertices);
        for (std::size_t index = 0; index < vertices; ++index) {
            const auto offset = index * static_cast<std::size_t>(kSourceArrayStride);
            std::array<GLfloat, kSourceArrayComponents> position{};
            std::array<GLfloat, kSourceArrayComponents> uv{};
            std::memcpy(position.data(), reinterpret_cast<const void*>(positionAddress + offset), sizeof(position));
            std::memcpy(uv.data(), reinterpret_cast<const void*>(uvAddress + offset), sizeof(uv));
            auto& vertex = command.vertices[index];
            vertex.position = {{position[0], position[1], 0, 1}};
            vertex.texcoords[0] = {{uv[0], uv[1], 0, 1}};
        }
        Capture(command);
    });
}

void CaptureFog(GLenum name, double value) {
    if (!Capturing()) {
        return;
    }
    switch (name) {
    case GL_FOG_MODE:
        CaptureValues(Op::FogMode, {}, Argument(EnumValue(value), kFogModes));
        break;
    case GL_FOG_DENSITY:
        CaptureValues(Op::FogDensity, {{value}});
        break;
    case GL_FOG_START:
        CaptureValues(Op::FogStart, {{value}});
        break;
    case GL_FOG_END:
        CaptureValues(Op::FogEnd, {{value}});
        break;
    default:
        Refuse(Status::InvalidValue);
        break;
    }
}

void CaptureEnvironment(GLenum target, GLenum name, double value) {
    if (!Capturing()) {
        return;
    }
    using cse::render::EnvironmentParameter;
    if (target != GL_TEXTURE_ENV) {
        Refuse(Status::InvalidValue);
        return;
    }
    const auto enumValue = EnumValue(value);
    switch (name) {
    case GL_TEXTURE_ENV_MODE:
        CaptureValues(Op::TextureEnvironment, {}, static_cast<std::uint32_t>(EnvironmentParameter::Mode),
            Argument(enumValue, kEnvironmentModes));
        break;
    case GL_COMBINE_RGB_ARB:
        CaptureValues(Op::TextureEnvironment, {}, static_cast<std::uint32_t>(EnvironmentParameter::CombineRGB),
            enumValue == GL_MODULATE ? 0 : kInvalidArgument);
        break;
    case GL_RGB_SCALE_ARB:
        CaptureValues(Op::TextureEnvironment, {}, static_cast<std::uint32_t>(EnvironmentParameter::RGBScale),
            enumValue);
        break;
    default:
        Refuse(Status::InvalidValue);
        break;
    }
}

bool CanQuery() {
    if (!EnsureNativeRecording()) {
        return false;
    }
    if (!installed || !recorder || failure != Status::Ok) {
        Refuse();
        return false;
    }
    const auto status = recorder->CaptureStatus();
    if (status != Status::Ok) {
        Refuse(status);
        return false;
    }
    if (recorder->InPrimitive()) {
        Refuse(Status::InvalidOperation);
        return false;
    }
    return true;
}

bool CanMutateNativeState() {
    if (!CanQuery()) {
        return false;
    }
    // These bridge-owned values are intentionally outside display-list compilation.
    if (!recorder->AtFrameBoundary()) {
        Refuse();
        return false;
    }
    return true;
}

void QueryFloats(GLenum name, GLfloat* values) {
    if (!CanQuery()) {
        return;
    }
    if (!values) {
        Refuse(Status::InvalidValue);
        return;
    }
    const auto& state = recorder->State();
    switch (name) {
    case GL_MODELVIEW_MATRIX:
    case GL_PROJECTION_MATRIX:
    case GL_TEXTURE_MATRIX: {
        const auto matrixMode = name == GL_MODELVIEW_MATRIX ? MatrixMode::ModelView :
            name == GL_PROJECTION_MATRIX ? MatrixMode::Projection : MatrixMode::Texture;
        const auto& matrix = state.Matrix(matrixMode, state.ActiveTexture()).values;
        std::copy(matrix.begin(), matrix.end(), values);
        break;
    }
    case GL_CURRENT_COLOR:
        std::copy(state.Current().color.begin(), state.Current().color.end(), values);
        break;
    case GL_CURRENT_TEXTURE_COORDS: {
        const auto& coordinates = state.Current().texcoords[state.ActiveTexture()];
        std::copy(coordinates.begin(), coordinates.end(), values);
        break;
    }
    case GL_FOG_COLOR:
        std::copy(state.raster.fogColor.begin(), state.raster.fogColor.end(), values);
        break;
    case GL_FOG_DENSITY:
        *values = state.raster.fogDensity;
        break;
    case GL_FOG_START:
        *values = state.raster.fogStart;
        break;
    case GL_FOG_END:
        *values = state.raster.fogEnd;
        break;
    default:
        Refuse();
        break;
    }
}

GLboolean QueryEnabled(GLenum capability) {
    if (!CanQuery()) {
        return GL_FALSE;
    }
    if (Native() && capability == GL_DITHER) {
        return nativeDither ? GL_TRUE : GL_FALSE;
    }
    if (Native() && capability == kSharedTexturePalette) {
        return recorder->State().sharedTexturePalette ? GL_TRUE : GL_FALSE;
    }
    const auto argument = Argument(capability, kCapabilities);
    if (argument == kInvalidArgument) {
        Refuse(Status::InvalidValue);
        return GL_FALSE;
    }
    const auto& state = recorder->State();
    bool enabled = false;
    if (argument == static_cast<std::uint32_t>(Capability::Texture2D)) {
        enabled = state.textures[state.ActiveTexture()].enabled2D;
    } else if (argument == static_cast<std::uint32_t>(Capability::TextureRectangle)) {
        enabled = state.textures[state.ActiveTexture()].enabledRectangle;
    } else {
        const auto index = argument == static_cast<std::uint32_t>(Capability::PolygonOffsetFill) ?
            state.raster.enabled.size() - 1 : static_cast<std::size_t>(argument);
        enabled = state.raster.enabled[index];
    }
    return enabled ? GL_TRUE : GL_FALSE;
}

template<typename Procedure>
void OwnSlot(Procedure& slot, Procedure wrapper, Procedure& saved) {
    if (slot != wrapper && (Native() || !referencesCaptured || saved)) {
        saved = slot;
    }
    // GL modes preserve loader absence; Native advertises only this owned CPU surface.
    slot = (Native() || saved) ? wrapper : nullptr;
}
}

namespace cse {
namespace render {

void RefreshLegacyDispatchReferences() {
    if (!installed) {
        return;
    }
    OwnSlot(qglReadPixels, &CseRenderReadPixels, driverReferences.ReadPixels);
    OwnSlot(qglReadBuffer, &CseRenderReadBuffer, driverReferences.ReadBuffer);
    OwnSlot(qglDrawBuffer, &CseRenderDrawBuffer, driverReferences.DrawBuffer);
    OwnSlot(qglPolygonMode, &CseRenderPolygonMode, driverReferences.PolygonMode);
    OwnSlot(qglFrontFace, &CseRenderFrontFace, driverReferences.FrontFace);
    OwnSlot(qglClearDepth, &CseRenderClearDepth, driverReferences.ClearDepth);
    OwnSlot(qglClearStencil, &CseRenderClearStencil, driverReferences.ClearStencil);
    OwnSlot(qglColorMask, &CseRenderColorMask, driverReferences.ColorMask);
    OwnSlot(qglLoadMatrixf, &CseRenderLoadMatrixf, driverReferences.LoadMatrixf);
    OwnSlot(qglMultMatrixf, &CseRenderMultMatrixf, driverReferences.MultMatrixf);
    OwnSlot(qglAlphaFunc, &CseRenderAlphaFunc, driverReferences.AlphaFunc);
    OwnSlot(qglBegin, &CseRenderBegin, driverReferences.Begin);
    OwnSlot(qglBlendFunc, &CseRenderBlendFunc, driverReferences.BlendFunc);
    OwnSlot(qglClear, &CseRenderClear, driverReferences.Clear);
    OwnSlot(qglClearColor, &CseRenderClearColor, driverReferences.ClearColor);
    OwnSlot(qglColor3f, &CseRenderColor3f, driverReferences.Color3f);
    OwnSlot(qglColor4f, &CseRenderColor4f, driverReferences.Color4f);
    OwnSlot(qglColor4ub, &CseRenderColor4ub, driverReferences.Color4ub);
    OwnSlot(qglColor4ubv, &CseRenderColor4ubv, driverReferences.Color4ubv);
    OwnSlot(qglCullFace, &CseRenderCullFace, driverReferences.CullFace);
    OwnSlot(qglDepthFunc, &CseRenderDepthFunc, driverReferences.DepthFunc);
    OwnSlot(qglDepthMask, &CseRenderDepthMask, driverReferences.DepthMask);
    OwnSlot(qglDepthRange, &CseRenderDepthRange, driverReferences.DepthRange);
    OwnSlot(qglDisable, &CseRenderDisable, driverReferences.Disable);
    OwnSlot(qglEnable, &CseRenderEnable, driverReferences.Enable);
    OwnSlot(qglEnd, &CseRenderEnd, driverReferences.End);
    OwnSlot(qglFinish, &CseRenderFinish, driverReferences.Finish);
    OwnSlot(qglFogf, &CseRenderFogf, driverReferences.Fogf);
    OwnSlot(qglFogfv, &CseRenderFogfv, driverReferences.Fogfv);
    OwnSlot(qglFogi, &CseRenderFogi, driverReferences.Fogi);
    OwnSlot(qglFrustum, &CseRenderFrustum, driverReferences.Frustum);
    OwnSlot(qglGetFloatv, &CseRenderGetFloatv, driverReferences.GetFloatv);
    OwnSlot(qglGetIntegerv, &CseRenderGetIntegerv, driverReferences.GetIntegerv);
    OwnSlot(qglGetString, &CseRenderGetString, driverReferences.GetString);
    OwnSlot(qglHint, &CseRenderHint, driverReferences.Hint);
    OwnSlot(qglIsEnabled, &CseRenderIsEnabled, driverReferences.IsEnabled);
    OwnSlot(qglLoadIdentity, &CseRenderLoadIdentity, driverReferences.LoadIdentity);
    OwnSlot(qglMatrixMode, &CseRenderMatrixMode, driverReferences.MatrixMode);
    OwnSlot(qglOrtho, &CseRenderOrtho, driverReferences.Ortho);
    OwnSlot(qglPopMatrix, &CseRenderPopMatrix, driverReferences.PopMatrix);
    OwnSlot(qglPushMatrix, &CseRenderPushMatrix, driverReferences.PushMatrix);
    OwnSlot(qglRotatef, &CseRenderRotatef, driverReferences.Rotatef);
    OwnSlot(qglScalef, &CseRenderScalef, driverReferences.Scalef);
    OwnSlot(qglShadeModel, &CseRenderShadeModel, driverReferences.ShadeModel);
    OwnSlot(qglTexCoord2f, &CseRenderTexCoord2f, driverReferences.TexCoord2f);
    OwnSlot(qglTexEnvf, &CseRenderTexEnvf, driverReferences.TexEnvf);
    OwnSlot(qglTexEnvi, &CseRenderTexEnvi, driverReferences.TexEnvi);
    OwnSlot(qglTexParameterf, &CseRenderTexParameterf, driverReferences.TexParameterf);
    OwnSlot(qglTranslatef, &CseRenderTranslatef, driverReferences.Translatef);
    OwnSlot(qglVertex2f, &CseRenderVertex2f, driverReferences.Vertex2f);
    OwnSlot(qglVertex3f, &CseRenderVertex3f, driverReferences.Vertex3f);
    OwnSlot(qglVertex3fv, &CseRenderVertex3fv, driverReferences.Vertex3fv);
    OwnSlot(qglViewport, &CseRenderViewport, driverReferences.Viewport);
    OwnSlot(qglSelectTextureSGIS, &CseRenderSelectTextureSGIS, driverReferences.SelectTextureSGIS);
    OwnSlot(qglMTexCoord2fSGIS, &CseRenderMTexCoord2fSGIS, driverReferences.MTexCoord2fSGIS);
    OwnSlot(qglBindTexture, &CseRenderBindTexture, driverReferences.BindTexture);
    OwnSlot(qglDeleteTextures, &CseRenderDeleteTextures, driverReferences.DeleteTextures);
    OwnSlot(qglGenTextures, &CseRenderGenTextures, driverReferences.GenTextures);
    OwnSlot(qglPixelStorei, &CseRenderPixelStorei, driverReferences.PixelStorei);
    OwnSlot(qglTexImage2D, &CseRenderTexImage2D, driverReferences.TexImage2D);
    OwnSlot(qglTexSubImage2D, &CseRenderTexSubImage2D, driverReferences.TexSubImage2D);
    OwnSlot(qglTexParameteri, &CseRenderTexParameteri, driverReferences.TexParameteri);
    OwnSlot(qglColorTableEXT, &CseRenderColorTableEXT, driverReferences.ColorTableEXT);
    OwnSlot(qglGenLists, &CseRenderGenLists, driverReferences.GenLists);
    OwnSlot(qglNewList, &CseRenderNewList, driverReferences.NewList);
    OwnSlot(qglEndList, &CseRenderEndList, driverReferences.EndList);
    OwnSlot(qglCallList, &CseRenderCallList, driverReferences.CallList);
    OwnSlot(qglDeleteLists, &CseRenderDeleteLists, driverReferences.DeleteLists);
    OwnSlot(qglEnableClientState, &CseRenderEnableClientState, driverReferences.EnableClientState);
    OwnSlot(qglTexCoordPointer, &CseRenderTexCoordPointer, driverReferences.TexCoordPointer);
    OwnSlot(qglVertexPointer, &CseRenderVertexPointer, driverReferences.VertexPointer);
    OwnSlot(qglDrawArrays, &CseRenderDrawArrays, driverReferences.DrawArrays);
    OwnSlot(qglScissor, &CseRenderScissor, driverReferences.Scissor);
    OwnSlot(qglPolygonOffset, &CseRenderPolygonOffset, driverReferences.PolygonOffset);
    referencesCaptured = true;
}

bool InstallLegacyDispatch(DispatchMode selectedMode, FrameRecorder* selectedRecorder, std::string& error) {
    if (selectedMode != DispatchMode::ReferenceGL && selectedMode != DispatchMode::ShadowCapture &&
        selectedMode != DispatchMode::CaptureOnly) {
        error = "Unknown legacy dispatch mode";
        return false;
    }
    if (selectedMode != DispatchMode::ReferenceGL && !selectedRecorder) {
        error = "Capture requires an owned frame recorder";
        return false;
    }
    currentDispatchMode = selectedMode;
    recorder = selectedRecorder;
    failure = Status::Ok;
    failedOperation = nullptr;
    installed = true;
    RefreshLegacyDispatchReferences();
    error.clear();
    return true;
}

void RemoveLegacyDispatch() {
    if (installed) {
        qglReadPixels = driverReferences.ReadPixels;
        qglReadBuffer = driverReferences.ReadBuffer;
        qglDrawBuffer = driverReferences.DrawBuffer;
        qglPolygonMode = driverReferences.PolygonMode;
        qglFrontFace = driverReferences.FrontFace;
        qglClearDepth = driverReferences.ClearDepth;
        qglClearStencil = driverReferences.ClearStencil;
        qglColorMask = driverReferences.ColorMask;
        qglLoadMatrixf = driverReferences.LoadMatrixf;
        qglMultMatrixf = driverReferences.MultMatrixf;
        qglBindTexture = driverReferences.BindTexture;
        qglDeleteTextures = driverReferences.DeleteTextures;
        qglGenTextures = driverReferences.GenTextures;
        qglPixelStorei = driverReferences.PixelStorei;
        qglTexImage2D = driverReferences.TexImage2D;
        qglTexSubImage2D = driverReferences.TexSubImage2D;
        qglTexParameteri = driverReferences.TexParameteri;
        qglColorTableEXT = driverReferences.ColorTableEXT;
        qglGenLists = driverReferences.GenLists;
        qglNewList = driverReferences.NewList;
        qglEndList = driverReferences.EndList;
        qglCallList = driverReferences.CallList;
        qglDeleteLists = driverReferences.DeleteLists;
        qglEnableClientState = driverReferences.EnableClientState;
        qglTexCoordPointer = driverReferences.TexCoordPointer;
        qglVertexPointer = driverReferences.VertexPointer;
        qglDrawArrays = driverReferences.DrawArrays;
        qglScissor = driverReferences.Scissor;
        qglPolygonOffset = driverReferences.PolygonOffset;
        qglAlphaFunc = driverReferences.AlphaFunc;
        qglBegin = driverReferences.Begin;
        qglBlendFunc = driverReferences.BlendFunc;
        qglClear = driverReferences.Clear;
        qglClearColor = driverReferences.ClearColor;
        qglColor3f = driverReferences.Color3f;
        qglColor4f = driverReferences.Color4f;
        qglColor4ub = driverReferences.Color4ub;
        qglColor4ubv = driverReferences.Color4ubv;
        qglCullFace = driverReferences.CullFace;
        qglDepthFunc = driverReferences.DepthFunc;
        qglDepthMask = driverReferences.DepthMask;
        qglDepthRange = driverReferences.DepthRange;
        qglDisable = driverReferences.Disable;
        qglEnable = driverReferences.Enable;
        qglEnd = driverReferences.End;
        qglFinish = driverReferences.Finish;
        qglFogf = driverReferences.Fogf;
        qglFogfv = driverReferences.Fogfv;
        qglFogi = driverReferences.Fogi;
        qglFrustum = driverReferences.Frustum;
        qglGetFloatv = driverReferences.GetFloatv;
        qglGetIntegerv = driverReferences.GetIntegerv;
        qglGetString = driverReferences.GetString;
        qglHint = driverReferences.Hint;
        qglIsEnabled = driverReferences.IsEnabled;
        qglLoadIdentity = driverReferences.LoadIdentity;
        qglMatrixMode = driverReferences.MatrixMode;
        qglOrtho = driverReferences.Ortho;
        qglPopMatrix = driverReferences.PopMatrix;
        qglPushMatrix = driverReferences.PushMatrix;
        qglRotatef = driverReferences.Rotatef;
        qglScalef = driverReferences.Scalef;
        qglShadeModel = driverReferences.ShadeModel;
        qglTexCoord2f = driverReferences.TexCoord2f;
        qglTexEnvf = driverReferences.TexEnvf;
        qglTexEnvi = driverReferences.TexEnvi;
        qglTexParameterf = driverReferences.TexParameterf;
        qglTranslatef = driverReferences.Translatef;
        qglVertex2f = driverReferences.Vertex2f;
        qglVertex3f = driverReferences.Vertex3f;
        qglVertex3fv = driverReferences.Vertex3fv;
        qglViewport = driverReferences.Viewport;
        qglSelectTextureSGIS = driverReferences.SelectTextureSGIS;
        qglMTexCoord2fSGIS = driverReferences.MTexCoord2fSGIS;
    }
    nativeBridge = LegacyNativeBridge{};
    nativeClear = ClearEvent{};
    readTarget = ReadbackStage::SceneColor;
    nativeDither = true;
    driverReferences = ReferenceTable{};
    recorder = nullptr;
    currentDispatchMode = DispatchMode::ReferenceGL;
    failure = Status::InvalidOperation;
    installed = false;
    referencesCaptured = false;
    vertexArray = ClientArray{};
    textureCoordinateArray = ClientArray{};
    selectTextureBase = kTexture0Sgis;
    textureCoordinateBase = kTexture0Sgis;
}

bool InstallNativeDispatch(FrameRecorder* selectedRecorder, const LegacyNativeBridge& bridge, std::string& error) {
    if (!selectedRecorder || !bridge.ensureRecording || !bridge.finish || !bridge.readPixels ||
        !bridge.textureUnits || bridge.textureUnits > kTextureUnits || !bridge.maximumTextureSize ||
        bridge.maximumTextureSize > static_cast<unsigned>(kTextureDimension)) {
        error = "Native dispatch requires a recorder, synchronous bridge, and bounded native capabilities";
        return false;
    }
    if (installed) {
        RemoveLegacyDispatch();
    }
    currentDispatchMode = DispatchMode::Native;
    recorder = selectedRecorder;
    nativeBridge = bridge;
    failure = Status::Ok;
    failedOperation = nullptr;
    installed = true;
    RefreshLegacyDispatchReferences();
    error.clear();
    return true;
}

ReadbackStage LegacyNativeReadTarget() {
    return readTarget;
}

ReadbackStage LegacyNativeDrawTarget() {
    // FRONT drawing requires explicit owner routing, never an ignored framebuffer selector.
    return ReadbackStage::SceneColor;
}

std::string LegacyDispatchDiagnostic() {
    return failedOperation ? std::string(failedOperation) + ": legacy capture operation failed (status " +
        std::to_string(static_cast<unsigned>(failure)) + ")" : std::string{};
}

Status SubmitNativeIndexed(const std::vector<Vertex>& vertices, const std::vector<std::uint32_t>& indices,
    std::size_t lastVertex, bool perVertexColor) {
    operationContext = "SubmitNativeIndexed";
    if (!Native()) {
        return Status::InvalidOperation;
    }
    CaptureOwned([&] {
        if (vertices.empty()) {
            if (!indices.empty()) {
                Refuse(Status::InvalidValue);
            }
            return;
        }
        const auto& limits = recorder->Limits().capture;
        constexpr std::size_t kVerticesPerTriangle = 3;
        if (recorder->InPrimitive() || lastVertex >= vertices.size() || indices.size() % kVerticesPerTriangle != 0) {
            Refuse(Status::InvalidOperation);
            return;
        }
        if (indices.size() > limits.verticesPerDraw || vertices.size() > limits.verticesPerDraw) {
            Refuse(Status::LimitExceeded);
            return;
        }
        if (recorder->AtFrameBoundary()) {
            const auto status = recorder->SubmitIndexed(vertices, indices, perVertexColor);
            if (status != Status::Ok) {
                Refuse(status);
                return;
            }
        } else if (!indices.empty()) {
            for (const auto index : indices) {
                if (index >= vertices.size()) {
                    Refuse(Status::InvalidValue);
                    return;
                }
            }
            // Lists own complete payloads until playback. Immediate capture copies
            // directly into its frame packet.
            Command command;
            command.op = Op::DrawNative;
            command.argument = static_cast<std::uint32_t>(Primitive::Triangles);
            command.second = perVertexColor ? 1 : 0;
            command.vertices = vertices;
            command.indices = indices;
            Capture(command);
        }
        const auto& last = vertices[lastVertex];
        CaptureValues(Op::TexCoord, {{last.texcoords[0][0], last.texcoords[0][1]}}, 0);
        if (perVertexColor) {
            CaptureValues(Op::Color, {{last.color[0], last.color[1], last.color[2], last.color[3]}});
        }
    });
    return failure;
}

Status SubmitNativeQuad(const std::array<Vertex, 4>& vertices) {
    operationContext = "SubmitNativeQuad";
    if (!Native()) {
        return Status::InvalidOperation;
    }
    CaptureOwned([&] {
        const auto result = recorder->SubmitQuad(vertices);
        if (result != Status::Ok) {
            Refuse(result);
        }
    });
    return failure;
}

Status SubmitNativeSurface(const std::shared_ptr<const SurfaceMesh>& mesh, float scroll, bool detail) {
    operationContext = "SubmitNativeSurface";
    if (!Native()) {
        return Status::InvalidOperation;
    }
    CaptureOwned([&] {
        const auto result = recorder->SubmitSurface(mesh, scroll, detail);
        if (result != Status::Ok) {
            Refuse(result);
        }
    });
    return failure;
}

Status LegacyDispatchStatus() {
    return failure;
}

bool LegacyIndexedDrawingAllowed() {
    return !Capturing();
}

LegacyProcedure RouteLegacyExtension(const char* name, LegacyProcedure procedure) {
    if (!name || !installed) {
        return nullptr;
    }
    if (Native()) {
        // Native aliases route semantic entry points without replacing the saved GL ownership snapshot.
        if (std::strcmp(name, "glBindTextureEXT") == 0) {
            return reinterpret_cast<LegacyProcedure>(&CseRenderBindTexture);
        }
        if (std::strcmp(name, "glColorTableEXT") == 0) {
            return reinterpret_cast<LegacyProcedure>(&CseRenderColorTableEXT);
        }
        if (std::strcmp(name, "glActiveTextureARB") == 0 || std::strcmp(name, "glSelectTextureSGIS") == 0) {
            selectTextureBase = std::strcmp(name, "glActiveTextureARB") == 0 ? GL_TEXTURE0_ARB : kTexture0Sgis;
            return reinterpret_cast<LegacyProcedure>(&CseRenderSelectTextureSGIS);
        }
        if (std::strcmp(name, "glMultiTexCoord2fARB") == 0 || std::strcmp(name, "glMTexCoord2fSGIS") == 0) {
            textureCoordinateBase = std::strcmp(name, "glMultiTexCoord2fARB") == 0 ? GL_TEXTURE0_ARB : kTexture0Sgis;
            return reinterpret_cast<LegacyProcedure>(&CseRenderMTexCoord2fSGIS);
        }
        return nullptr;
    }
    if (std::strcmp(name, "glBindTextureEXT") == 0) {
        const auto wrapper = reinterpret_cast<LegacyProcedure>(&CseRenderBindTexture);
        if (procedure != wrapper) {
            driverReferences.BindTexture = reinterpret_cast<decltype(qglBindTexture)>(procedure);
        }
        qglBindTexture = driverReferences.BindTexture ? &CseRenderBindTexture : nullptr;
        return reinterpret_cast<LegacyProcedure>(qglBindTexture);
    }
    if (std::strcmp(name, "glColorTableEXT") == 0) {
        const auto wrapper = reinterpret_cast<LegacyProcedure>(&CseRenderColorTableEXT);
        if (procedure != wrapper) {
            driverReferences.ColorTableEXT = reinterpret_cast<decltype(qglColorTableEXT)>(procedure);
        }
        qglColorTableEXT = driverReferences.ColorTableEXT ? &CseRenderColorTableEXT : nullptr;
        return reinterpret_cast<LegacyProcedure>(qglColorTableEXT);
    }
    if (std::strcmp(name, "glActiveTextureARB") == 0 || std::strcmp(name, "glSelectTextureSGIS") == 0) {
        const auto wrapper = reinterpret_cast<LegacyProcedure>(&CseRenderSelectTextureSGIS);
        if (procedure != wrapper) {
            driverReferences.SelectTextureSGIS = reinterpret_cast<decltype(qglSelectTextureSGIS)>(procedure);
        }
        selectTextureBase = std::strcmp(name, "glActiveTextureARB") == 0 ? GL_TEXTURE0_ARB : kTexture0Sgis;
        qglSelectTextureSGIS = driverReferences.SelectTextureSGIS ? &CseRenderSelectTextureSGIS : nullptr;
        return reinterpret_cast<LegacyProcedure>(qglSelectTextureSGIS);
    }
    if (std::strcmp(name, "glMultiTexCoord2fARB") == 0 || std::strcmp(name, "glMTexCoord2fSGIS") == 0) {
        const auto wrapper = reinterpret_cast<LegacyProcedure>(&CseRenderMTexCoord2fSGIS);
        if (procedure != wrapper) {
            driverReferences.MTexCoord2fSGIS = reinterpret_cast<decltype(qglMTexCoord2fSGIS)>(procedure);
        }
        textureCoordinateBase = std::strcmp(name, "glMultiTexCoord2fARB") == 0 ? GL_TEXTURE0_ARB : kTexture0Sgis;
        qglMTexCoord2fSGIS = driverReferences.MTexCoord2fSGIS ? &CseRenderMTexCoord2fSGIS : nullptr;
        return reinterpret_cast<LegacyProcedure>(qglMTexCoord2fSGIS);
    }
    return nullptr;
}

}
}

void APIENTRY CseRenderAlphaFunc(GLenum function, GLclampf reference) {
    operationContext = __func__;
    CaptureValues(Op::AlphaFunction, {{reference}}, Argument(function, kComparisons));
    if (CanForward(driverReferences.AlphaFunc != nullptr)) {
        driverReferences.AlphaFunc(function, reference);
    }
}

void APIENTRY CseRenderBegin(GLenum mode) {
    operationContext = __func__;
    CaptureValues(Op::Begin, {}, Argument(mode, kPrimitives));
    if (CanForward(driverReferences.Begin != nullptr)) {
        driverReferences.Begin(mode);
    }
}

void APIENTRY CseRenderBlendFunc(GLenum source, GLenum destination) {
    operationContext = __func__;
    CaptureValues(Op::BlendFunction, {}, Argument(source, kBlendFactors), Argument(destination, kBlendFactors));
    if (CanForward(driverReferences.BlendFunc != nullptr)) {
        driverReferences.BlendFunc(source, destination);
    }
}

void APIENTRY CseRenderClear(GLbitfield mask) {
    operationContext = __func__;
    if (Native()) {
        CaptureOwned([&] {
            constexpr GLbitfield kSupportedMask = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
            if ((mask & ~kSupportedMask) != 0) {
                Refuse(Status::InvalidValue);
                return;
            }
            if (!CanMutateNativeState() || !mask) {
                return;
            }
            auto clear = nativeClear;
            clear.mask = ((mask & GL_COLOR_BUFFER_BIT) ? cse::render::kClearColor : 0) |
                ((mask & GL_DEPTH_BUFFER_BIT) ? cse::render::kClearDepth : 0) |
                ((mask & GL_STENCIL_BUFFER_BIT) ? cse::render::kClearStencil : 0);
            const auto& raster = recorder->State().raster;
            clear.scissor = raster.scissor;
            clear.scissorEnabled = raster.enabled[static_cast<std::size_t>(Capability::ScissorTest)];
            clear.colorWrite = raster.colorWrite;
            clear.depthWrite = raster.depthWrite;
            const auto status = recorder->Clear(clear);
            if (status != Status::Ok) {
                Refuse(status);
            }
        });
        return;
    }
    CaptureUnsupported();
    if (CanForward(driverReferences.Clear != nullptr)) {
        driverReferences.Clear(mask);
    }
}

void APIENTRY CseRenderClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
    operationContext = __func__;
    if (Native()) {
        if (CanMutateNativeState()) {
            const std::array<float, 4> color{{red, green, blue, alpha}};
            for (const auto component : color) {
                if (!std::isfinite(component)) {
                    Refuse(Status::InvalidValue);
                    return;
                }
            }
            for (std::size_t channel = 0; channel < color.size(); ++channel) {
                nativeClear.color[channel] = std::max(0.0f, std::min(color[channel], 1.0f));
            }
        }
        return;
    }
    CaptureUnsupported();
    if (CanForward(driverReferences.ClearColor != nullptr)) {
        driverReferences.ClearColor(red, green, blue, alpha);
    }
}

void APIENTRY CseRenderColor3f(GLfloat red, GLfloat green, GLfloat blue) {
    operationContext = __func__;
    CaptureValues(Op::Color, {{red, green, blue, 1}});
    if (CanForward(driverReferences.Color3f != nullptr)) {
        driverReferences.Color3f(red, green, blue);
    }
}

void APIENTRY CseRenderColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    operationContext = __func__;
    CaptureValues(Op::Color, {{red, green, blue, alpha}});
    if (CanForward(driverReferences.Color4f != nullptr)) {
        driverReferences.Color4f(red, green, blue, alpha);
    }
}

void APIENTRY CseRenderColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha) {
    operationContext = __func__;
    CaptureValues(Op::Color, {{red / kByteColorMaximum, green / kByteColorMaximum,
        blue / kByteColorMaximum, alpha / kByteColorMaximum}});
    if (CanForward(driverReferences.Color4ub != nullptr)) {
        driverReferences.Color4ub(red, green, blue, alpha);
    }
}

void APIENTRY CseRenderColor4ubv(const GLubyte* color) {
    operationContext = __func__;
    if (Capturing()) {
        if (color) {
            CaptureValues(Op::Color, {{color[0] / kByteColorMaximum, color[1] / kByteColorMaximum,
                color[2] / kByteColorMaximum, color[3] / kByteColorMaximum}});
        } else {
            Refuse(Status::InvalidValue);
        }
    }
    if (CanForward(driverReferences.Color4ubv != nullptr)) {
        driverReferences.Color4ubv(color);
    }
}

void APIENTRY CseRenderCullFace(GLenum mode) {
    operationContext = __func__;
    CaptureValues(Op::CullFace, {}, Argument(mode, kCullModes));
    if (CanForward(driverReferences.CullFace != nullptr)) {
        driverReferences.CullFace(mode);
    }
}

void APIENTRY CseRenderDepthFunc(GLenum function) {
    operationContext = __func__;
    CaptureValues(Op::DepthFunction, {}, Argument(function, kComparisons));
    if (CanForward(driverReferences.DepthFunc != nullptr)) {
        driverReferences.DepthFunc(function);
    }
}

void APIENTRY CseRenderDepthMask(GLboolean enabled) {
    operationContext = __func__;
    CaptureValues(Op::DepthWrite, {{enabled != GL_FALSE ? 1.0 : 0.0}});
    if (CanForward(driverReferences.DepthMask != nullptr)) {
        driverReferences.DepthMask(enabled);
    }
}

void APIENTRY CseRenderDepthRange(GLclampd nearDepth, GLclampd farDepth) {
    operationContext = __func__;
    CaptureValues(Op::DepthRange, {{nearDepth, farDepth}});
    if (CanForward(driverReferences.DepthRange != nullptr)) {
        driverReferences.DepthRange(nearDepth, farDepth);
    }
}

void APIENTRY CseRenderDisable(GLenum capability) {
    operationContext = __func__;
    if (Native() && (capability == GL_DITHER || capability == GL_LIGHTING || capability == GL_COLOR_MATERIAL ||
        capability == GL_NORMALIZE || capability == kSharedTexturePalette)) {
        if (capability == GL_DITHER) {
            if (CanMutateNativeState()) {
                nativeDither = false;
            }
        } else if (capability == kSharedTexturePalette) {
            CaptureValues(Op::Enable, {{0}}, static_cast<std::uint32_t>(Capability::SharedTexturePalette));
        } else {
            CanMutateNativeState();
        }
        return;
    }
    CaptureValues(Op::Enable, {{0}}, Argument(capability, kCapabilities));
    if (CanForward(driverReferences.Disable != nullptr)) {
        driverReferences.Disable(capability);
    }
}

void APIENTRY CseRenderEnable(GLenum capability) {
    operationContext = __func__;
    if (Native() && (capability == GL_DITHER || capability == kSharedTexturePalette)) {
        if (capability == GL_DITHER) {
            if (CanMutateNativeState()) {
                nativeDither = true;
            }
        } else {
            CaptureValues(Op::Enable, {{1}}, static_cast<std::uint32_t>(Capability::SharedTexturePalette));
        }
        return;
    }
    CaptureValues(Op::Enable, {{1}}, Argument(capability, kCapabilities));
    if (CanForward(driverReferences.Enable != nullptr)) {
        driverReferences.Enable(capability);
    }
}

void APIENTRY CseRenderEnd() {
    operationContext = __func__;
    CaptureValues(Op::End);
    if (CanForward(driverReferences.End != nullptr)) {
        driverReferences.End();
    }
}

void APIENTRY CseRenderFinish() {
    operationContext = __func__;
    if (Native()) {
        if (CanQuery() && !nativeBridge.finish(nativeBridge.user)) {
            Refuse();
        }
        return;
    }
    // T3 supplies the synchronous Session barrier; never pretend that Seal is a Finish.
    if (currentDispatchMode == DispatchMode::CaptureOnly) {
        CaptureUnsupported();
    }
    if (CanForward(driverReferences.Finish != nullptr)) {
        driverReferences.Finish();
    }
}

void APIENTRY CseRenderFogf(GLenum name, GLfloat value) {
    operationContext = __func__;
    CaptureFog(name, value);
    if (CanForward(driverReferences.Fogf != nullptr)) {
        driverReferences.Fogf(name, value);
    }
}

void APIENTRY CseRenderFogfv(GLenum name, const GLfloat* values) {
    operationContext = __func__;
    if (Capturing()) {
        if (!values) {
            Refuse(Status::InvalidValue);
        } else if (name == GL_FOG_COLOR) {
            CaptureValues(Op::FogColor, {{values[0], values[1], values[2], values[3]}});
        } else {
            CaptureFog(name, values[0]);
        }
    }
    if (CanForward(driverReferences.Fogfv != nullptr)) {
        driverReferences.Fogfv(name, values);
    }
}

void APIENTRY CseRenderFogi(GLenum name, GLint value) {
    operationContext = __func__;
    CaptureFog(name, value);
    if (CanForward(driverReferences.Fogi != nullptr)) {
        driverReferences.Fogi(name, value);
    }
}

void APIENTRY CseRenderFrustum(
    GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble nearDepth, GLdouble farDepth) {
    operationContext = __func__;
    CaptureValues(Op::Frustum, {{left, right, bottom, top, nearDepth, farDepth}});
    if (CanForward(driverReferences.Frustum != nullptr)) {
        driverReferences.Frustum(left, right, bottom, top, nearDepth, farDepth);
    }
}

void APIENTRY CseRenderGetFloatv(GLenum name, GLfloat* values) {
    operationContext = __func__;
    if (currentDispatchMode == DispatchMode::CaptureOnly || Native()) {
        QueryFloats(name, values);
        return;
    }
    if (CanForward(driverReferences.GetFloatv != nullptr)) {
        driverReferences.GetFloatv(name, values);
    }
}

void APIENTRY CseRenderGetIntegerv(GLenum name, GLint* value) {
    operationContext = __func__;
    if (Native()) {
        if (!CanQuery()) {
            return;
        }
        if (!value) {
            Refuse(Status::InvalidValue);
            return;
        }
        const auto& state = recorder->State();
        const auto& pixelStore = recorder->Textures().Unpack();
        switch (name) {
        case GL_UNPACK_ALIGNMENT:
            *value = pixelStore.unpackAlignment;
            break;
        case GL_UNPACK_ROW_LENGTH:
            *value = pixelStore.unpackRowLength;
            break;
        case GL_PACK_ALIGNMENT:
            *value = pixelStore.packAlignment;
            break;
        case GL_TEXTURE_BINDING_2D:
            *value = static_cast<GLint>(state.textures[state.ActiveTexture()].texture2D);
            break;
        case GL_MAX_TEXTURE_UNITS_ARB:
            *value = static_cast<GLint>(nativeBridge.textureUnits);
            break;
        case GL_MAX_TEXTURE_SIZE:
            *value = static_cast<GLint>(nativeBridge.maximumTextureSize);
            break;
        case GL_RED_BITS:
        case GL_GREEN_BITS:
        case GL_BLUE_BITS:
        case GL_ALPHA_BITS:
            *value = static_cast<GLint>(nativeBridge.colorBits / nativeClear.color.size());
            break;
        case GL_DEPTH_BITS:
            *value = static_cast<GLint>(nativeBridge.depthBits);
            break;
        case GL_STENCIL_BITS:
            *value = static_cast<GLint>(nativeBridge.stencilBits);
            break;
        case GL_READ_BUFFER:
            *value = readTarget == cse::render::ReadbackStage::SceneColor ? GL_BACK : GL_FRONT;
            break;
        case GL_DRAW_BUFFER:
            *value = GL_BACK;
            break;
        case GL_MATRIX_MODE:
            *value = static_cast<GLint>(kMatrices[static_cast<std::size_t>(state.Mode())]);
            break;
        case GL_VIEWPORT:
            std::copy(state.raster.viewport.begin(), state.raster.viewport.end(), value);
            break;
        default:
            Refuse();
            break;
        }
        return;
    }
    // Backend limits/configuration are not inferred from the small semantic state owner.
    if (currentDispatchMode == DispatchMode::CaptureOnly) {
        CaptureUnsupported();
        return;
    }
    if (CanForward(driverReferences.GetIntegerv != nullptr)) {
        driverReferences.GetIntegerv(name, value);
    }
}

const GLubyte* APIENTRY CseRenderGetString(GLenum name) {
    operationContext = __func__;
    if (Native()) {
        Refuse();
        return nullptr;
    }
    // No fabricated version or extension string may advertise a general GL implementation.
    if (currentDispatchMode == DispatchMode::CaptureOnly) {
        CaptureUnsupported();
        return nullptr;
    }
    if (!CanForward(driverReferences.GetString != nullptr)) {
        return nullptr;
    }
    return driverReferences.GetString(name);
}

void APIENTRY CseRenderHint(GLenum target, GLenum mode) {
    operationContext = __func__;
    if (Capturing()) {
        if (target == GL_FOG_HINT || target == GL_PERSPECTIVE_CORRECTION_HINT) {
            CaptureValues(target == GL_FOG_HINT ? Op::FogHint : Op::PerspectiveHint, {}, Argument(mode, kHints));
        } else {
            Refuse(Status::InvalidValue);
        }
    }
    if (CanForward(driverReferences.Hint != nullptr)) {
        driverReferences.Hint(target, mode);
    }
}

GLboolean APIENTRY CseRenderIsEnabled(GLenum capability) {
    operationContext = __func__;
    if (currentDispatchMode == DispatchMode::CaptureOnly || Native()) {
        return QueryEnabled(capability);
    }
    if (!CanForward(driverReferences.IsEnabled != nullptr)) {
        return GL_FALSE;
    }
    return driverReferences.IsEnabled(capability);
}

void APIENTRY CseRenderLoadIdentity() {
    operationContext = __func__;
    CaptureValues(Op::Identity);
    if (CanForward(driverReferences.LoadIdentity != nullptr)) {
        driverReferences.LoadIdentity();
    }
}

void APIENTRY CseRenderMatrixMode(GLenum mode) {
    operationContext = __func__;
    CaptureValues(Op::MatrixMode, {}, Argument(mode, kMatrices));
    if (CanForward(driverReferences.MatrixMode != nullptr)) {
        driverReferences.MatrixMode(mode);
    }
}

void APIENTRY CseRenderOrtho(
    GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble nearDepth, GLdouble farDepth) {
    operationContext = __func__;
    CaptureValues(Op::Ortho, {{left, right, bottom, top, nearDepth, farDepth}});
    if (CanForward(driverReferences.Ortho != nullptr)) {
        driverReferences.Ortho(left, right, bottom, top, nearDepth, farDepth);
    }
}

void APIENTRY CseRenderPopMatrix() {
    operationContext = __func__;
    CaptureValues(Op::Pop);
    if (CanForward(driverReferences.PopMatrix != nullptr)) {
        driverReferences.PopMatrix();
    }
}

void APIENTRY CseRenderPushMatrix() {
    operationContext = __func__;
    CaptureValues(Op::Push);
    if (CanForward(driverReferences.PushMatrix != nullptr)) {
        driverReferences.PushMatrix();
    }
}

void APIENTRY CseRenderRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    operationContext = __func__;
    CaptureValues(Op::Rotate, {{angle, x, y, z}});
    if (CanForward(driverReferences.Rotatef != nullptr)) {
        driverReferences.Rotatef(angle, x, y, z);
    }
}

void APIENTRY CseRenderScalef(GLfloat x, GLfloat y, GLfloat z) {
    operationContext = __func__;
    CaptureValues(Op::Scale, {{x, y, z}});
    if (CanForward(driverReferences.Scalef != nullptr)) {
        driverReferences.Scalef(x, y, z);
    }
}

void APIENTRY CseRenderShadeModel(GLenum mode) {
    operationContext = __func__;
    CaptureValues(Op::ShadeModel, {}, Argument(mode, kShades));
    if (CanForward(driverReferences.ShadeModel != nullptr)) {
        driverReferences.ShadeModel(mode);
    }
}

void APIENTRY CseRenderTexCoord2f(GLfloat s, GLfloat t) {
    operationContext = __func__;
    CaptureValues(Op::TexCoord, {{s, t}}, 0);
    if (CanForward(driverReferences.TexCoord2f != nullptr)) {
        driverReferences.TexCoord2f(s, t);
    }
}

void APIENTRY CseRenderTexEnvf(GLenum target, GLenum name, GLfloat value) {
    operationContext = __func__;
    CaptureEnvironment(target, name, value);
    if (CanForward(driverReferences.TexEnvf != nullptr)) {
        driverReferences.TexEnvf(target, name, value);
    }
}

void APIENTRY CseRenderTexEnvi(GLenum target, GLenum name, GLint value) {
    operationContext = __func__;
    CaptureEnvironment(target, name, value);
    if (CanForward(driverReferences.TexEnvi != nullptr)) {
        driverReferences.TexEnvi(target, name, value);
    }
}

void APIENTRY CseRenderTexParameterf(GLenum target, GLenum name, GLfloat value) {
    operationContext = __func__;
    CaptureTextureParameter(target, name, value);
    if (CanForward(driverReferences.TexParameterf != nullptr)) {
        driverReferences.TexParameterf(target, name, value);
    }
}

void APIENTRY CseRenderTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    operationContext = __func__;
    CaptureValues(Op::Translate, {{x, y, z}});
    if (CanForward(driverReferences.Translatef != nullptr)) {
        driverReferences.Translatef(x, y, z);
    }
}

void APIENTRY CseRenderVertex2f(GLfloat x, GLfloat y) {
    operationContext = __func__;
    CaptureValues(Op::Vertex, {{x, y, 0}});
    if (CanForward(driverReferences.Vertex2f != nullptr)) {
        driverReferences.Vertex2f(x, y);
    }
}

void APIENTRY CseRenderVertex3f(GLfloat x, GLfloat y, GLfloat z) {
    operationContext = __func__;
    CaptureValues(Op::Vertex, {{x, y, z}});
    if (CanForward(driverReferences.Vertex3f != nullptr)) {
        driverReferences.Vertex3f(x, y, z);
    }
}

void APIENTRY CseRenderVertex3fv(const GLfloat* vertex) {
    operationContext = __func__;
    if (Capturing()) {
        if (vertex) {
            CaptureValues(Op::Vertex, {{vertex[0], vertex[1], vertex[2]}});
        } else {
            Refuse(Status::InvalidValue);
        }
    }
    if (CanForward(driverReferences.Vertex3fv != nullptr)) {
        driverReferences.Vertex3fv(vertex);
    }
}

void APIENTRY CseRenderViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    operationContext = __func__;
    CaptureValues(Op::Viewport, {{static_cast<double>(x), static_cast<double>(y),
        static_cast<double>(width), static_cast<double>(height)}});
    if (CanForward(driverReferences.Viewport != nullptr)) {
        driverReferences.Viewport(x, y, width, height);
    }
}

void APIENTRY CseRenderSelectTextureSGIS(GLenum texture) {
    operationContext = __func__;
    CaptureValues(Op::ActiveTexture, {}, TextureUnit(texture, selectTextureBase));
    if (CanForward(driverReferences.SelectTextureSGIS != nullptr)) {
        driverReferences.SelectTextureSGIS(texture);
    }
}

void APIENTRY CseRenderMTexCoord2fSGIS(GLenum target, GLfloat s, GLfloat t) {
    operationContext = __func__;
    CaptureValues(Op::TexCoord, {{s, t}}, TextureUnit(target, textureCoordinateBase));
    if (CanForward(driverReferences.MTexCoord2fSGIS != nullptr)) {
        driverReferences.MTexCoord2fSGIS(target, s, t);
    }
}

// T2b capture owns command data; original driver calls remain outside the allocation/refusal boundary.

void APIENTRY CseRenderBindTexture(GLenum target, GLuint texture) {
    operationContext = __func__;
    CaptureOwned([&] {
        CaptureValues(Op::BindTexture, {}, Argument(target, kTextureTargets), texture);
    });
    if (CanForward(driverReferences.BindTexture != nullptr)) {
        driverReferences.BindTexture(target, texture);
    }
}

void APIENTRY CseRenderDeleteTextures(GLsizei count, const GLuint* textures) {
    operationContext = __func__;
    CaptureOwned([&] {
        if (recorder->InPrimitive()) {
            Refuse(Status::InvalidOperation);
            return;
        }
        if (count < 0) {
            Refuse(Status::InvalidValue);
            return;
        }
        const auto names = static_cast<std::size_t>(count);
        if (names > cse::render::kTextureNames) {
            Refuse(Status::LimitExceeded);
            return;
        }
        if (!PointerExtent(textures, names * sizeof(GLuint))) {
            Refuse(Status::InvalidValue);
            return;
        }
        Command command;
        command.op = Op::DeleteTextures;
        if (names) {
            command.names.assign(textures, textures + names);
        }
        Capture(command);
    });
    if (CanForward(driverReferences.DeleteTextures != nullptr)) {
        driverReferences.DeleteTextures(count, textures);
    }
}

void APIENTRY CseRenderGenTextures(GLsizei count, GLuint* textures) {
    operationContext = __func__;
    if (Native()) {
        CaptureOwned([&] {
            if (count < 0) {
                Refuse(Status::InvalidValue);
                return;
            }
            if (static_cast<std::size_t>(count) > recorder->Limits().capture.textureNames) {
                Refuse(Status::LimitExceeded);
                return;
            }
            if (!PointerExtent(textures, static_cast<std::size_t>(count) * sizeof(GLuint))) {
                Refuse(Status::InvalidValue);
                return;
            }
            std::vector<std::uint32_t> names;
            const auto status = recorder->GenerateTextures(static_cast<std::size_t>(count), names);
            if (status != Status::Ok) {
                Refuse(status);
                return;
            }
            std::copy(names.begin(), names.end(), textures);
        });
        return;
    }
    CaptureUnsupported();
    if (CanForward(driverReferences.GenTextures != nullptr)) {
        driverReferences.GenTextures(count, textures);
    }
}

void APIENTRY CseRenderPixelStorei(GLenum name, GLint value) {
    operationContext = __func__;
    CaptureOwned([&] {
        CaptureValues(Op::PixelStore, {{static_cast<double>(value)}}, Argument(name, kPixelStores));
    });
    if (CanForward(driverReferences.PixelStorei != nullptr)) {
        driverReferences.PixelStorei(name, value);
    }
}

void APIENTRY CseRenderTexImage2D(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height,
    GLint border, GLenum format, GLenum type, const GLvoid* pixels) {
    operationContext = __func__;
    CaptureUpload(target, level, internalFormat, 0, 0, width, height, border, format, type, pixels, false);
    if (CanForward(driverReferences.TexImage2D != nullptr)) {
        driverReferences.TexImage2D(target, level, internalFormat, width, height, border, format, type, pixels);
    }
}

void APIENTRY CseRenderTexSubImage2D(GLenum target, GLint level, GLint x, GLint y, GLsizei width, GLsizei height,
    GLenum format, GLenum type, const GLvoid* pixels) {
    operationContext = __func__;
    CaptureUpload(target, level, GL_RGBA8, x, y, width, height, 0, format, type, pixels, true);
    if (CanForward(driverReferences.TexSubImage2D != nullptr)) {
        driverReferences.TexSubImage2D(target, level, x, y, width, height, format, type, pixels);
    }
}

void APIENTRY CseRenderTexParameteri(GLenum target, GLenum name, GLint value) {
    operationContext = __func__;
    CaptureTextureParameter(target, name, static_cast<double>(value));
    if (CanForward(driverReferences.TexParameteri != nullptr)) {
        driverReferences.TexParameteri(target, name, value);
    }
}

void APIENTRY CseRenderColorTableEXT(GLenum target, GLenum internalFormat, GLsizei width, GLenum format,
    GLenum type, const void* table) {
    operationContext = __func__;
    CaptureOwned([&] {
        if (recorder->InPrimitive()) {
            Refuse(Status::InvalidOperation);
            return;
        }
        if (target != kSharedTexturePalette || internalFormat != GL_RGB || format != GL_RGB ||
            type != GL_UNSIGNED_BYTE || width != static_cast<GLsizei>(cse::render::kPaletteEntries)) {
            Refuse(Status::InvalidOperation);
            return;
        }
        if (!PointerExtent(table, cse::render::kPaletteBytes)) {
            Refuse(Status::InvalidValue);
            return;
        }
        Command command;
        command.op = Op::SharedPalette;
        command.upload.bytes.resize(cse::render::kPaletteBytes);
        std::memcpy(command.upload.bytes.data(), table, cse::render::kPaletteBytes);
        Capture(command);
    });
    if (CanForward(driverReferences.ColorTableEXT != nullptr)) {
        driverReferences.ColorTableEXT(target, internalFormat, width, format, type, table);
    }
}

GLuint APIENTRY CseRenderGenLists(GLsizei range) {
    operationContext = __func__;
    if (Native()) {
        std::uint32_t first = 0;
        CaptureOwned([&] {
            if (range < 0) {
                Refuse(Status::InvalidValue);
                return;
            }
            const auto status = recorder->GenerateLists(static_cast<std::size_t>(range), first);
            if (status != Status::Ok) {
                Refuse(status);
            }
        });
        return first;
    }
    CaptureUnsupported();
    if (CanForward(driverReferences.GenLists != nullptr)) {
        return driverReferences.GenLists(range);
    }
    return 0;
}

void APIENTRY CseRenderNewList(GLuint list, GLenum mode) {
    operationContext = __func__;
    CaptureOwned([&] {
        if (mode != GL_COMPILE && mode != GL_COMPILE_AND_EXECUTE) {
            Refuse(Status::InvalidValue);
            return;
        }
        const auto status = recorder->BeginList(list, mode == GL_COMPILE_AND_EXECUTE);
        if (status != Status::Ok) {
            Refuse(status);
        }
    });
    if (CanForward(driverReferences.NewList != nullptr)) {
        driverReferences.NewList(list, mode);
    }
}

void APIENTRY CseRenderEndList() {
    operationContext = __func__;
    CaptureOwned([&] {
        const auto status = recorder->EndList();
        if (status != Status::Ok) {
            Refuse(status);
        }
    });
    if (CanForward(driverReferences.EndList != nullptr)) {
        driverReferences.EndList();
    }
}

void APIENTRY CseRenderCallList(GLuint list) {
    operationContext = __func__;
    CaptureOwned([&] {
        if (!list) {
            Refuse(Status::InvalidValue);
            return;
        }
        CaptureValues(Op::CallList, {}, list);
    });
    if (CanForward(driverReferences.CallList != nullptr)) {
        driverReferences.CallList(list);
    }
}

void APIENTRY CseRenderDeleteLists(GLuint list, GLsizei range) {
    operationContext = __func__;
    CaptureOwned([&] {
        if (recorder->InPrimitive()) {
            Refuse(Status::InvalidOperation);
            return;
        }
        if (range < 0) {
            Refuse(Status::InvalidValue);
        } else if (Native()) {
            const auto count = static_cast<std::uint32_t>(range);
            if (count > recorder->Limits().capture.lists ||
                (count && count - 1 > std::numeric_limits<std::uint32_t>::max() - list)) {
                Refuse(Status::LimitExceeded);
                return;
            }
            for (std::uint32_t offset = 0; offset < count; ++offset) {
                const auto status = recorder->DeleteList(list + offset);
                if (status != Status::Ok) {
                    Refuse(status);
                    return;
                }
            }
        } else if (range > 1) {
            // R3: only source range one is captured; no unchecked namespace iteration.
            Refuse(Status::InvalidOperation);
        } else if (range == 1) {
            const auto status = recorder->DeleteList(list);
            if (status != Status::Ok) {
                Refuse(status);
            }
        }
    });
    if (CanForward(driverReferences.DeleteLists != nullptr)) {
        driverReferences.DeleteLists(list, range);
    }
}

void APIENTRY CseRenderEnableClientState(GLenum array) {
    operationContext = __func__;
    CaptureOwned([&] {
        if (recorder->InPrimitive()) {
            Refuse(Status::InvalidOperation);
        } else if (array == GL_VERTEX_ARRAY) {
            vertexArray.enabled = true;
        } else if (array == GL_TEXTURE_COORD_ARRAY) {
            textureCoordinateArray.enabled = true;
        } else {
            Refuse(Status::InvalidOperation);
        }
    });
    if (CanForward(driverReferences.EnableClientState != nullptr)) {
        driverReferences.EnableClientState(array);
    }
}

void APIENTRY CseRenderTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer) {
    operationContext = __func__;
    CaptureArrayPointer(textureCoordinateArray, size, type, stride, pointer);
    if (CanForward(driverReferences.TexCoordPointer != nullptr)) {
        driverReferences.TexCoordPointer(size, type, stride, pointer);
    }
}

void APIENTRY CseRenderVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer) {
    operationContext = __func__;
    CaptureArrayPointer(vertexArray, size, type, stride, pointer);
    if (CanForward(driverReferences.VertexPointer != nullptr)) {
        driverReferences.VertexPointer(size, type, stride, pointer);
    }
}

void APIENTRY CseRenderDrawArrays(GLenum mode, GLint first, GLsizei count) {
    operationContext = __func__;
    CaptureArrays(mode, first, count);
    if (CanForward(driverReferences.DrawArrays != nullptr)) {
        driverReferences.DrawArrays(mode, first, count);
    }
}

void APIENTRY CseRenderScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    operationContext = __func__;
    CaptureOwned([&] {
        CaptureValues(Op::Scissor, {{static_cast<double>(x), static_cast<double>(y),
            static_cast<double>(width), static_cast<double>(height)}});
    });
    if (CanForward(driverReferences.Scissor != nullptr)) {
        driverReferences.Scissor(x, y, width, height);
    }
}

void APIENTRY CseRenderPolygonOffset(GLfloat factor, GLfloat units) {
    operationContext = __func__;
    CaptureOwned([&] {
        CaptureValues(Op::PolygonOffset, {{factor, units}});
    });
    if (CanForward(driverReferences.PolygonOffset != nullptr)) {
        driverReferences.PolygonOffset(factor, units);
    }
}

void APIENTRY CseRenderReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
    GLenum type, GLvoid* pixels) {
    operationContext = __func__;
    if (Native()) {
        if (!CanQuery()) {
            return;
        }
        if (x < 0 || y < 0 || width < 0 || height < 0 ||
            (format != GL_RGB && format != GL_RGBA) || type != GL_UNSIGNED_BYTE) {
            Refuse(Status::InvalidValue);
            return;
        }
        if (!width || !height) {
            return;
        }
        constexpr std::size_t kRgbComponents = 3;
        constexpr std::size_t kRgbaComponents = 4;
        const auto components = format == GL_RGB ? kRgbComponents : kRgbaComponents;
        const auto alignment = static_cast<std::size_t>(recorder->Textures().Unpack().packAlignment);
        if (static_cast<std::size_t>(width) >
            (std::numeric_limits<std::size_t>::max() - (alignment - 1)) / components) {
            Refuse(Status::InvalidValue);
            return;
        }
        const auto rowBytes = static_cast<std::size_t>(width) * components;
        const auto stride = (rowBytes + alignment - 1) / alignment * alignment;
        const auto rowsBeforeLast = static_cast<std::size_t>(height - 1);
        if (rowsBeforeLast > (std::numeric_limits<std::size_t>::max() - rowBytes) / stride ||
            !PointerExtent(pixels, rowsBeforeLast * stride + rowBytes)) {
            Refuse(Status::InvalidValue);
            return;
        }
        if (!nativeBridge.readPixels(nativeBridge.user, x, y, width, height, static_cast<unsigned>(components),
            static_cast<unsigned>(alignment), pixels)) {
            Refuse();
        }
        return;
    }
    CaptureUnsupported();
    if (CanForward(driverReferences.ReadPixels != nullptr)) {
        driverReferences.ReadPixels(x, y, width, height, format, type, pixels);
    }
}

void APIENTRY CseRenderReadBuffer(GLenum buffer) {
    operationContext = __func__;
    if (Native()) {
        if (CanMutateNativeState()) {
            if (buffer == GL_BACK || buffer == GL_FRONT) {
                readTarget = buffer == GL_BACK ? cse::render::ReadbackStage::SceneColor :
                    cse::render::ReadbackStage::PresentedColor;
            } else {
                Refuse(Status::InvalidValue);
            }
        }
        return;
    }
    CaptureUnsupported();
    if (CanForward(driverReferences.ReadBuffer != nullptr)) {
        driverReferences.ReadBuffer(buffer);
    }
}

void APIENTRY CseRenderDrawBuffer(GLenum buffer) {
    operationContext = __func__;
    if (Native()) {
        if (CanMutateNativeState() && buffer != GL_BACK) {
            // Environment-map/time-refresh owners route native FRONT work explicitly.
            Refuse(Status::InvalidOperation);
        }
        return;
    }
    CaptureUnsupported();
    if (CanForward(driverReferences.DrawBuffer != nullptr)) {
        driverReferences.DrawBuffer(buffer);
    }
}

void APIENTRY CseRenderPolygonMode(GLenum face, GLenum mode) {
    operationContext = __func__;
    constexpr GLenum kPolygonModes[] = {GL_POINT, GL_LINE, GL_FILL};
    CaptureValues(Op::PolygonMode, {}, Argument(face, kCullModes), Argument(mode, kPolygonModes));
    if (CanForward(driverReferences.PolygonMode != nullptr)) {
        driverReferences.PolygonMode(face, mode);
    }
}

void APIENTRY CseRenderFrontFace(GLenum mode) {
    operationContext = __func__;
    CaptureValues(Op::FrontFace, {}, mode == GL_CCW ? 1 : mode == GL_CW ? 0 : kInvalidArgument);
    if (CanForward(driverReferences.FrontFace != nullptr)) {
        driverReferences.FrontFace(mode);
    }
}

void APIENTRY CseRenderClearDepth(GLclampd depth) {
    operationContext = __func__;
    if (Native()) {
        if (CanMutateNativeState()) {
            if (!std::isfinite(depth)) {
                Refuse(Status::InvalidValue);
            } else {
                nativeClear.depth = std::max(0.0, std::min(depth, 1.0));
            }
        }
        return;
    }
    CaptureUnsupported();
    if (CanForward(driverReferences.ClearDepth != nullptr)) {
        driverReferences.ClearDepth(depth);
    }
}

void APIENTRY CseRenderClearStencil(GLint stencil) {
    operationContext = __func__;
    if (Native()) {
        if (CanMutateNativeState()) {
            nativeClear.stencil = static_cast<std::uint32_t>(stencil);
        }
        return;
    }
    CaptureUnsupported();
    if (CanForward(driverReferences.ClearStencil != nullptr)) {
        driverReferences.ClearStencil(stencil);
    }
}

void APIENTRY CseRenderColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
    operationContext = __func__;
    CaptureValues(Op::ColorWrite, {{red != GL_FALSE ? 1.0 : 0.0, green != GL_FALSE ? 1.0 : 0.0,
        blue != GL_FALSE ? 1.0 : 0.0, alpha != GL_FALSE ? 1.0 : 0.0}});
    if (CanForward(driverReferences.ColorMask != nullptr)) {
        driverReferences.ColorMask(red, green, blue, alpha);
    }
}

namespace {
void CaptureMatrix(const GLfloat* matrix, Op operation) {
    CaptureOwned([&] {
        Command command;
        command.op = operation;
        if (!PointerExtent(matrix, sizeof(command.matrix.values))) {
            Refuse(Status::InvalidValue);
            return;
        }
        std::copy(matrix, matrix + command.matrix.values.size(), command.matrix.values.begin());
        Capture(command);
    });
}
}

void APIENTRY CseRenderLoadMatrixf(const GLfloat* matrix) {
    operationContext = __func__;
    CaptureMatrix(matrix, Op::LoadMatrix);
    if (CanForward(driverReferences.LoadMatrixf != nullptr)) {
        driverReferences.LoadMatrixf(matrix);
    }
}

void APIENTRY CseRenderMultMatrixf(const GLfloat* matrix) {
    operationContext = __func__;
    CaptureMatrix(matrix, Op::MultiplyMatrix);
    if (CanForward(driverReferences.MultMatrixf != nullptr)) {
        driverReferences.MultMatrixf(matrix);
    }
}
