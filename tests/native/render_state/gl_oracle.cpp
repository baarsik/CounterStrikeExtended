#include "legacy_commands.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <fstream>
#include <string>
#include <vector>
#if defined(_WIN32)
#include <windows.h>
#endif

using namespace cse::render;

namespace {

constexpr int kOracleSide = 64;
constexpr float kOracleHalfExtent = kOracleSide / 2.0f;

bool diagnostic = false;
bool rasterEnabled = true;
bool textureMode = false;
bool textureRasterOnly = false;
int rasterFailures = 0;

void Check(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "FAIL actual GL oracle: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

template<class T> T Load(const char* name) {
    auto address = SDL_GL_GetProcAddress(name);
    Check(address != nullptr, name);
    return reinterpret_cast<T>(address);
}

#define GL_ENTRY(name) decltype(&gl##name) name = Load<decltype(&gl##name)>("gl" #name)
struct GL {
    GL_ENTRY(IsEnabled);
    GL_ENTRY(GetBooleanv);
    GL_ENTRY(GetDoublev);
    GL_ENTRY(Enable);
    GL_ENTRY(Disable);
    GL_ENTRY(BlendFunc);
    GL_ENTRY(DepthMask);
    GL_ENTRY(DepthFunc);
    GL_ENTRY(AlphaFunc);
    GL_ENTRY(CullFace);
    GL_ENTRY(Scissor);
    GL_ENTRY(DepthRange);
    GL_ENTRY(Clear);
    GL_ENTRY(ClearColor);
    GL_ENTRY(ReadPixels);
    GL_ENTRY(PixelStorei);
    GL_ENTRY(GetString);
    GL_ENTRY(GetError);
    GL_ENTRY(GetFloatv);
    GL_ENTRY(GetIntegerv);
    GL_ENTRY(MatrixMode);
    GL_ENTRY(LoadIdentity);
    GL_ENTRY(PushMatrix);
    GL_ENTRY(PopMatrix);
    GL_ENTRY(Translatef);
    GL_ENTRY(Scalef);
    GL_ENTRY(Rotatef);
    GL_ENTRY(Ortho);
    GL_ENTRY(Frustum);
    GL_ENTRY(Color4f);
    GL_ENTRY(TexCoord2f);
    GL_ENTRY(Viewport);
    GL_ENTRY(ShadeModel);
    GL_ENTRY(Begin);
    GL_ENTRY(Vertex3f);
    GL_ENTRY(Vertex4f);
    GL_ENTRY(End);
    GL_ENTRY(NewList);
    GL_ENTRY(EndList);
    GL_ENTRY(CallList);
    GL_ENTRY(DeleteLists);
    GL_ENTRY(FeedbackBuffer);
    GL_ENTRY(RenderMode);
    GL_ENTRY(VertexPointer);
    GL_ENTRY(TexCoordPointer);
    GL_ENTRY(EnableClientState);
    GL_ENTRY(DrawArrays);
    GL_ENTRY(DisableClientState);
    GL_ENTRY(BindTexture);
    GL_ENTRY(GenTextures);
    GL_ENTRY(DeleteTextures);
    GL_ENTRY(IsTexture);
    GL_ENTRY(TexImage2D);
    GL_ENTRY(TexSubImage2D);
    GL_ENTRY(GetTexImage);
    GL_ENTRY(TexParameteri);
    GL_ENTRY(TexParameterf);
    GL_ENTRY(GetTexParameteriv);
    GL_ENTRY(GetTexParameterfv);
    GL_ENTRY(TexEnvi);
    GL_ENTRY(GetTexEnviv);
    GL_ENTRY(Fogi);
    GL_ENTRY(Fogf);
    GL_ENTRY(Fogfv);
    GL_ENTRY(PolygonMode);
    GL_ENTRY(PolygonOffset);
    GL_ENTRY(FrontFace);
    GL_ENTRY(LoadMatrixf);
    GL_ENTRY(Hint);
    PFNGLACTIVETEXTUREPROC ActiveTexture = Load<PFNGLACTIVETEXTUREPROC>("glActiveTexture");
    using MultiTexCoordFunction = void (APIENTRY*)(GLenum, GLfloat, GLfloat);
    MultiTexCoordFunction MultiTexCoord2f = Load<MultiTexCoordFunction>("glMultiTexCoord2f");
};
#undef GL_ENTRY

constexpr std::array<GLenum, 8> primitives{{GL_TRIANGLES, GL_TRIANGLE_FAN, GL_QUADS, GL_POLYGON,
    GL_LINES, GL_TRIANGLE_STRIP, GL_QUAD_STRIP, GL_LINE_LOOP}};
constexpr std::array<GLenum, 8> capabilities{{GL_BLEND, GL_DEPTH_TEST, GL_ALPHA_TEST, GL_CULL_FACE,
    GL_SCISSOR_TEST, GL_FOG, GL_TEXTURE_2D, GL_TEXTURE_RECTANGLE}};
constexpr std::array<GLenum, 8> comparisons{{GL_NEVER, GL_LESS, GL_EQUAL, GL_LEQUAL, GL_GREATER, GL_NOTEQUAL,
    GL_GEQUAL, GL_ALWAYS}};
constexpr std::array<GLenum, 10> blendFactors{{GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR,
    GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_DST_COLOR,
    GL_ONE_MINUS_DST_COLOR}};
constexpr std::array<GLenum, 3> cullFaces{{GL_FRONT, GL_BACK, GL_FRONT_AND_BACK}};
constexpr std::array<GLenum, 3> matrixModes{{GL_MODELVIEW, GL_PROJECTION, GL_TEXTURE}};
constexpr std::array<GLenum, 2> textureTargets{{GL_TEXTURE_2D, GL_TEXTURE_RECTANGLE}};
constexpr std::array<GLenum, 7> pixelFormats{{GL_RGBA, GL_BGRA, GL_RGB, GL_LUMINANCE, GL_ALPHA,
    GL_LUMINANCE_ALPHA, GL_COLOR_INDEX}};
constexpr std::array<GLenum, 14> internalFormats{{GL_RGBA8, GL_RGB8, GL_RGB5_A1, GL_RGB5, GL_RGBA4,
    GL_LUMINANCE8, GL_ALPHA8, GL_INTENSITY8, GL_COLOR_INDEX8_EXT, GL_RGBA, GL_RGB, GL_LUMINANCE, GL_ALPHA,
    GL_INTENSITY}};
constexpr std::array<GLenum, 6> textureParameters{{GL_TEXTURE_MIN_FILTER, GL_TEXTURE_MAG_FILTER,
    GL_TEXTURE_WRAP_S, GL_TEXTURE_WRAP_T, GL_TEXTURE_MAX_LEVEL, GL_TEXTURE_MAX_ANISOTROPY_EXT}};
constexpr std::array<GLenum, 6> filters{{GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST,
    GL_LINEAR_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR}};
constexpr std::array<GLenum, 3> wraps{{GL_REPEAT, GL_CLAMP, GL_CLAMP_TO_EDGE}};
constexpr std::array<GLenum, 3> environments{{GL_MODULATE, GL_REPLACE, GL_COMBINE}};
constexpr std::array<GLenum, 3> pixelStores{{GL_UNPACK_ALIGNMENT, GL_UNPACK_ROW_LENGTH, GL_PACK_ALIGNMENT}};
constexpr std::array<GLenum, 3> fogModes{{GL_EXP, GL_EXP2, GL_LINEAR}};
constexpr std::array<GLenum, 3> polygonModes{{GL_POINT, GL_LINE, GL_FILL}};
constexpr std::array<GLenum, 3> hints{{GL_DONT_CARE, GL_FASTEST, GL_NICEST}};

Command Make(Op op, std::initializer_list<double> values = {}, std::uint32_t argument = 0) {
    Command command;
    command.op = op;
    command.argument = argument;
    std::size_t index = 0;
    for (double value : values) {
        command.values[index++] = value;
    }
    return command;
}

void Submit(GL& gl, Recorder& cpu, const Command& command) {
    Check(cpu.Submit(command) == Status::Ok, "CPU command accepted");
    const auto& v = command.values;
    const auto f = [&v](std::size_t index) {
        return static_cast<float>(v[index]);
    };
    switch (command.op) {
    case Op::MatrixMode:
        gl.MatrixMode(matrixModes[command.argument]);
        break;
    case Op::ActiveTexture:
        gl.ActiveTexture(GL_TEXTURE0 + command.argument);
        break;
    case Op::Identity:
        gl.LoadIdentity();
        break;
    case Op::Push:
        gl.PushMatrix();
        break;
    case Op::Pop:
        gl.PopMatrix();
        break;
    case Op::Translate:
        gl.Translatef(f(0), f(1), f(2));
        break;
    case Op::Scale:
        gl.Scalef(f(0), f(1), f(2));
        break;
    case Op::Rotate:
        gl.Rotatef(f(0), f(1), f(2), f(3));
        break;
    case Op::Ortho:
        gl.Ortho(v[0], v[1], v[2], v[3], v[4], v[5]);
        break;
    case Op::Frustum:
        gl.Frustum(v[0], v[1], v[2], v[3], v[4], v[5]);
        break;
    case Op::Color:
        gl.Color4f(f(0), f(1), f(2), f(3));
        break;
    case Op::TexCoord:
        gl.MultiTexCoord2f(GL_TEXTURE0 + command.argument, f(0), f(1));
        break;
    case Op::ShadeModel:
        gl.ShadeModel(command.argument == 0 ? GL_FLAT : GL_SMOOTH);
        break;
    case Op::Viewport:
        gl.Viewport(static_cast<int>(v[0]), static_cast<int>(v[1]), static_cast<int>(v[2]), static_cast<int>(v[3]));
        break;
    case Op::Enable: {
        const auto capability = command.argument == static_cast<std::uint32_t>(Capability::PolygonOffsetFill) ?
            GL_POLYGON_OFFSET_FILL : capabilities[command.argument];
        if (v[0] != 0) {
            gl.Enable(capability);
        } else {
            gl.Disable(capability);
        }
        break;
    }
    case Op::DepthWrite:
        gl.DepthMask(v[0] != 0 ? GL_TRUE : GL_FALSE);
        break;
    case Op::DepthFunction:
        gl.DepthFunc(comparisons[command.argument]);
        break;
    case Op::AlphaFunction:
        gl.AlphaFunc(comparisons[command.argument], f(0));
        break;
    case Op::BlendFunction:
        gl.BlendFunc(blendFactors[command.argument], blendFactors[command.second]);
        break;
    case Op::CullFace:
        gl.CullFace(cullFaces[command.argument]);
        break;
    case Op::Scissor:
        gl.Scissor(static_cast<int>(v[0]), static_cast<int>(v[1]), static_cast<int>(v[2]), static_cast<int>(v[3]));
        break;
    case Op::DepthRange:
        gl.DepthRange(v[0], v[1]);
        break;
    case Op::Begin:
        gl.Begin(primitives[command.argument]);
        break;
    case Op::Vertex:
        gl.Vertex3f(f(0), f(1), f(2));
        break;
    case Op::End:
        gl.End();
        break;
    case Op::CallList:
        gl.CallList(command.argument);
        break;
    case Op::BindTexture:
        gl.BindTexture(textureTargets[command.argument], command.second);
        break;
    case Op::DeleteTextures:
        gl.DeleteTextures(static_cast<GLsizei>(command.names.size()), command.names.data());
        break;
    case Op::TextureImage: {
        const auto& u = command.upload;
        gl.TexImage2D(textureTargets[command.argument], u.level,
            static_cast<GLint>(internalFormats[static_cast<std::size_t>(u.internal)]), u.width, u.height, 0,
            pixelFormats[static_cast<std::size_t>(u.format)], GL_UNSIGNED_BYTE,
            u.allocateOnly ? nullptr : u.bytes.data());
        break;
    }
    case Op::TextureSubImage: {
        const auto& u = command.upload;
        gl.TexSubImage2D(textureTargets[command.argument], u.level, u.x, u.y, u.width, u.height,
            pixelFormats[static_cast<std::size_t>(u.format)], GL_UNSIGNED_BYTE, u.bytes.data());
        break;
    }
    case Op::PixelStore:
        gl.PixelStorei(pixelStores[command.argument], static_cast<GLint>(v[0]));
        break;
    case Op::TextureParameter: {
        auto value = f(0);
        if (command.second <= static_cast<std::uint32_t>(TextureParameter::MagFilter)) {
            value = static_cast<float>(filters[static_cast<std::size_t>(v[0])]);
        } else if (command.second <= static_cast<std::uint32_t>(TextureParameter::WrapT)) {
            value = static_cast<float>(wraps[static_cast<std::size_t>(v[0])]);
        }
        gl.TexParameterf(textureTargets[command.argument], textureParameters[command.second], value);
        break;
    }
    case Op::TextureEnvironment:
        if (command.argument == static_cast<std::uint32_t>(EnvironmentParameter::Mode)) {
            gl.TexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, static_cast<GLint>(environments[command.second]));
        } else if (command.argument == static_cast<std::uint32_t>(EnvironmentParameter::CombineRGB)) {
            gl.TexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
        } else {
            gl.TexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, static_cast<GLint>(command.second));
        }
        break;
    case Op::FogMode:
        gl.Fogi(GL_FOG_MODE, static_cast<GLint>(fogModes[command.argument]));
        break;
    case Op::FogHint:
    case Op::PerspectiveHint:
        gl.Hint(command.op == Op::FogHint ? GL_FOG_HINT : GL_PERSPECTIVE_CORRECTION_HINT, hints[command.argument]);
        break;
    case Op::FogColor: {
        const std::array<float, 4> color{{f(0), f(1), f(2), f(3)}};
        gl.Fogfv(GL_FOG_COLOR, color.data());
        break;
    }
    case Op::FogStart:
    case Op::FogEnd:
    case Op::FogDensity:
        gl.Fogf(command.op == Op::FogStart ? GL_FOG_START : command.op == Op::FogEnd ? GL_FOG_END : GL_FOG_DENSITY,
            f(0));
        break;
    case Op::FrontFace:
        gl.FrontFace(command.argument ? GL_CCW : GL_CW);
        break;
    case Op::PolygonMode:
        gl.PolygonMode(cullFaces[command.argument], polygonModes[command.second]);
        break;
    case Op::PolygonOffset:
        gl.PolygonOffset(f(0), f(1));
        break;
    case Op::DrawArrays:
        gl.EnableClientState(GL_VERTEX_ARRAY);
        gl.EnableClientState(GL_TEXTURE_COORD_ARRAY);
        gl.VertexPointer(4, GL_FLOAT, static_cast<GLsizei>(sizeof(Vertex)), command.vertices.data()->position.data());
        gl.TexCoordPointer(4, GL_FLOAT, static_cast<GLsizei>(sizeof(Vertex)),
            command.vertices.data()->texcoords[0].data());
        gl.DrawArrays(primitives[command.argument], 0, static_cast<GLsizei>(command.vertices.size()));
        gl.DisableClientState(GL_VERTEX_ARRAY);
        gl.DisableClientState(GL_TEXTURE_COORD_ARRAY);
        break;
    default:
        Check(false, "oracle adapter operation selected");
    }
}

