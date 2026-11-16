// Intentionally absent in the first immutable RED snapshot: production API, never a fixture substitute.
#include "legacy_dispatch.h"
#include "frame_stream.h"
#include <SDL3/SDL_opengl.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <type_traits>
#include <vector>

// Exact GL types and APIENTRY are inherited from the retained SDL GL declarations.
// These are engine qgl linkage slots, with a CPU driver spy; no GL implementation is linked.
#define CORE_SLOTS(X) \
    X(AlphaFunc) X(Begin) X(BlendFunc) X(Clear) X(ClearColor) X(Color3f) X(Color4f) \
    X(Color4ub) X(Color4ubv) X(CullFace) X(DepthFunc) X(DepthMask) X(DepthRange) X(Disable) \
    X(Enable) X(End) X(Finish) X(Fogf) X(Fogfv) X(Fogi) X(Frustum) X(GetFloatv) X(GetIntegerv) \
    X(GetString) X(Hint) X(IsEnabled) X(LoadIdentity) X(MatrixMode) X(Ortho) X(PopMatrix) \
    X(PushMatrix) X(Rotatef) X(Scalef) X(ShadeModel) X(TexCoord2f) X(TexEnvf) X(TexEnvi) \
    X(TexParameterf) X(Translatef) X(Vertex2f) X(Vertex3f) X(Vertex3fv) X(Viewport)