void Matrices(GL& gl, Recorder& cpu, const char* stage, bool exact) {
    constexpr std::array<GLenum, 3> queries{{GL_MODELVIEW_MATRIX, GL_PROJECTION_MATRIX, GL_TEXTURE_MATRIX}};
    float maximum = 0;
    int different = 0;
    for (std::size_t mode = 0; mode < matrixModes.size(); ++mode) {
        const std::size_t units = mode == 2 ? kTextureUnits : 1;
        for (std::size_t unit = 0; unit < units; ++unit) {
            gl.ActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(unit));
            std::array<float, 16> actual{};
            gl.GetFloatv(queries[mode], actual.data());
            const auto& expected = cpu.State().Matrix(static_cast<MatrixMode>(mode), unit).values;
            for (std::size_t index = 0; index < actual.size(); ++index) {
                Check(std::isfinite(actual[index]) && std::isfinite(expected[index]),
                    "selected matrix oracle values finite");
                if (actual[index] != expected[index]) {
                    ++different;
                    const float delta = std::fabs(actual[index] - expected[index]);
                    maximum = std::max(maximum, delta);
                    std::printf("matrix_delta stage=%s mode=%zu unit=%zu index=%zu GL=%.9g CPU=%.9g abs=%.9g\n",
                        stage, mode, unit, index, actual[index], expected[index], delta);
                }
            }
        }
    }
    gl.ActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(cpu.State().ActiveTexture()));
    std::printf("matrix_summary stage=%s differing=%d max_abs=%.9g exact_required=%d\n",
        stage, different, maximum, exact ? 1 : 0);
    if (exact) {
        Check(different == 0, "known representable matrix trace exact GL comparison");
    } else {
        // Scoped only to this bounded trace, not a pixel or gameplay threshold.
        constexpr float selectedTraceBudget = 1.0f / 262144.0f;
        Check(maximum <= selectedTraceBudget, "selected matrix arithmetic trace budget");
    }
    Check(gl.GetError() == GL_NO_ERROR, "real matrix queries have no GL error");
}