#define DEFINE_SLOT(name) decltype(&gl##name) qgl##name = nullptr;
CORE_SLOTS(DEFINE_SLOT)
#undef DEFINE_SLOT
#define CHECK_SIGNATURE(name) \
    static_assert(std::is_same<decltype(&CseRender##name), decltype(&gl##name)>::value, "GL ABI: " #name);
CORE_SLOTS(CHECK_SIGNATURE)
#undef CHECK_SIGNATURE
void (APIENTRY* qglSelectTextureSGIS)(GLenum) = nullptr;
void (APIENTRY* qglMTexCoord2fSGIS)(GLenum, GLfloat, GLfloat) = nullptr;

#include "t2b_slots.inc"
#define NATIVE_SLOTS(X) X(ReadPixels) X(ReadBuffer) X(DrawBuffer) X(PolygonMode) X(FrontFace) \
    X(ClearDepth) X(ClearStencil) X(ColorMask) X(LoadMatrixf) X(MultMatrixf)
#define NATIVE_DEFINE_SLOT(name) decltype(&gl##name) qgl##name = nullptr;
NATIVE_SLOTS(NATIVE_DEFINE_SLOT)
#undef NATIVE_DEFINE_SLOT
#define NATIVE_CHECK_SIGNATURE(name) \
    static_assert(std::is_same<decltype(&CseRender##name), decltype(&gl##name)>::value, "Native GL ABI: " #name);
NATIVE_SLOTS(NATIVE_CHECK_SIGNATURE)
#undef NATIVE_CHECK_SIGNATURE

namespace {
using namespace cse::render;

struct Event {
    std::string name;
    std::vector<double> values;
    const void* pointer = nullptr;
};
std::vector<Event> events;

void Check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL dispatch: %s\n", message);
        std::exit(1);
    }
}

void Record(const char* name, std::initializer_list<double> values = {}, const void* pointer = nullptr) {
    events.push_back({name, values, pointer});
}

void Expect(std::initializer_list<Event> expected) {
    Check(events.size() == expected.size(), "driver event count changed");
    std::size_t index = 0;
    for (const auto& event : expected) {
        const auto& actual = events[index++];
        Check(actual.name == event.name, "driver call order changed");
        Check(actual.values == event.values, "driver parameter value/type conversion changed");
        Check(actual.pointer == event.pointer, "driver borrowed pointer changed");
    }
}

#define SPY(name, parameters, ...) \
    void APIENTRY Spy##name parameters { Record(#name, {__VA_ARGS__}); }
SPY(AlphaFunc, (GLenum function, GLclampf reference), double(function), reference)
SPY(Begin, (GLenum mode), double(mode))
SPY(BlendFunc, (GLenum source, GLenum destination), double(source), double(destination))
SPY(Clear, (GLbitfield mask), double(mask))
SPY(ClearColor, (GLclampf r, GLclampf g, GLclampf b, GLclampf a), r, g, b, a)
SPY(Color3f, (GLfloat r, GLfloat g, GLfloat b), r, g, b)
SPY(Color4f, (GLfloat r, GLfloat g, GLfloat b, GLfloat a), r, g, b, a)
SPY(Color4ub, (GLubyte r, GLubyte g, GLubyte b, GLubyte a), double(r), double(g), double(b), double(a))
SPY(CullFace, (GLenum mode), double(mode))
SPY(DepthFunc, (GLenum function), double(function))
SPY(DepthMask, (GLboolean enabled), double(enabled))
SPY(DepthRange, (GLclampd nearDepth, GLclampd farDepth), nearDepth, farDepth)
SPY(Disable, (GLenum capability), double(capability))
SPY(Enable, (GLenum capability), double(capability))
SPY(End, ())
SPY(Finish, ())
SPY(Fogf, (GLenum name, GLfloat value), double(name), value)
SPY(Fogi, (GLenum name, GLint value), double(name), double(value))
SPY(Frustum, (GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f), l, r, b, t, n, f)
SPY(Hint, (GLenum target, GLenum mode), double(target), double(mode))
SPY(LoadIdentity, ())
SPY(MatrixMode, (GLenum mode), double(mode))
SPY(Ortho, (GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f), l, r, b, t, n, f)
SPY(PopMatrix, ())
SPY(PushMatrix, ())
SPY(Rotatef, (GLfloat angle, GLfloat x, GLfloat y, GLfloat z), angle, x, y, z)
SPY(Scalef, (GLfloat x, GLfloat y, GLfloat z), x, y, z)
SPY(ShadeModel, (GLenum mode), double(mode))
SPY(TexCoord2f, (GLfloat s, GLfloat t), s, t)
SPY(TexEnvf, (GLenum target, GLenum name, GLfloat value), double(target), double(name), value)
SPY(TexEnvi, (GLenum target, GLenum name, GLint value), double(target), double(name), double(value))
SPY(TexParameterf, (GLenum target, GLenum name, GLfloat value), double(target), double(name), value)
SPY(Translatef, (GLfloat x, GLfloat y, GLfloat z), x, y, z)
SPY(Vertex2f, (GLfloat x, GLfloat y), x, y)
SPY(Vertex3f, (GLfloat x, GLfloat y, GLfloat z), x, y, z)
SPY(Viewport, (GLint x, GLint y, GLsizei width, GLsizei height), double(x), double(y), double(width), double(height))
SPY(SelectTexture, (GLenum unit), double(unit))
SPY(MTexCoord, (GLenum unit, GLfloat s, GLfloat t), double(unit), s, t)
#undef SPY

void APIENTRY SpyColor4ubv(const GLubyte* color) {
    Record("Color4ubv", {double(color[0]), double(color[1]), double(color[2]), double(color[3])}, color);
}
void APIENTRY SpyVertex3fv(const GLfloat* vertex) {
    Record("Vertex3fv", {vertex[0], vertex[1], vertex[2]}, vertex);
}
void APIENTRY SpyFogfv(GLenum name, const GLfloat* values) {
    Record("Fogfv", {double(name), values[0], values[1], values[2], values[3]}, values);
}
void APIENTRY SpyGetFloatv(GLenum name, GLfloat* values) {
    Record("GetFloatv", {double(name)}, values);
    // Deliberately distinguish real driver output from semantic CPU state in forwarding modes.
    constexpr float kDriverMatrixSentinel = 73.25f;
    values[0] = kDriverMatrixSentinel;
}
void APIENTRY SpyGetIntegerv(GLenum name, GLint* value) {
    Record("GetIntegerv", {double(name)}, value);
    *value = 8;
}
GLboolean APIENTRY SpyIsEnabled(GLenum capability) {
    Record("IsEnabled", {double(capability)});
    return GL_TRUE;
}
const GLubyte* APIENTRY SpyGetString(GLenum name) {
    Record("GetString", {double(name)});
    static const GLubyte value[] = "driver-owned-version";
    return value;
}

void ResetDriver() {
    RemoveLegacyDispatch();
#define RESET_SLOT(name) qgl##name = &Spy##name;
    CORE_SLOTS(RESET_SLOT)
#undef RESET_SLOT
    qglSelectTextureSGIS = nullptr;
    qglMTexCoord2fSGIS = nullptr;
    events.clear();
}

void Install(DispatchMode mode, FrameRecorder* frame) {
    std::string error;
    Check(InstallLegacyDispatch(mode, frame, error), error.c_str());
}

void Start(FrameRecorder& frame) {
    constexpr std::uint32_t kFixtureExtent = 64;
    Check(frame.BeginFrame({1, 1, kFixtureExtent, kFixtureExtent}) == Status::Ok, "frame begin rejected");
}

void Lifecycle() {
    ResetDriver();
    Install(DispatchMode::ReferenceGL, nullptr);
    Check(qglBegin == &CseRenderBegin && qglEnd == &CseRenderEnd, "qgl slots are not owned");
    qglBegin(GL_TRIANGLES);
    qglEnd();
    Install(DispatchMode::ReferenceGL, nullptr);
    qglBegin(GL_LINES);
    qglEnd();
    RemoveLegacyDispatch();
    Check(qglBegin == &SpyBegin && qglEnd == &SpyEnd, "removal did not restore driver slots");
    RemoveLegacyDispatch();
    Install(DispatchMode::ReferenceGL, nullptr);
    CseRenderBegin(GL_QUADS);
    CseRenderEnd();
    Expect({{"Begin", {GL_TRIANGLES}}, {"End", {}}, {"Begin", {GL_LINES}}, {"End", {}},
        {"Begin", {GL_QUADS}}, {"End", {}}});
    RemoveLegacyDispatch();
    qglGetString = nullptr;
    Install(DispatchMode::ReferenceGL, nullptr);
    Check(qglGetString == nullptr, "absent core procedure advertised");
}

void APIENTRY SpyLoggedBegin(GLenum primitive) {
    Record("LogBegin", {double(primitive)});
    SpyBegin(primitive);
}

void RefreshOwnership() {
    ResetDriver();
    qglGetString = nullptr;
    Install(DispatchMode::ReferenceGL, nullptr);
    // Logging replaces qgl with an intermediary, and refresh must retain ownership without recursion.
    qglBegin = &SpyLoggedBegin;
    qglGetString = &SpyGetString;
    RefreshLegacyDispatchReferences();
    Check(qglBegin == &CseRenderBegin, "logging refresh lost qgl ownership");
    Check(qglGetString == nullptr, "logging refresh advertised absent procedure");
    qglBegin(GL_TRIANGLES);
    qglEnd();
    Expect({{"LogBegin", {GL_TRIANGLES}}, {"Begin", {GL_TRIANGLES}}, {"End", {}}});
    RemoveLegacyDispatch();
    Check(qglBegin == &SpyLoggedBegin && qglGetString == nullptr, "refresh restore changed availability");
}

void QueryRefusal() {
    ResetDriver();
    FrameRecorder frame;
    Install(DispatchMode::CaptureOnly, &frame);
    constexpr GLfloat kUntouchedQueryValue = 19.5f;
    std::array<GLfloat, 16> values;
    values.fill(kUntouchedQueryValue);
    const auto original = values;
    qglGetFloatv(GL_MODELVIEW_MATRIX, values.data());
    Check(values == original && LegacyDispatchStatus() == Status::InvalidOperation,
        "inactive query fabricated output or succeeded");
    Check(qglIsEnabled(GL_FOG) == GL_FALSE && events.empty(), "inactive query invoked GL");
    RemoveLegacyDispatch();
    Start(frame);
    Check(frame.Refuse(Status::LimitExceeded) == Status::LimitExceeded, "query refusal setup failed");
    Install(DispatchMode::CaptureOnly, &frame);
    qglGetFloatv(GL_MODELVIEW_MATRIX, values.data());
    Check(values == original && LegacyDispatchStatus() == Status::LimitExceeded,
        "failed frame query wrote output or lost first error");
    Check(qglIsEnabled(GL_FOG) == GL_FALSE && events.empty(), "failed query invoked GL");
    RemoveLegacyDispatch();
    frame.Abort();
    Start(frame);
    Install(DispatchMode::CaptureOnly, &frame);
    qglGetFloatv(GL_MODELVIEW_MATRIX, values.data());
    Check(values == Matrix4::Identity().values && LegacyDispatchStatus() == Status::Ok,
        "active query failed after Abort recovery");
    FrameSnapshot snapshot;
    Check(frame.Seal(snapshot) == Status::Ok && snapshot.events.empty(), "read-only query submitted a command");
    values = original;
    qglGetFloatv(GL_MODELVIEW_MATRIX, values.data());
    Check(values == original && LegacyDispatchStatus() == Status::InvalidOperation && events.empty(),
        "sealed frame query fabricated output or invoked GL");
}

void GetFloatvBeginEnd() {
    ResetDriver();
    FrameRecorder frame;
    Start(frame);
    Install(DispatchMode::CaptureOnly, &frame);
    constexpr GLfloat kUntouchedQueryValue = 29.5f;
    const std::array<GLfloat, 16> identity{{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
    std::array<GLfloat, 16> values;
    values.fill(kUntouchedQueryValue);
    qglGetFloatv(GL_MODELVIEW_MATRIX, values.data());
    Check(values == identity && LegacyDispatchStatus() == Status::Ok,
        "GetFloatv outside Begin/End did not return the modelview matrix");
    values.fill(kUntouchedQueryValue);
    const auto untouched = values;
    qglBegin(GL_TRIANGLES);
    qglGetFloatv(GL_MODELVIEW_MATRIX, values.data());
    Check(values == untouched, "GetFloatv inside Begin/End overwrote query output");
    Check(LegacyDispatchStatus() == Status::InvalidOperation,
        "GetFloatv inside Begin/End did not refuse the operation");
    Check(frame.Refuse(Status::LimitExceeded) == Status::InvalidOperation,
        "GetFloatv refusal lost the first error");
    qglEnd();
    qglGetFloatv(GL_MODELVIEW_MATRIX, values.data());
    Check(values == untouched && LegacyDispatchStatus() == Status::InvalidOperation && events.empty(),
        "GetFloatv refusal was cleared by End, wrote output, or invoked GL");
    FrameSnapshot snapshot;
    constexpr std::uint64_t kUntouchedSnapshotSerial = 93;
    snapshot.info.serial = kUntouchedSnapshotSerial;
    Check(frame.Seal(snapshot) == Status::InvalidOperation && snapshot.info.serial == kUntouchedSnapshotSerial &&
        snapshot.events.empty() && snapshot.draws.empty(), "GetFloatv refused frame sealed or mutated output");
    RemoveLegacyDispatch();
    Install(DispatchMode::CaptureOnly, &frame);
    qglGetFloatv(GL_MODELVIEW_MATRIX, values.data());
    Check(values == untouched && LegacyDispatchStatus() == Status::InvalidOperation,
        "dispatch reinstall cleared GetFloatv frame refusal without Abort");
    RemoveLegacyDispatch();
    frame.Abort();
    Start(frame);
    Install(DispatchMode::CaptureOnly, &frame);
    qglGetFloatv(GL_MODELVIEW_MATRIX, values.data());
    Check(values == identity && LegacyDispatchStatus() == Status::Ok,
        "GetFloatv did not recover after Abort");
    Check(frame.Seal(snapshot) == Status::Ok && snapshot.events.empty() && snapshot.draws.empty() && events.empty(),
        "GetFloatv after Abort submitted commands or invoked GL");
}

void IsEnabledBeginEnd() {
    ResetDriver();
    FrameRecorder frame;
    Start(frame);
    Install(DispatchMode::CaptureOnly, &frame);
    qglEnable(GL_FOG);
    Check(qglIsEnabled(GL_FOG) == GL_TRUE && LegacyDispatchStatus() == Status::Ok,
        "IsEnabled outside Begin/End did not return enabled state");
    qglBegin(GL_TRIANGLES);
    Check(qglIsEnabled(GL_FOG) == GL_FALSE, "IsEnabled inside Begin/End returned enabled state");
    Check(LegacyDispatchStatus() == Status::InvalidOperation,
        "IsEnabled inside Begin/End did not refuse the operation");
    Check(frame.Refuse(Status::LimitExceeded) == Status::InvalidOperation,
        "IsEnabled refusal lost the first error");
    qglEnd();
    Check(qglIsEnabled(GL_FOG) == GL_FALSE && LegacyDispatchStatus() == Status::InvalidOperation && events.empty(),
        "IsEnabled refusal was cleared by End or invoked GL");
    FrameSnapshot snapshot;
    constexpr std::uint64_t kUntouchedSnapshotSerial = 97;
    snapshot.info.serial = kUntouchedSnapshotSerial;
    Check(frame.Seal(snapshot) == Status::InvalidOperation && snapshot.info.serial == kUntouchedSnapshotSerial &&
        snapshot.events.empty() && snapshot.draws.empty(), "IsEnabled refused frame sealed or mutated output");
    RemoveLegacyDispatch();
    Install(DispatchMode::CaptureOnly, &frame);
    Check(qglIsEnabled(GL_FOG) == GL_FALSE && LegacyDispatchStatus() == Status::InvalidOperation,
        "dispatch reinstall cleared IsEnabled frame refusal without Abort");
    RemoveLegacyDispatch();
    frame.Abort();
    Start(frame);
    Install(DispatchMode::CaptureOnly, &frame);
    qglEnable(GL_FOG);
    Check(qglIsEnabled(GL_FOG) == GL_TRUE && LegacyDispatchStatus() == Status::Ok,
        "IsEnabled did not recover after Abort");
    qglDisable(GL_FOG);
    Check(qglIsEnabled(GL_FOG) == GL_FALSE && LegacyDispatchStatus() == Status::Ok,
        "IsEnabled outside Begin/End did not return disabled state");
    Check(frame.Seal(snapshot) == Status::Ok && snapshot.events.empty() && snapshot.draws.empty() && events.empty(),
        "IsEnabled after Abort submitted draws or invoked GL");
}

void ForwardingSurface() {
    ResetDriver();
    Install(DispatchMode::ReferenceGL, nullptr);
    const std::array<GLfloat, 4> fog{{0.25f, 0.5f, 0.75f, 1}};
    GLint units = 0;
    qglAlphaFunc(GL_GREATER, 0.25f);
    qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    qglClearColor(0.25f, 0.5f, 0.75f, 1);
    qglColor3f(0.25f, 0.5f, 0.75f);
    qglColor4ub(17, 33, 65, 129);
    qglCullFace(GL_FRONT);
    qglDepthFunc(GL_LEQUAL);
    qglDepthMask(GL_FALSE);
    qglDepthRange(0.25, 0.75);
    qglFogfv(GL_FOG_COLOR, fog.data());
    qglFogi(GL_FOG_MODE, GL_LINEAR);
    qglHint(GL_FOG_HINT, GL_NICEST);
    qglShadeModel(GL_FLAT);
    qglTexCoord2f(0.25f, 0.75f);
    qglTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 2);
    qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    qglGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &units);
    const auto version = qglGetString(GL_VERSION);
    qglFinish();
    Check(units == 8 && std::string(reinterpret_cast<const char*>(version)) == "driver-owned-version",
        "reference capability queries changed");
    Expect({{"AlphaFunc", {GL_GREATER, 0.25}}, {"BlendFunc", {GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA}},
        {"Clear", {GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT}}, {"ClearColor", {0.25, 0.5, 0.75, 1}},
        {"Color3f", {0.25, 0.5, 0.75}}, {"Color4ub", {17, 33, 65, 129}}, {"CullFace", {GL_FRONT}},
        {"DepthFunc", {GL_LEQUAL}}, {"DepthMask", {GL_FALSE}}, {"DepthRange", {0.25, 0.75}},
        {"Fogfv", {GL_FOG_COLOR, 0.25, 0.5, 0.75, 1}, fog.data()}, {"Fogi", {GL_FOG_MODE, GL_LINEAR}},
        {"Hint", {GL_FOG_HINT, GL_NICEST}}, {"ShadeModel", {GL_FLAT}}, {"TexCoord2f", {0.25, 0.75}},
        {"TexEnvf", {GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 2}},
        {"TexEnvi", {GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE}},
        {"TexParameterf", {GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR}},
        {"GetIntegerv", {GL_MAX_TEXTURE_UNITS_ARB}, &units}, {"GetString", {GL_VERSION}}, {"Finish", {}}});
}

void DeferredOwnershipRefusal() {
    ResetDriver();
    FrameRecorder frame;
    Start(frame);
    Install(DispatchMode::ShadowCapture, &frame);
    qglClearColor(0.25f, 0.5f, 0.75f, 1);
    qglClear(GL_COLOR_BUFFER_BIT);
    qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    RefreshLegacyDispatchReferences();
    Check(LegacyDispatchStatus() != Status::Ok, "logging refresh cleared shadow refusal");
    qglFinish();
    Expect({{"ClearColor", {0.25, 0.5, 0.75, 1}}, {"Clear", {GL_COLOR_BUFFER_BIT}},
        {"TexParameterf", {GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR}}, {"Finish", {}}});
    FrameSnapshot snapshot;
    Check(frame.Seal(snapshot) != Status::Ok, "unowned clear/texture state accepted partial frame");
    RemoveLegacyDispatch();
    frame.Abort();
    Start(frame);
    events.clear();
    Install(DispatchMode::CaptureOnly, &frame);
    GLint untouched = -17;
    qglGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &untouched);
    Check(untouched == -17 && LegacyDispatchStatus() != Status::Ok, "capture fabricated backend limits");
    Check(qglGetString(GL_VERSION) == nullptr, "capture fabricated GL version");
    qglClear(GL_COLOR_BUFFER_BIT);
    qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    Check(events.empty() && frame.Seal(snapshot) != Status::Ok, "unsupported capture reached GL or accepted frame");
}

void MixedOwner() {
    ResetDriver();
    FrameRecorder frame;
    Start(frame);
    Install(DispatchMode::ShadowCapture, &frame);
    const GLubyte color[] = {17, 33, 65, 129};
    const GLfloat vertex[] = {4, 5, 6};
    CseRenderColor4ubv(color);
    qglBegin(GL_TRIANGLES);
    CseRenderVertex3fv(vertex);
    qglVertex3f(7, 8, 9);
    CseRenderVertex3f(10, 11, 12);
    qglEnd();
    Expect({{"Color4ubv", {17, 33, 65, 129}, color}, {"Begin", {GL_TRIANGLES}},
        {"Vertex3fv", {4, 5, 6}, vertex}, {"Vertex3f", {7, 8, 9}}, {"Vertex3f", {10, 11, 12}}, {"End", {}}});
    FrameSnapshot snapshot;
    Check(frame.Seal(snapshot) == Status::Ok, "mixed capture refused");
    Check(snapshot.events.size() == 1 && snapshot.draws.size() == 1, "raw and qgl do not share frame owner");
    const auto& vertices = snapshot.draws[0].vertices;
    Check(vertices.size() == 3 && vertices[0].position[0] == 4 && vertices[2].position[2] == 12,
        "mixed vertices missing/reordered");
    Check(vertices[0].color[3] == 129.0f / 255.0f, "ubyte color normalization changed");
}

void MatricesQueries() {
    ResetDriver();
    FrameRecorder frame;
    Start(frame);
    Install(DispatchMode::ShadowCapture, &frame);
    std::array<GLfloat, 16> result{};
    qglMatrixMode(GL_MODELVIEW);
    qglLoadIdentity();
    qglPushMatrix();
    qglTranslatef(3, 4, 5);
    qglRotatef(90, 0, 0, 1);
    qglScalef(2, 3, 4);
    qglGetFloatv(GL_MODELVIEW_MATRIX, result.data());
    Check(result[0] == 73.25f, "shadow replaced driver query result");
    Check(qglIsEnabled(GL_FOG) == GL_TRUE, "shadow replaced driver enabled result");
    qglPopMatrix();
    qglMatrixMode(GL_PROJECTION);
    qglLoadIdentity();
    qglFrustum(-1, 1, -2, 2, 4, 64);
    Expect({{"MatrixMode", {GL_MODELVIEW}}, {"LoadIdentity", {}}, {"PushMatrix", {}},
        {"Translatef", {3, 4, 5}}, {"Rotatef", {90, 0, 0, 1}}, {"Scalef", {2, 3, 4}},
        {"GetFloatv", {GL_MODELVIEW_MATRIX}, result.data()}, {"IsEnabled", {GL_FOG}}, {"PopMatrix", {}},
        {"MatrixMode", {GL_PROJECTION}}, {"LoadIdentity", {}}, {"Frustum", {-1, 1, -2, 2, 4, 64}}});
    Check(frame.State().Depth(MatrixMode::ModelView) == 1, "modelview pop lost");
    Check(frame.State().Matrix(MatrixMode::ModelView).values == Matrix4::Identity().values, "matrix restore lost");
    Check(frame.State().Matrix(MatrixMode::Projection).values[0] == 4, "frustum did not reach CPU owner");
    RemoveLegacyDispatch();
    events.clear();
    Install(DispatchMode::CaptureOnly, &frame);
    qglMatrixMode(GL_MODELVIEW);
    qglTranslatef(3, 4, 5);
    result.fill(0);
    qglGetFloatv(GL_MODELVIEW_MATRIX, result.data());
    Check(result[12] == 3 && result[13] == 4 && result[14] == 5, "capture matrix query not semantic");
    qglEnable(GL_FOG);
    Check(qglIsEnabled(GL_FOG) == GL_TRUE, "capture enable query not semantic");
    Check(events.empty(), "capture query invoked GL");
}

void ShadowFailure() {
    ResetDriver();
    FrameLimits limits;
    limits.capture.verticesPerDraw = 1;
    FrameRecorder frame(limits);
    Start(frame);
    Install(DispatchMode::ShadowCapture, &frame);
    qglBegin(GL_TRIANGLES);
    qglVertex3f(1, 2, 3);
    qglVertex3f(4, 5, 6); // CPU bound fails; the driver still receives this and subsequent calls.
    qglVertex3f(7, 8, 9);
    qglEnd();
    CseRenderEnable(GL_FOG); // gl_rmain raw path must remain ordered after semantic failure.
    CseRenderFogf(GL_FOG_START, 2);
    qglColor4f(1, 0, 0, 1);
    Expect({{"Begin", {GL_TRIANGLES}}, {"Vertex3f", {1, 2, 3}}, {"Vertex3f", {4, 5, 6}},
        {"Vertex3f", {7, 8, 9}}, {"End", {}}, {"Enable", {GL_FOG}}, {"Fogf", {GL_FOG_START, 2}},
        {"Color4f", {1, 0, 0, 1}}});
    Check(LegacyDispatchStatus() != Status::Ok, "shadow failure not recorded");
    FrameSnapshot snapshot;
    Check(frame.Seal(snapshot) != Status::Ok, "failed shadow frame accepted");
}

void CaptureRefusal() {
    ResetDriver();
    FrameRecorder frame;
    Start(frame);
    Install(DispatchMode::CaptureOnly, &frame);
    constexpr GLenum kUnsupportedCapability = 0xFFFFFFFFu;
    CseRenderEnable(kUnsupportedCapability);
    qglBegin(GL_TRIANGLES);
    qglEnd();
    Check(events.empty(), "unexpected capture API invoked GL");
    Check(LegacyDispatchStatus() != Status::Ok, "unsupported capture API did not refuse");
    FrameSnapshot snapshot;
    Check(frame.Seal(snapshot) != Status::Ok, "unsupported capture frame accepted");
    RemoveLegacyDispatch();
    std::string error;
    Check(!InstallLegacyDispatch(DispatchMode::CaptureOnly, nullptr, error), "unbound capture accepted");
    Check(!error.empty(), "unbound capture lacks diagnostic");
    frame.Abort();
    Start(frame);
    Install(DispatchMode::CaptureOnly, &frame);
    CseRenderFinish(); // T3 Session is unbound: this cannot pretend to submit/wait or call GL.
    Check(LegacyDispatchStatus() != Status::Ok && events.empty(), "unbound Finish did not refuse without GL");
    Check(frame.Seal(snapshot) != Status::Ok, "unbound Finish accepted partial frame");
}

void RefusalGate() {
    FrameRecorder frame;
    Start(frame);
    Check(frame.Refuse(Status::LimitExceeded) == Status::LimitExceeded, "first refusal not retained");
    Check(frame.Refuse(Status::InvalidValue) == Status::LimitExceeded, "later refusal replaced first error");
    Check(frame.Refuse(Status::Ok) == Status::LimitExceeded, "Ok cleared sticky refusal");
    FrameSnapshot snapshot;
    snapshot.info.serial = 91;
    Check(frame.Seal(snapshot) == Status::LimitExceeded && snapshot.info.serial == 91,
        "refused Seal mutated output or lost first error");
    frame.Abort();
    Start(frame);
    Check(frame.Refuse(Status::Ok) != Status::Ok, "Ok is not a valid refusal");
    Check(frame.Seal(snapshot) != Status::Ok, "invalid refusal allowed Seal");
    frame.Abort();
    Start(frame);
    Check(frame.Seal(snapshot) == Status::Ok, "Abort did not reset refusal");
}

void ExtensionAliases() {
    ResetDriver();
    FrameRecorder frame;
    Start(frame);
    Install(DispatchMode::ShadowCapture, &frame);
    const auto select = reinterpret_cast<LegacyProcedure>(&SpySelectTexture);
    const auto coord = reinterpret_cast<LegacyProcedure>(&SpyMTexCoord);
    qglSelectTextureSGIS = reinterpret_cast<decltype(qglSelectTextureSGIS)>(
        RouteLegacyExtension("glActiveTextureARB", select));
    qglMTexCoord2fSGIS = reinterpret_cast<decltype(qglMTexCoord2fSGIS)>(
        RouteLegacyExtension("glMultiTexCoord2fARB", coord));
    Check(qglSelectTextureSGIS == &CseRenderSelectTextureSGIS, "ARB probing bypassed wrapper");
    Check(qglMTexCoord2fSGIS == &CseRenderMTexCoord2fSGIS, "ARB coordinate probing bypassed wrapper");
    qglSelectTextureSGIS(GL_TEXTURE1_ARB);
    qglMTexCoord2fSGIS(GL_TEXTURE1_ARB, 0.25f, 0.75f);
    Check(frame.State().ActiveTexture() == 1, "ARB unit not normalized for CPU state");
    Check(frame.State().Current().texcoords[1][0] == 0.25f, "ARB coordinate missing");
    qglSelectTextureSGIS = reinterpret_cast<decltype(qglSelectTextureSGIS)>(
        RouteLegacyExtension("glSelectTextureSGIS", select));
    constexpr GLenum kTexture2Sgis = 0x8360;
    qglSelectTextureSGIS(kTexture2Sgis);
    Check(frame.State().ActiveTexture() == 2, "SGIS unit not normalized for CPU state");
    Expect({{"SelectTexture", {GL_TEXTURE1_ARB}}, {"MTexCoord", {GL_TEXTURE1_ARB, 0.25, 0.75}},
        {"SelectTexture", {kTexture2Sgis}}});
    Check(RouteLegacyExtension("glActiveTextureARB", nullptr) == nullptr, "absent proc advertised");
    Check(RouteLegacyExtension("glUnknownExtensionCSE", select) == nullptr, "unsupported proc advertised");
    RemoveLegacyDispatch();
    Check(qglSelectTextureSGIS == nullptr, "removed absent alias restored stale proc");
}

void HudTrace() {
    ResetDriver();
    FrameRecorder frame;
    Start(frame);
    Install(DispatchMode::ShadowCapture, &frame);
    // Independently spelled source-shaped GLBeginHud / triangle / GLFinishHud sequence.
    qglViewport(0, 0, 64, 64);
    qglMatrixMode(GL_PROJECTION);
    qglPushMatrix();
    qglLoadIdentity();
    qglOrtho(0, 64, 64, 0, -99999, 99999);
    qglMatrixMode(GL_MODELVIEW);
    qglPushMatrix();
    qglLoadIdentity();
    qglDisable(GL_DEPTH_TEST);
    qglDisable(GL_CULL_FACE);
    qglDisable(GL_BLEND);
    qglEnable(GL_ALPHA_TEST);
    qglColor4f(1, 1, 1, 1);
    qglBegin(GL_TRIANGLES);
    qglVertex2f(8, 8);
    qglVertex2f(24, 8);
    qglVertex2f(8, 24);
    qglEnd();
    qglMatrixMode(GL_PROJECTION);
    qglPopMatrix();
    qglMatrixMode(GL_MODELVIEW);
    qglPopMatrix();
    qglEnable(GL_DEPTH_TEST);
    qglEnable(GL_CULL_FACE);
    qglEnable(GL_BLEND);
    Expect({{"Viewport", {0, 0, 64, 64}}, {"MatrixMode", {GL_PROJECTION}}, {"PushMatrix", {}},
        {"LoadIdentity", {}}, {"Ortho", {0, 64, 64, 0, -99999, 99999}}, {"MatrixMode", {GL_MODELVIEW}},
        {"PushMatrix", {}}, {"LoadIdentity", {}}, {"Disable", {GL_DEPTH_TEST}}, {"Disable", {GL_CULL_FACE}},
        {"Disable", {GL_BLEND}}, {"Enable", {GL_ALPHA_TEST}}, {"Color4f", {1, 1, 1, 1}},
        {"Begin", {GL_TRIANGLES}}, {"Vertex2f", {8, 8}}, {"Vertex2f", {24, 8}}, {"Vertex2f", {8, 24}},
        {"End", {}}, {"MatrixMode", {GL_PROJECTION}}, {"PopMatrix", {}}, {"MatrixMode", {GL_MODELVIEW}},
        {"PopMatrix", {}}, {"Enable", {GL_DEPTH_TEST}}, {"Enable", {GL_CULL_FACE}}, {"Enable", {GL_BLEND}}});
    FrameSnapshot snapshot;
    Check(frame.Seal(snapshot) == Status::Ok && snapshot.draws.size() == 1, "HUD draw capture missing");
    Check(snapshot.draws[0].vertices.size() == 3, "HUD triangle vertices missing");
}
#include "t2b_cases.inc"
#include "native_cases.inc"
}

int main(int argc, char** argv) {
    Check(argc == 2, "one named case required");
    const std::string name = argv[1];
    if (name == "lifecycle") {
        ResetDriver();
        Install(DispatchMode::ReferenceGL, nullptr);
        Check(LegacyIndexedDrawingAllowed(), "reference renderer must permit indexed studio drawing");
        FrameRecorder frame;
        Install(DispatchMode::ShadowCapture, &frame);
        Check(!LegacyIndexedDrawingAllowed(), "shadow capture must retain immediate studio commands");
        Install(DispatchMode::CaptureOnly, &frame);
        Check(!LegacyIndexedDrawingAllowed(), "capture-only must retain immediate studio commands");
        Lifecycle();
        RefreshOwnership();
        ForwardingSurface();
    } else if (name == "mixed-owner") {
        MixedOwner();
    } else if (name == "matrices-queries") {
        MatricesQueries();
        QueryRefusal();
    } else if (name == "shadow-failure") {
        ShadowFailure();
    } else if (name == "capture-refusal") {
        CaptureRefusal();
        DeferredOwnershipRefusal();
    } else if (name == "refusal-gate") {
        RefusalGate();
    } else if (name == "extension-aliases") {
        ExtensionAliases();
    } else if (name == "hud-trace") {
        HudTrace();
    } else if (name == "getfloatv-begin-end") {
        GetFloatvBeginEnd();
    } else if (name == "isenabled-begin-end") {
        IsEnabledBeginEnd();
    } else if (name == "native-install") {
        NativeInstallRed();
    } else if (name == "native-noop-primitive-refusal") {
        NativeNoopPrimitiveRefusal();
    } else if (name == "native-palette-snapshot") {
        NativePaletteSnapshot();
    } else if (name == "native-startup-arrays") {
        NativeStartupArrays();
    } else if (name == "native-list-namespace") {
        NativeListNamespace();
    } else if (name == "native-callback-failure") {
        NativeCallbackFailure();
    } else if (name == "native-ensure-failure") {
        NativeEnsureFailure();
    } else if (name == "native-front-draw-refusal") {
        NativeFrontDrawRefusal();
    } else if (name == "native-list-clear-refusal") {
        NativeListClearRefusal();
    } else if (name == "native-alias-ownership") {
        NativeAliasOwnership();
    } else if (name == "native-commands") {
        NativeCommands();
    } else if (name == "native-indexed") {
        NativeIndexed();
    } else if (name == "native-readback") {
        NativeReadback();
    } else if (name == "native-failure") {
        NativeFailure();
    } else if (RunT2bCase(name)) {
        // Additive T2b cases; existing bodies and assertions above remain unchanged.
    } else {
        Check(false, "unknown case");
    }
    RemoveLegacyDispatch();
    std::printf("PASS dispatch-%s\n", name.c_str());
    return 0;
}