void Current(GL& gl, Recorder& cpu) {
    std::array<float, 4> color{};
    gl.GetFloatv(GL_CURRENT_COLOR, color.data());
    Check(color == cpu.State().Current().color, "actual current color exact");
    for (std::size_t unit = 0; unit < kTextureUnits; ++unit) {
        gl.ActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(unit));
        std::array<float, 4> coordinates{};
        gl.GetFloatv(GL_CURRENT_TEXTURE_COORDS, coordinates.data());
        Check(coordinates == cpu.State().Current().texcoords[unit], "actual per-unit current coordinates exact");
    }
    gl.ActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(cpu.State().ActiveTexture()));
}

void Emit(GL& gl, Recorder& cpu, Primitive primitive) {
    Submit(gl, cpu, Make(Op::Begin, {}, static_cast<std::uint32_t>(primitive)));
    constexpr std::array<std::array<double, 3>, 4> points{{{{-1, -1, 0}}, {{1, -1, 0}}, {{-1, 1, 0}}, {{1, 1, 0}}}};
    for (std::size_t index = 0; index < points.size(); ++index) {
        if (diagnostic) {
            std::printf("submitted primitive=%u vertex=%zu xyz=%.3g,%.3g,%.3g rgba=%.3g,0.5,0.75,1\n",
                static_cast<unsigned>(primitive), index, points[index][0], points[index][1], points[index][2],
                static_cast<double>(index) / 4);
        }
        Submit(gl, cpu, Make(Op::Color, {static_cast<double>(index) / 4, 0.5, 0.75, 1}));
        Submit(gl, cpu, Make(Op::TexCoord, {static_cast<double>(index) / 4, 0.5}));
        Submit(gl, cpu, Make(Op::Vertex, {points[index][0], points[index][1], points[index][2]}));
    }
    Submit(gl, cpu, Make(Op::End));
}

void Feedback(GL& gl, Recorder& cpu, Primitive primitive, Shade shade, std::uint32_t list = 0) {
    cpu.ClearDraws();
    Submit(gl, cpu, Make(Op::ShadeModel, {}, static_cast<std::uint32_t>(shade)));
    constexpr std::size_t capacity = 4096;
    std::array<float, capacity> buffer{};
    gl.FeedbackBuffer(static_cast<GLsizei>(buffer.size()), GL_3D_COLOR_TEXTURE, buffer.data());
    gl.RenderMode(GL_FEEDBACK);
    if (list) {
        Submit(gl, cpu, Make(Op::CallList, {}, list));
    } else {
        Emit(gl, cpu, primitive);
    }
    const int count = gl.RenderMode(GL_RENDER);
    Check(count >= 0, "feedback capacity sufficient");
    std::vector<Vertex> vertices;
    Primitive topology{};
    auto feedbackDraw = cpu.Draws().back();
    // GL feedback reports per-vertex attributes before flat-color raster selection.
    feedbackDraw.shade = Shade::Smooth;
    Check(ExpandTopology(feedbackDraw, vertices, topology) == Status::Ok, "qualified topology expansion");
    const std::size_t perPrimitive = topology == Primitive::Lines ? 2 : 3;
    std::size_t cursor = 0;
    std::size_t output = 0;
    float maximum = 0;
    while (cursor < static_cast<std::size_t>(count)) {
        const auto token = static_cast<GLenum>(buffer[cursor++]);
        if (perPrimitive == 3) {
            Check(token == GL_POLYGON_TOKEN && buffer[cursor++] == 3, "actual triangle feedback token");
        } else {
            Check(token == GL_LINE_TOKEN || token == GL_LINE_RESET_TOKEN, "actual line feedback token");
        }
        std::vector<std::array<float, 11>> expectedPrimitive;
        for (std::size_t corner = 0; corner < perPrimitive; ++corner) {
            Check(output < vertices.size(), "feedback vertex count bounded");
            const auto& vertex = vertices[output++];
            const auto& draw = cpu.Draws().back();
            const auto clip = Transform(draw.projection, Transform(draw.modelView, vertex.position));
            const auto texcoord = Transform(draw.textureMatrices[0], vertex.texcoords[0]);
            std::array<float, 11> expected{};
            expected[0] = (clip[0] / clip[3] + 1) * kOracleHalfExtent;
            expected[1] = (clip[1] / clip[3] + 1) * kOracleHalfExtent;
            expected[2] = (clip[2] / clip[3] + 1) / 2;
            std::copy(vertex.color.begin(), vertex.color.end(), expected.begin() + 3);
            std::copy(texcoord.begin(), texcoord.end(), expected.begin() + 7);
            expectedPrimitive.push_back(expected);
        }
        constexpr std::size_t entriesPerVertex = 11;
        Check(cursor + perPrimitive * entriesPerVertex <= static_cast<std::size_t>(count),
            "feedback primitive read bounded");
        float best = 1.0e30f;
        std::size_t selectedShift = 0;
        // Feedback need not expose a triangle's submission starting vertex.
        // Compare oriented cyclic order; reversed winding is never accepted.
        const std::size_t shifts = perPrimitive == 3 ? perPrimitive : 1;
        for (std::size_t shift = 0; shift < shifts; ++shift) {
            float delta = 0;
            for (std::size_t corner = 0; corner < perPrimitive; ++corner) {
                const auto& expected = expectedPrimitive[(corner + shift) % perPrimitive];
                for (std::size_t component = 0; component < entriesPerVertex; ++component) {
                    const float actual = buffer[cursor + corner * entriesPerVertex + component];
                    Check(std::isfinite(actual) && std::isfinite(expected[component]),
                        "selected feedback values finite");
                    delta = std::max(delta, std::fabs(actual - expected[component]));
                }
            }
            if (delta < best) {
                best = delta;
                selectedShift = shift;
            }
        }
        std::printf("feedback_order primitive=%u shade=%u first_output=%zu cyclic_shift=%zu max_abs=%.9g\n",
            static_cast<unsigned>(primitive), static_cast<unsigned>(shade), output - perPrimitive, selectedShift, best);
        maximum = std::max(maximum, best);
        cursor += perPrimitive * entriesPerVertex;
    }
    std::printf("feedback primitive=%u shade=%u entries=%d vertices=%zu max_abs=%.9g\n",
        static_cast<unsigned>(primitive), static_cast<unsigned>(shade), count, output, maximum);
    Check(output == vertices.size() && maximum == 0, "known-byte feedback geometry/color/UV/order exact");
    Check(gl.GetError() == GL_NO_ERROR, "actual geometry feedback has no GL error");
}

void RasterTopology(GL& gl, Recorder& cpu, Primitive primitive, Shade shade) {
    cpu.ClearDraws();
    gl.ClearColor(0, 0, 0, 0);
    gl.Clear(GL_COLOR_BUFFER_BIT);
    Submit(gl, cpu, Make(Op::ShadeModel, {}, static_cast<std::uint32_t>(shade)));
    GLint actualShade = 0;
    gl.GetIntegerv(GL_SHADE_MODEL, &actualShade);
    GLint provoking = 0;
    gl.GetIntegerv(GL_PROVOKING_VERTEX, &provoking);
    const auto provokingError = gl.GetError();
    std::printf("raster_driver_state primitive=%u requested_shade=%u actual_shade=%x provoking=%x query_error=%x\n",
        static_cast<unsigned>(primitive), static_cast<unsigned>(shade), actualShade, provoking, provokingError);
    Check(actualShade == (shade == Shade::Flat ? GL_FLAT : GL_SMOOTH), "actual requested shade mode");
    Emit(gl, cpu, primitive);
    constexpr std::size_t width = kOracleSide;
    constexpr std::size_t channels = 3;
    std::array<unsigned char, width * width * channels> original{};
    gl.PixelStorei(GL_PACK_ALIGNMENT, 1);
    gl.ReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(width),
        GL_RGB, GL_UNSIGNED_BYTE, original.data());
    std::vector<Vertex> vertices;
    Primitive topology{};
    Check(ExpandTopology(cpu.Draws().back(), vertices, topology) == Status::Ok, "lowered raster topology");
    gl.Clear(GL_COLOR_BUFFER_BIT);
    gl.Begin(primitives[static_cast<std::size_t>(topology)]);
    for (const auto& vertex : vertices) {
        gl.Color4f(vertex.color[0], vertex.color[1], vertex.color[2], vertex.color[3]);
        gl.Vertex3f(vertex.position[0], vertex.position[1], vertex.position[2]);
    }
    gl.End();
    std::array<unsigned char, width * width * channels> lowered{};
    gl.ReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(width),
        GL_RGB, GL_UNSIGNED_BYTE, lowered.data());
    std::size_t different = 0;
    int maximum = 0;
    bool nonzero = false;
    for (std::size_t index = 0; index < original.size(); ++index) {
        nonzero = nonzero || original[index] != 0;
        if (original[index] != lowered[index]) {
            constexpr std::size_t maximumReportedDifferences = 12;
            if (diagnostic && different < maximumReportedDifferences) {
                std::printf("RGB_difference x=%zu y=%zu component=%zu original=%u lowered=%u\n",
                    (index / channels) % width, index / (width * channels), index % channels,
                    static_cast<unsigned>(original[index]), static_cast<unsigned>(lowered[index]));
            }
            ++different;
            maximum = std::max(maximum, std::abs(static_cast<int>(original[index]) - lowered[index]));
        }
    }
    std::printf("actual_GL_vs_GL_lowering primitive=%u shade=%u differing_bytes=%zu max_delta=%d nonzero=%d\n",
        static_cast<unsigned>(primitive), static_cast<unsigned>(shade), different, maximum, nonzero ? 1 : 0);
    if (different != 0 || !nonzero) {
        ++rasterFailures;
    }
    for (bool baseline : {true, false}) {
        char filename[96];
        std::snprintf(filename, sizeof(filename), "oracle-%u-%u-%s.rgb",
            static_cast<unsigned>(primitive),
            static_cast<unsigned>(shade), baseline ? "original" : "lowered");
        std::ofstream file(filename, std::ios::binary);
        const auto& bytes = baseline ? original : lowered;
        file.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        Check(file.good(), "save actual owned GL RGB receipt");
    }
    if (!diagnostic) {
        Check(nonzero && different == 0, "selected actual GL original/lowered raster bytes exact");
    }
    const auto& color = cpu.State().Current().color;
    gl.Color4f(color[0], color[1], color[2], color[3]);
    Check(gl.GetError() == GL_NO_ERROR, "actual lowering raster has no GL error");
}

void RasterStateQueries(GL& gl, Recorder& cpu) {
    for (std::uint32_t index = 0; index < 6; ++index) {
        Submit(gl, cpu, Make(Op::Enable, {1}, index));
        Check(gl.IsEnabled(capabilities[index]) == GL_TRUE && cpu.State().raster.enabled[index],
            "actual enable state exact");
        Submit(gl, cpu, Make(Op::Enable, {0}, index));
    }
    Submit(gl, cpu, Make(Op::ActiveTexture, {}, 2));
    Submit(gl, cpu, Make(Op::Enable, {1}, static_cast<std::uint32_t>(Capability::Texture2D)));
    Check(gl.IsEnabled(GL_TEXTURE_2D) == GL_TRUE && cpu.State().textures[2].enabled2D,
        "texture2D enable belongs to unit2");
    Submit(gl, cpu, Make(Op::ActiveTexture, {}, 0));
    Check(gl.IsEnabled(GL_TEXTURE_2D) == GL_FALSE && !cpu.State().textures[0].enabled2D,
        "unit0 enable independent");
    Submit(gl, cpu, Make(Op::ActiveTexture, {}, 2));
    Submit(gl, cpu, Make(Op::Enable, {0}, static_cast<std::uint32_t>(Capability::Texture2D)));
    Submit(gl, cpu, Make(Op::DepthFunction, {}, static_cast<std::uint32_t>(Compare::LessEqual)));
    Submit(gl, cpu, Make(Op::DepthWrite, {0}));
    Submit(gl, cpu, Make(Op::AlphaFunction, {0.5}, static_cast<std::uint32_t>(Compare::Greater)));
    auto blend = Make(Op::BlendFunction, {}, static_cast<std::uint32_t>(BlendFactor::SourceAlpha));
    blend.second = static_cast<std::uint32_t>(BlendFactor::InverseSourceAlpha);
    Submit(gl, cpu, blend);
    Submit(gl, cpu, Make(Op::CullFace, {}, static_cast<std::uint32_t>(Cull::Front)));
    Submit(gl, cpu, Make(Op::Scissor, {1, 2, 31, 32}));
    Submit(gl, cpu, Make(Op::DepthRange, {0.25, 0.75}));
    GLint value = 0;
    gl.GetIntegerv(GL_DEPTH_FUNC, &value);
    Check(value == static_cast<GLint>(comparisons[static_cast<std::size_t>(cpu.State().raster.depthFunction)]),
        "actual depth comparison exact");
    GLboolean boolean = GL_TRUE;
    gl.GetBooleanv(GL_DEPTH_WRITEMASK, &boolean);
    Check(boolean == GL_FALSE && !cpu.State().raster.depthWrite, "actual depth write exact");
    gl.GetIntegerv(GL_ALPHA_TEST_FUNC, &value);
    Check(value == GL_GREATER, "actual alpha comparison exact");
    GLfloat reference = 0;
    gl.GetFloatv(GL_ALPHA_TEST_REF, &reference);
    Check(reference == cpu.State().raster.alphaReference, "actual alpha reference exact");
    gl.GetIntegerv(GL_BLEND_SRC, &value);
    Check(value == GL_SRC_ALPHA, "actual source blend exact");
    gl.GetIntegerv(GL_BLEND_DST, &value);
    Check(value == GL_ONE_MINUS_SRC_ALPHA, "actual destination blend exact");
    gl.GetIntegerv(GL_CULL_FACE_MODE, &value);
    Check(value == GL_FRONT, "actual cull face exact");
    std::array<GLint, 4> rectangle{};
    gl.GetIntegerv(GL_SCISSOR_BOX, rectangle.data());
    Check(rectangle == cpu.State().raster.scissor, "actual scissor exact");
    std::array<GLdouble, 2> depthRange{};
    gl.GetDoublev(GL_DEPTH_RANGE, depthRange.data());
    Check(depthRange == cpu.State().raster.depthRange, "actual depth range exact");
    Submit(gl, cpu, Make(Op::DepthRange, {0, 1}));
    Check(gl.GetError() == GL_NO_ERROR, "raster state query profile no GL error");
    std::puts("raster_state_queries=exact");
}

void VguiSequence(GL& gl, Recorder& cpu) {
    Submit(gl, cpu, Make(Op::MatrixMode, {}, 0));
    const auto original = cpu.State().Matrix(MatrixMode::ModelView).values;
    Submit(gl, cpu, Make(Op::Push));
    Submit(gl, cpu, Make(Op::MatrixMode, {}, 1));
    Submit(gl, cpu, Make(Op::Identity));
    Submit(gl, cpu, Make(Op::Ortho, {0, 640, 480, 0, -1, 1}));
    const auto projection = cpu.State().Matrix(MatrixMode::Projection).values;
    Submit(gl, cpu, Make(Op::MatrixMode, {}, 0));
    Submit(gl, cpu, Make(Op::Push));
    Submit(gl, cpu, Make(Op::Identity));
    Submit(gl, cpu, Make(Op::Translate, {5, 7, 0}));
    Submit(gl, cpu, Make(Op::MatrixMode, {}, 1));
    Submit(gl, cpu, Make(Op::MatrixMode, {}, 0));
    Submit(gl, cpu, Make(Op::Pop));
    Submit(gl, cpu, Make(Op::Pop));
    Check(cpu.State().Matrix(MatrixMode::ModelView).values == original, "inherited modelview restored");
    Check(cpu.State().Matrix(MatrixMode::Projection).values == projection, "inherited projection rewritten");
    Matrices(gl, cpu, "inherited-VGUI-sequence", true);
}

void Run(GL& gl) {
    Recorder cpu;
    RasterStateQueries(gl, cpu);
    Submit(gl, cpu, Make(Op::Translate, {2, 3, 4}));
    Submit(gl, cpu, Make(Op::Scale, {2, 4, 8}));
    Submit(gl, cpu, Make(Op::Push));
    Submit(gl, cpu, Make(Op::Identity));
    Submit(gl, cpu, Make(Op::Pop));
    Matrices(gl, cpu, "postmultiply-stack", true);
    Submit(gl, cpu, Make(Op::ActiveTexture, {}, 2));
    Submit(gl, cpu, Make(Op::MatrixMode, {}, 2));
    Submit(gl, cpu, Make(Op::Scale, {2, 4, 1}));
    Submit(gl, cpu, Make(Op::TexCoord, {0.25, 0.5}, 2));
    Submit(gl, cpu, Make(Op::TexCoord, {0.75, 0.125}));
    Submit(gl, cpu, Make(Op::Color, {1.5, -0.25, 0.5, 1}));
    Current(gl, cpu);
    Matrices(gl, cpu, "unit2-detail-scale", true);
    Submit(gl, cpu, Make(Op::MatrixMode, {}, 1));
    Submit(gl, cpu, Make(Op::Ortho, {-4, 4, -4, 4, -1, 1}));
    Submit(gl, cpu, Make(Op::MatrixMode, {}, 0));
    Submit(gl, cpu, Make(Op::Identity));
    Submit(gl, cpu, Make(Op::Viewport, {0, 0, kOracleSide, kOracleSide}));
    for (auto primitive : {Primitive::Triangles, Primitive::TriangleStrip, Primitive::TriangleFan,
        Primitive::Lines, Primitive::LineLoop}) {
        Feedback(gl, cpu, primitive, Shade::Smooth);
        Feedback(gl, cpu, primitive, Shade::Flat);
        if (rasterEnabled) {
            RasterTopology(gl, cpu, primitive, Shade::Smooth);
            RasterTopology(gl, cpu, primitive, Shade::Flat);
        }
    }
    constexpr GLuint list = 19;
    Check(cpu.BeginList(list, false) == Status::Ok, "CPU compile list");
    gl.NewList(list, GL_COMPILE);
    Emit(gl, cpu, Primitive::Triangles);
    Check(cpu.EndList() == Status::Ok, "CPU commit list");
    gl.EndList();
    Submit(gl, cpu, Make(Op::Translate, {0.5, 0.25, 0}));
    Submit(gl, cpu, Make(Op::CallList, {}, list));
    Current(gl, cpu);
    Feedback(gl, cpu, Primitive::Triangles, Shade::Flat, list);
    Matrices(gl, cpu, "list-replay-later-matrix", true);
    Check(cpu.DeleteList(list) == Status::Ok, "CPU delete list");
    gl.DeleteLists(list, 1);
    constexpr GLuint arrayList = 23;
    Check(cpu.BeginList(arrayList, false) == Status::Ok, "owned array list compile");
    gl.NewList(arrayList, GL_COMPILE);
    {
        Command array = Make(Op::DrawArrays, {}, static_cast<std::uint32_t>(Primitive::Triangles));
        array.vertices.resize(3);
        array.vertices[0].position = {{-1, -1, 0, 1}};
        array.vertices[1].position = {{1, -1, 0, 1}};
        array.vertices[2].position = {{0, 1, 0, 1}};
        Submit(gl, cpu, array);
        array.vertices.assign(3, Vertex{});
    }
    Check(cpu.EndList() == Status::Ok, "owned array list commit");
    gl.EndList();
    Submit(gl, cpu, Make(Op::Color, {0.25, 0.5, 0.75, 1}));
    Feedback(gl, cpu, Primitive::Triangles, Shade::Smooth, arrayList);
    Check(cpu.DeleteList(arrayList) == Status::Ok, "delete array list");
    gl.DeleteLists(arrayList, 1);
    VguiSequence(gl, cpu);
    Submit(gl, cpu, Make(Op::Rotate, {37, 1, 2, 3}));
    Submit(gl, cpu, Make(Op::MatrixMode, {}, 1));
    Submit(gl, cpu, Make(Op::Identity));
    Submit(gl, cpu, Make(Op::Frustum, {-1.25, 2.75, -1.5, 2, 4, 4096}));
    Matrices(gl, cpu, "selected-rotation-frustum", false);
}

#include "texture_oracle.inc"

}

int main(int argc, char** argv) {
    Check(argc == 1 || argc == 2, "optional state/lowering/diagnostic mode");
    if (argc == 2) {
        const std::string mode = argv[1];
        Check(mode == "state" || mode == "lowering" || mode == "diagnostic" || mode == "textures"
            || mode == "texture-raster", "known oracle mode");
        diagnostic = mode == "diagnostic";
        rasterEnabled = mode != "state";
        textureMode = mode == "textures" || mode == "texture-raster";
        textureRasterOnly = mode == "texture-raster";
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::puts("stage=load-SDL");
#if defined(_WIN32)
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    constexpr DWORD maximumModulePathCharacters = 32768;
    std::array<char, maximumModulePathCharacters> modulePath{};
    const auto length = GetModuleFileNameA(GetModuleHandleA("SDL3.dll"), modulePath.data(),
        maximumModulePathCharacters);
    Check(length != 0 && length < maximumModulePathCharacters, "resolve actual loaded SDL3 module");
    std::printf("loaded_SDL3=%s\n", modulePath.data());
#endif
    Check(SDL_GetVersion() == SDL_VERSION, "pinned SDL runtime version");
    Check(SDL_Init(SDL_INIT_VIDEO), SDL_GetError());
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    auto* window = SDL_CreateWindow("CSE renderer state oracle", kOracleSide, kOracleSide,
        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    Check(window != nullptr, SDL_GetError());
    auto context = SDL_GL_CreateContext(window);
    Check(context != nullptr, SDL_GetError());
    std::puts("stage=created-real-GL-context");
    const auto getString = Load<decltype(&glGetString)>("glGetString");
    std::printf("context_vendor=%s context_renderer=%s context_version=%s\n",
        reinterpret_cast<const char*>(getString(GL_VENDOR)), reinterpret_cast<const char*>(getString(GL_RENDERER)),
        reinterpret_cast<const char*>(getString(GL_VERSION)));
    GL gl;
    std::puts("stage=loaded-profile-entrypoints");
    std::printf("SDL=%d GL_VENDOR=%s GL_RENDERER=%s GL_VERSION=%s\n", SDL_GetVersion(),
        reinterpret_cast<const char*>(gl.GetString(GL_VENDOR)),
        reinterpret_cast<const char*>(gl.GetString(GL_RENDERER)),
        reinterpret_cast<const char*>(gl.GetString(GL_VERSION)));
    GLint units = 0;
    gl.GetIntegerv(GL_MAX_TEXTURE_UNITS, &units);
    Check(units >= static_cast<GLint>(kTextureUnits), "real driver supports engine three-unit profile");
    if (textureMode) {
        if (textureRasterOnly) {
            RunTextureRasters(gl);
        } else {
            RunTextures(gl);
        }
    } else {
        Run(gl);
    }
    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    std::printf("actual_GL_raster_failures=%d\n", rasterFailures);
    Check(rasterFailures == 0, "actual original/lowered raster mismatch remains a failed qualification");
    std::puts("PASS actual OpenGL state/feedback oracle; no pixel/backend parity claim");
    return 0;
}
