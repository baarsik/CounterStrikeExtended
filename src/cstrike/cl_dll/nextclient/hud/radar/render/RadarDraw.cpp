#include "RadarDraw.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include "main.h"

#include "hud/radar/integration/Performance.h"

#include <ncl_gl/loader.h>
#include <ncl_gl/program.h>
#include <glad/glad.h>
#include <ncl_math/vec3.h>
#include <ncl_math/aabb.h>
#include "RadarShaders.h"
#include "NativeSdk.h"

#include "hud/radar/render/RadarDrawInternal.h"

#include "hud/radar/render/RadarPolyline.h"

namespace {

constexpr float kColorByteScale = 255.0f;
constexpr float kPrimitivePaddingPixels = 2.0f;
constexpr float kDiscFeatherPixels = 1.2f;
constexpr float kShapeFeatherPixels = 1.1f;

constexpr const char* kMaskUniforms[RadarDrawUniform::kMaskCount] = {
    "uMode",          "uCenter",  "uRadius",    "uFeather",   "uGamma",     "uSquare",
    "uPremultiplied", "uAlphaAa", "uRingInner", "uRingColor", "uFadeColor", "uFadeAmt",
};

enum {
    kPrimClipCenter,
    kPrimClipRadius,
    kPrimClipSquare,
    kPrimFadeColor,
    kPrimFadeAmt,
    kPrimCount,
};
constexpr const char* kPrimUniforms[kPrimCount] = {
    "uClipCenter", "uClipRadius", "uClipSquare", "uFadeColor", "uFadeAmt",
};

constexpr const char* kPrimAttribs[] = {
    "aPos", "aShape", "aCenter", "aColor", "aTri", "aTri2",
};

constexpr const char* kBlurUniforms[RadarDrawUniform::kBlurCount] = {
    "uCenter",     "uRadius", "uFeather", "uSquare",    "uDirection",
    "uFilterPass", "uDarken", "uAlpha",   "uFadeColor", "uFadeAmt",
};

static_assert(RadarDrawUniform::kMaskCount <= ncl_gl::kMaxUniforms && kPrimCount <= ncl_gl::kMaxUniforms &&
              RadarDrawUniform::kBlurCount <= ncl_gl::kMaxUniforms);

std::string LoadShaderSource(const char* name) {
    return NativeRadar_Shader(name);
}

bool EnsureGlad() {
    return ncl_gl::EnsureLoaded() && GLAD_GL_VERSION_2_0 != 0;
}

// Must match the vShape.x dispatch in radar_prim.frag.
constexpr float kShapeDisc = 0.0f;
constexpr float kShapeCone = 1.0f;
constexpr float kShapeTriangle = 2.0f;
constexpr float kShapeCross = 3.0f;
constexpr float kShapeRing = 4.0f;
constexpr float kShapeSolid = 5.0f;
constexpr float kShapeGlyph = 6.0f;
constexpr float kShapeQuestionMark = 7.0f;
constexpr float kShapeBomb = 8.0f;
constexpr float kShapeDisconnectedSignal = 9.0f;
constexpr float kShapePlayerMarkerFade = 10.0f;

} // namespace

bool RadarDrawContext::are_prims_ready() const {
    return NativeRadar_Renderer() ? (NativeRadar_RendererState().capabilities & CSE_RADAR_CAP_PRIMITIVES) != 0 : prim_.ok;
}

bool RadarDrawContext::CheckPrimsAvailable() {
    if (NativeRadar_Renderer()) {
        return are_prims_ready();
    }
    EnsurePrograms();

    return prim_.ok;
}

void RadarDrawContext::ResetGl(bool release) {
    ResetEffectsGl(release);

    if (release && ncl_gl::IsLoaded()) {
        if (mask_.prog) {
            glDeleteProgram(mask_.prog);
        }

        if (prim_.prog) {
            glDeleteProgram(prim_.prog);
        }

        if (blur_.prog) {
            glDeleteProgram(blur_.prog);
        }
    }

    owning_context_ = nullptr;
    owning_window_ = nullptr;
    prim_region_ = 0;
    clip_center_ = {};
    clip_r_ = kUnboundedClipRadius;
    clip_square_ = false;
    fade_color_ = {};
    fade_amt_ = 0.0f;
    program_done_ = false;
    mask_ = ncl_gl::Program{};
    prim_ = ncl_gl::Program{};
    blur_ = ncl_gl::Program{};
    current_program_ = 0;
    batch_ = PrimBatch{};
    blend_set_ = false;
}

void RadarDrawContext::ResetStats() {
    stats_ = RadarDrawStats{};
}

RadarDrawStats RadarDrawContext::GetStats() {
    return stats_;
}

void RadarDrawContext::SetPrimClip(ncl_math::Vector2 center, float radius, bool square) {
    clip_center_ = center;
    clip_r_ = radius;
    clip_square_ = square;
}

void RadarDrawContext::ClearPrimClip() {
    clip_center_ = {};
    clip_r_ = kUnboundedClipRadius;
    clip_square_ = false;
}

void RadarDrawContext::BeginPrims() {
    prim_region_++;
}

void RadarDrawContext::EndPrims() {
    if (prim_region_ > 0 && --prim_region_ == 0) {
        UseProgram(0);
    }
}

void RadarDrawContext::FlushPrims() {
    if (batch_.open) {
        FlushBatch();
        UseProgram(0);
    }
}

void RadarDrawContext::SetFade(ncl_math::Color color) {
    // The fade applies per batch at flush time; queued primitives must be committed first
    // or the change recolors them retroactively.
    FlushPrims();
    fade_color_ = ncl_math::ColorF{color.r / kColorByteScale, color.g / kColorByteScale, color.b / kColorByteScale};
    fade_amt_ = std::clamp(color.a / kColorByteScale, 0.0f, 1.0f);
}

void RadarDrawContext::ClearFade() {
    FlushPrims();
    fade_amt_ = 0.0f;
}

void RadarDrawContext::EndFrame() {
    ClearFade();
    ClearPrimClip();
    prim_region_ = 0;
    UseProgram(0);
}

ncl_math::Color RadarDrawContext::ApplyFade(ncl_math::Color color) {
    if (fade_amt_ <= 0.0f) {
        return color;
    }

    // Lerp blends all four channels, so the source alpha is put back explicitly.
    return ncl_math::Lerp(color, fade_color_.ToBytes().WithAlpha(color.a), fade_amt_);
}

void RadarDrawContext::DrawPolyline(std::span<const ncl_math::Vector2> points, std::span<const uint8_t> alpha,
                                    ncl_math::Color color, float width) {
    int count = (int)points.size();

    if (count < 2 || !CheckPrimsAvailable()) {
        return;
    }

    ncl_math::ColorF cf = color.ToFloat();

    PrimVertex v{};
    v.shape[0] = kShapeSolid;
    v.color[0] = cf.r;
    v.color[1] = cf.g;
    v.color[2] = cf.b;

    RadarPolyline_ForEachQuad(points, width, [&](size_t i, const std::array<ncl_math::Vector2, 4>& positions) {
        PrimVertex corners[4] = {v, v, v, v};
        for (size_t corner = 0; corner < positions.size(); corner++) {
            corners[corner].pos[0] = positions[corner].x;
            corners[corner].pos[1] = positions[corner].y;
        }
        corners[0].color[3] = alpha[i] / kColorByteScale;
        corners[1].color[3] = alpha[i] / kColorByteScale;
        corners[2].color[3] = alpha[i + 1] / kColorByteScale;
        corners[3].color[3] = alpha[i + 1] / kColorByteScale;
        AddQuad(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, corners);
    });
}

void RadarDrawContext::DrawDisc(ncl_math::Vector2 center, float radius, ncl_math::Color color) {
    float e = radius + kPrimitivePaddingPixels;
    AddShapeQuad({.shape = kShapeDisc,
                  .feather = kDiscFeatherPixels,
                  .center = center,
                  .radius = radius,
                  .color = color,
                  .alpha = color.a / kColorByteScale},
                 GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, {{center.x - e, center.y - e}, {center.x + e, center.y + e}});
}

void RadarDrawContext::DrawPlayerMarkerFade(const RadarDrawPlayerMarker& current, const RadarDrawPlayerMarker& previous,
                                            float radius, ncl_math::Color color) {
    constexpr float kMarkerExtentScale = 2.0f;
    constexpr float kMarkerFeatherPixels = 1.2f;
    // Must match the offscreen flag decoding in radar_prim.frag.
    constexpr float kCurrentOffscreenFlag = 1.0f;
    constexpr float kPreviousOffscreenFlag = 2.0f;
    ncl_math::Vector2 current_dir{};
    ncl_math::Vector2 previous_dir{};
    ncl_math::TryNormalize(current.direction, &current_dir);
    ncl_math::TryNormalize(previous.direction, &previous_dir);
    float flags = (current.offscreen ? kCurrentOffscreenFlag : 0.0f) +
                  (previous.offscreen ? kPreviousOffscreenFlag : 0.0f);
    ncl_math::Aabb2 bounds = ncl_math::Aabb2::Empty();
    bounds.Include(current.pos);
    bounds.Include(previous.pos);
    bounds = bounds.Expanded(radius * kMarkerExtentScale + kPrimitivePaddingPixels);
    AddShapeQuad(
        {
            .shape = kShapePlayerMarkerFade,
            .feather = kMarkerFeatherPixels,
            .center = current.pos,
            .radius = radius,
            .len = previous.opacity,
            .dir = previous.pos,
            .spread = current.weight,
            .color = color,
            .alpha = current.opacity,
            .t0 = current_dir,
            .t1 = previous_dir,
            .t2 = {flags, previous.weight},
        },
        GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, bounds);
}

void RadarDrawContext::DrawTriangle(ncl_math::Vector2 p0, ncl_math::Vector2 p1, ncl_math::Vector2 p2,
                                    ncl_math::Color color) {
    ncl_math::Aabb2 bb = ncl_math::Aabb2::Empty();
    bb.Include(p0);
    bb.Include(p1);
    bb.Include(p2);
    bb = bb.Expanded(kPrimitivePaddingPixels);
    AddShapeQuad({.shape = kShapeTriangle,
                  .feather = kShapeFeatherPixels,
                  .color = color,
                  .alpha = color.a / kColorByteScale,
                  .t0 = p0,
                  .t1 = p1,
                  .t2 = p2},
                 GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, bb);
}

void RadarDrawContext::DrawRect(ncl_math::Vector2 min, ncl_math::Vector2 max, ncl_math::Color color) {
    ncl_math::ColorF cf = color.ToFloat();

    PrimVertex v{};
    v.shape[0] = kShapeSolid;
    v.color[0] = cf.r;
    v.color[1] = cf.g;
    v.color[2] = cf.b;
    v.color[3] = color.a / kColorByteScale;

    PrimVertex corners[4] = {v, v, v, v};
    const ncl_math::Vector2 pos[4] = {{min.x, min.y}, {max.x, min.y}, {max.x, max.y}, {min.x, max.y}};

    for (int c = 0; c < 4; c++) {
        corners[c].pos[0] = pos[c].x;
        corners[c].pos[1] = pos[c].y;
    }

    AddQuad(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, corners);
}

void RadarDrawContext::DrawGlyph(unsigned int atlas, const ncl_math::Aabb2& rect, const ncl_math::Aabb2& uv,
    ncl_math::Color fill) {
    DrawGlyphImpl(atlas, {}, rect, uv, fill);
}

void RadarDrawContext::DrawGlyph(CseRadarTexture atlas, const ncl_math::Aabb2& rect, const ncl_math::Aabb2& uv,
    ncl_math::Color fill) {
    DrawGlyphImpl(0, atlas, rect, uv, fill);
}

void RadarDrawContext::DrawGlyphImpl(unsigned int atlas, CseRadarTexture service_atlas, const ncl_math::Aabb2& rect,
    const ncl_math::Aabb2& uv, ncl_math::Color fill) {
    PrimVertex v{};
    v.shape[0] = kShapeGlyph;
    v.color[0] = fill.r / kColorByteScale;
    v.color[1] = fill.g / kColorByteScale;
    v.color[2] = fill.b / kColorByteScale;
    v.color[3] = fill.a / kColorByteScale;

    PrimVertex corners[4] = {v, v, v, v};
    const ncl_math::Vector2 pos[4] = {
        {rect.min.x, rect.min.y}, {rect.max.x, rect.min.y}, {rect.max.x, rect.max.y}, {rect.min.x, rect.max.y}};
    const ncl_math::Vector2 tex[4] = {
        {uv.min.x, uv.min.y}, {uv.max.x, uv.min.y}, {uv.max.x, uv.max.y}, {uv.min.x, uv.max.y}};

    for (int c = 0; c < 4; c++) {
        corners[c].pos[0] = pos[c].x;
        corners[c].pos[1] = pos[c].y;
        corners[c].tri[0] = tex[c].x;
        corners[c].tri[1] = tex[c].y;
    }

    AddQuad(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, corners, static_cast<GLuint>(atlas), service_atlas);
}

void RadarDrawContext::DrawBomb(ncl_math::Vector2 center, float half_extent, ncl_math::Color color) {
    float margin = half_extent + kPrimitivePaddingPixels;
    AddShapeQuad({.shape = kShapeBomb,
                  .feather = kShapeFeatherPixels,
                  .center = center,
                  .radius = half_extent,
                  .color = color,
                  .alpha = color.a / kColorByteScale},
                 GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                 {{center.x - margin, center.y - margin}, {center.x + margin, center.y + margin}});
}

void RadarDrawContext::DrawDisconnectedSignal(ncl_math::Vector2 center, float half_extent, float half_width,
                                              ncl_math::Color color) {
    constexpr float kSignalFeatherPixels = 0.75f;
    float margin = half_extent + half_width + kPrimitivePaddingPixels;
    AddShapeQuad({.shape = kShapeDisconnectedSignal,
                  .feather = kSignalFeatherPixels,
                  .center = center,
                  .radius = half_extent,
                  .len = half_width,
                  .color = color,
                  .alpha = color.a / kColorByteScale},
                 GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                 {{center.x - margin, center.y - margin}, {center.x + margin, center.y + margin}});
}

void RadarDrawContext::DrawCross(ncl_math::Vector2 center, float half_extent, float half_width, ncl_math::Color color) {
    float m = half_extent + half_width + kPrimitivePaddingPixels;
    AddShapeQuad({.shape = kShapeCross,
                  .feather = kShapeFeatherPixels,
                  .center = center,
                  .radius = half_extent,
                  .len = half_width,
                  .color = color,
                  .alpha = color.a / kColorByteScale},
                 GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, {{center.x - m, center.y - m}, {center.x + m, center.y + m}});
}

void RadarDrawContext::DrawQuestionMark(ncl_math::Vector2 center, float half_extent, float half_width,
                                        ncl_math::Color color) {
    float m = half_extent + half_width + kPrimitivePaddingPixels;
    AddShapeQuad({.shape = kShapeQuestionMark,
                  .feather = kShapeFeatherPixels,
                  .center = center,
                  .radius = half_extent,
                  .len = half_width,
                  .color = color,
                  .alpha = color.a / kColorByteScale},
                 GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, {{center.x - m, center.y - m}, {center.x + m, center.y + m}});
}

void RadarDrawContext::DrawRing(ncl_math::Vector2 center, float outer, float inner, ncl_math::Color color) {
    float m = outer + kPrimitivePaddingPixels;
    AddShapeQuad({.shape = kShapeRing,
                  .feather = kShapeFeatherPixels,
                  .center = center,
                  .radius = outer,
                  .len = inner,
                  .color = color,
                  .alpha = color.a / kColorByteScale},
                 GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, {{center.x - m, center.y - m}, {center.x + m, center.y + m}});
}

void RadarDrawContext::DrawBorderRing(ncl_math::Vector2 center, float outer, float inner, float feather,
                                      ncl_math::Color color) {
    if (const auto* service = NativeRadar_Renderer()) {
        FlushPrims();
        CseRadarMaskRingDraw draw{};
        draw.size = sizeof(draw);
        draw.rendererEpoch = NativeRadar_RendererState().rendererEpoch;
        draw.hud = NativeRadar_HudSpace();
        draw.outer = {{center.x, center.y}, outer, feather, CSE_RADAR_SHAPE_CIRCLE, 0};
        draw.innerRadius = inner;
        const auto rgba = color.ToFloat();
        draw.color[0] = rgba.r;
        draw.color[1] = rgba.g;
        draw.color[2] = rgba.b;
        draw.color[3] = rgba.a;
        draw.fade = {{fade_color_.r, fade_color_.g, fade_color_.b}, fade_amt_};
        NativeRadar_ReportRenderResult("ring", service->DrawMaskRing(&draw));
        return;
    }

    // sin(pi / 4), retaining the upstream precision for the eighth-circle rasterizer.
    constexpr float kOctantRadiusScale = 0.70710678f;
    constexpr float kPixelRoundingBias = 0.5f;
    if (BeginMaskRing(center, outer, inner, feather, color.ToFloat(), false)) {
        SetBlend(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        float m = outer + kPrimitivePaddingPixels;
        glBegin(GL_QUADS);
        glVertex2f(center.x - m, center.y - m);
        glVertex2f(center.x + m, center.y - m);
        glVertex2f(center.x + m, center.y + m);
        glVertex2f(center.x - m, center.y + m);
        glEnd();
        EndMask();
        return;
    }

    if (!EnsureGlad()) {
        return;
    }

    // No mask program: rasterize with 1px quads (eighth-circle symmetry), FillRGBABlend-style.
    UseProgram(0);
    glDisable(GL_TEXTURE_2D);
    SetBlend(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4ub(color.r, color.g, color.b, color.a);
    glBegin(GL_QUADS);

    auto px = [](int x, int y) {
        glVertex2f((float)x, (float)y);
        glVertex2f((float)(x + 1), (float)y);
        glVertex2f((float)(x + 1), (float)(y + 1));
        glVertex2f((float)x, (float)(y + 1));
    };

    int icx = (int)center.x;
    int icy = (int)center.y;
    int radius = (int)outer;
    int thickness = (int)(outer - inner);

    for (int t = 0; t < thickness; t++) {
        int rr = radius - t;

        if (rr <= 0) {
            break;
        }

        int count = (int)(kOctantRadiusScale * rr + kPixelRoundingBias);

        for (int i = 0; i <= count; i++) {
            int s = (int)(std::sqrt((float)(rr * rr - i * i)) + kPixelRoundingBias);
            px(icx + i, icy + s);
            px(icx + i, icy - s);
            px(icx - i, icy + s);
            px(icx - i, icy - s);
            px(icx + s, icy + i);
            px(icx + s, icy - i);
            px(icx - s, icy + i);
            px(icx - s, icy - i);
        }
    }

    glEnd();
    glEnable(GL_TEXTURE_2D);
    glColor4ub(255, 255, 255, 255);
}

void RadarDrawContext::DrawCone(ncl_math::Vector2 apex, ncl_math::Vector2 dir, float length, float spread,
                                ncl_math::Color color, float peak_alpha) {
    constexpr float kConeFeatherPixels = 1.5f;
    ncl_math::Vector2 perp = dir.Perp();
    float hw = length * tanf(spread);
    ncl_math::Vector2 tip = apex + dir * length;
    ncl_math::Aabb2 bb = ncl_math::Aabb2::Empty();
    bb.Include(apex);
    bb.Include(tip + perp * hw);
    bb.Include(tip - perp * hw);
    bb = bb.Expanded(kPrimitivePaddingPixels);
    AddShapeQuad({.shape = kShapeCone,
                  .feather = kConeFeatherPixels,
                  .center = apex,
                  .len = length,
                  .dir = dir,
                  .spread = spread,
                  .color = color,
                  .alpha = peak_alpha},
                 GL_SRC_ALPHA, GL_ONE, bb);
}

void RadarDrawContext::EnsurePrograms() {
    if (owning_context_ && !program_done_) {
        Init();
    }
}

ncl_gl::Program& RadarDrawContext::GetMaskProgram() {
    return mask_;
}

ncl_gl::Program& RadarDrawContext::GetBlurProgram() {
    return blur_;
}

void RadarDrawContext::DrawLayerIndicator(ncl_math::Vector2 screen, int vertical, int size, ncl_math::Color color) {
    constexpr float kIndicatorHalfSpanScale = 0.6f;
    constexpr float kIndicatorWidthScale = 0.3f;
    constexpr float kIndicatorMinimumWidthPixels = 1.0f;
    constexpr float kIndicatorHeightScale = 0.5f;
    if (vertical == 0) {
        return;
    }

    float half = size * kIndicatorHalfSpanScale;
    float width = std::max(kIndicatorMinimumWidthPixels, size * kIndicatorWidthScale);
    ncl_math::Vector2 points[] = {
        screen + ncl_math::Vector2{-half, vertical * half * kIndicatorHeightScale},
        screen + ncl_math::Vector2{0.0f, -vertical * half * kIndicatorHeightScale},
        screen + ncl_math::Vector2{half, vertical * half * kIndicatorHeightScale},
    };
    uint8_t alpha[] = {color.a, color.a, color.a};
    DrawPolyline(points, alpha, color, width);
}

void RadarDrawContext::Init() {
    program_done_ = true;

    if (!EnsureGlad()) {
        return;
    }

    std::string vs_src = LoadShaderSource("radar.vert");
    std::string prim_vs_src = LoadShaderSource("radar_prim.vert");
    std::string mask_src = LoadShaderSource("radar_mask.frag");
    std::string prim_src = LoadShaderSource("radar_prim.frag");
    std::string backdrop_src = LoadShaderSource("radar_backdrop.frag");

    ncl_gl::ProgramBuildCallbacks callbacks = performance_.ProgramCallbacks();
    std::string error_log;
    ncl_gl::LoadProgram(mask_, vs_src.c_str(), mask_src.c_str(), kMaskUniforms, RadarDrawUniform::kMaskCount, nullptr,
                        0, &error_log, &callbacks);

    if (!mask_.ok) {
        gEngfuncs.Con_DPrintf("radar mask shader failed: %s\n", error_log.c_str());
    }

    if (mask_.ok) {
        glUseProgram(mask_.prog);
        glUniform1i(glGetUniformLocation(mask_.prog, "uTex"), 0);
        glUseProgram(0);
    }

    error_log.clear();
    ncl_gl::LoadProgram(prim_, prim_vs_src.c_str(), prim_src.c_str(), kPrimUniforms, kPrimCount, kPrimAttribs,
                        kAttrCount, &error_log, &callbacks);

    if (!prim_.ok) {
        gEngfuncs.Con_DPrintf("radar prim shader failed: %s\n", error_log.c_str());
    }

    if (prim_.ok) {
        for (int i = 0; i < kAttrCount; i++) {
            prim_attr_[i] = glGetAttribLocation(prim_.prog, kPrimAttribs[i]);
        }

        glUseProgram(prim_.prog);
        glUniform1i(glGetUniformLocation(prim_.prog, "uAtlas"), 0);
        glUseProgram(0);
    }

    error_log.clear();
    ncl_gl::LoadProgram(blur_, vs_src.c_str(), backdrop_src.c_str(), kBlurUniforms, RadarDrawUniform::kBlurCount,
                        nullptr, 0, &error_log, &callbacks);

    if (!blur_.ok) {
        gEngfuncs.Con_DPrintf("radar backdrop shader failed: %s\n", error_log.c_str());
    }

    if (blur_.ok) {
        glUseProgram(blur_.prog);
        glUniform1i(glGetUniformLocation(blur_.prog, "uTex"), 0);
        glUseProgram(0);

        InitEffectsGl(blur_);
    }
}

// UseProgram(0) invalidates this cache before returning control to the engine.
void RadarDrawContext::SetBlend(GLenum src, GLenum dst) {
    if (blend_set_ && blend_src_ == src && blend_dst_ == dst) {
        return;
    }

    glEnable(GL_BLEND);
    glBlendFunc(src, dst);
    blend_src_ = src;
    blend_dst_ = dst;
    blend_set_ = true;
}

// Bind directly here: UseProgram flushes the batch before switching programs.
void RadarDrawContext::FlushBatch() {
    if (batch_.verts.empty()) {
        batch_.open = false;
        return;
    }

    if (const auto* service = NativeRadar_Renderer()) {
        static_assert(sizeof(PrimVertex) == sizeof(CseRadarPrimitiveVertex));
        std::vector<CseRadarPrimitiveVertex> vertices(batch_.verts.size());
        std::memcpy(vertices.data(), batch_.verts.data(), vertices.size() * sizeof(vertices[0]));
        CseRadarPrimitiveBatch draw{};
        draw.size = sizeof(draw);
        draw.rendererEpoch = NativeRadar_RendererState().rendererEpoch;
        draw.hud = NativeRadar_HudSpace();
        draw.blend = batch_.dst == GL_ONE ? CSE_RADAR_BLEND_ADDITIVE_ALPHA : CSE_RADAR_BLEND_STRAIGHT;
        draw.clip = {{batch_.clip_center.x, batch_.clip_center.y}, batch_.clip_radius,
            static_cast<std::uint32_t>(batch_.clip_radius == kUnboundedClipRadius ? CSE_RADAR_SHAPE_NONE :
                (batch_.clip_square ? CSE_RADAR_SHAPE_SQUARE : CSE_RADAR_SHAPE_CIRCLE))};
        draw.fade = {{fade_color_.r, fade_color_.g, fade_color_.b}, fade_amt_};
        draw.atlas = batch_.service_atlas;
        draw.quadCount = static_cast<std::uint32_t>(vertices.size() / 4);
        draw.vertices = vertices.data();
        NativeRadar_ReportRenderResult("primitives", service->DrawPrimitiveQuads(&draw));
        ++stats_.batches;
        stats_.quads += static_cast<int>(draw.quadCount);
        batch_.verts.clear();
        batch_.open = false;
        batch_.service_atlas = {};
        return;
    }

    if (current_program_ != prim_.prog) {
        glUseProgram(prim_.prog);
        current_program_ = prim_.prog;
    }

    SetBlend(batch_.src, batch_.dst);
    glUniform2f(prim_.loc[kPrimClipCenter], batch_.clip_center.x, batch_.clip_center.y);
    glUniform1f(prim_.loc[kPrimClipRadius], batch_.clip_radius);
    glUniform1f(prim_.loc[kPrimClipSquare], batch_.clip_square);
    glUniform3f(prim_.loc[kPrimFadeColor], fade_color_.r, fade_color_.g, fade_color_.b);
    glUniform1f(prim_.loc[kPrimFadeAmt], fade_amt_);

    // Restore rather than unbind afterwards: hw.dll caches the bound texture name and
    // early-outs on a hit, so leaving a foreign texture bound desyncs that cache.
    GLint prev_tex = 0;
    GLint prev_unit = GL_TEXTURE0;

    if (batch_.tex != 0) {
        // The engine's multitexture path can leave unit 1 current; the sampler is on unit 0.
        glGetIntegerv(GL_ACTIVE_TEXTURE, &prev_unit);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);
        glBindTexture(GL_TEXTURE_2D, batch_.tex);
    }

    // Client-side vertex arrays, so make sure no engine VBO hijacks the pointers.
    GLint prev_vbo = 0;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prev_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    const char* base = reinterpret_cast<const char*>(batch_.verts.data());
    GLsizei stride = sizeof(PrimVertex);
    glVertexAttribPointer(prim_attr_[kAttrPos], 2, GL_FLOAT, GL_FALSE, stride, base + offsetof(PrimVertex, pos));
    glVertexAttribPointer(prim_attr_[kAttrShape], 4, GL_FLOAT, GL_FALSE, stride, base + offsetof(PrimVertex, shape));
    glVertexAttribPointer(prim_attr_[kAttrCenter], 4, GL_FLOAT, GL_FALSE, stride, base + offsetof(PrimVertex, center));
    glVertexAttribPointer(prim_attr_[kAttrColor], 4, GL_FLOAT, GL_FALSE, stride, base + offsetof(PrimVertex, color));
    glVertexAttribPointer(prim_attr_[kAttrTri], 4, GL_FLOAT, GL_FALSE, stride, base + offsetof(PrimVertex, tri));
    glVertexAttribPointer(prim_attr_[kAttrTri2], 4, GL_FLOAT, GL_FALSE, stride, base + offsetof(PrimVertex, tri2));

    for (int i = 0; i < kAttrCount; i++) {
        glEnableVertexAttribArray(prim_attr_[i]);
    }

    // The engine's alpha test must not discard shader coverage or marker crossfades.
    GLboolean alpha_test = glIsEnabled(GL_ALPHA_TEST);
    if (alpha_test) {
        glDisable(GL_ALPHA_TEST);
    }

    performance::PerformanceMonitorInterface* monitor = performance_.get_monitor();
    {
        performance::PerformanceCpuScope cpu(monitor, performance::PerfStage::RadarBatch);
        performance::PerformanceGpuScope gpu(monitor, performance::PerfStage::RadarBatch);
        glDrawArrays(GL_QUADS, 0, static_cast<GLsizei>(batch_.verts.size()));
    }

    if (monitor) {
        monitor->AddCounter(performance::PerfCounterId::RadarBatches, 1);
        monitor->AddCounter(performance::PerfCounterId::RadarQuads, batch_.verts.size() / 4);
    }

    if (alpha_test) {
        glEnable(GL_ALPHA_TEST);
    }

    stats_.batches++;
    stats_.quads += (int)batch_.verts.size() / 4;

    for (int i = 0; i < kAttrCount; i++) {
        glDisableVertexAttribArray(prim_attr_[i]);
    }

    glBindBuffer(GL_ARRAY_BUFFER, prev_vbo);

    if (batch_.tex != 0) {
        glBindTexture(GL_TEXTURE_2D, (GLuint)prev_tex);
        glActiveTexture((GLenum)prev_unit);
    }

    batch_.verts.clear();
    batch_.open = false;
    batch_.tex = 0;
}

void RadarDrawContext::UseProgram(GLuint p) {
    if (NativeRadar_Renderer()) {
        if (batch_.open) {
            FlushBatch();
        }
        return;
    }
    // Hand-off must draw the queued primitives first, so they land in call order.
    if (p != prim_.prog && batch_.open) {
        FlushBatch();
    }

    if (p != current_program_) {
        glUseProgram(p);
        current_program_ = p;
    }

    if (p == 0) {
        blend_set_ = false;
    }
}

void RadarDrawContext::ApplyFade(GLint color_loc, GLint amt_loc) {
    glUniform3f(color_loc, fade_color_.r, fade_color_.g, fade_color_.b);
    glUniform1f(amt_loc, fade_amt_);
}

void RadarDrawContext::AddQuad(GLenum blend_src, GLenum blend_dst, const PrimVertex (&corners)[4], GLuint tex, CseRadarTexture atlas) {
    float clip_sq = clip_square_ ? 1.0f : 0.0f;

    if (batch_.open && (batch_.src != blend_src || batch_.dst != blend_dst || batch_.clip_center != clip_center_ ||
                        batch_.clip_radius != clip_r_ || batch_.clip_square != clip_sq ||
                        (tex != 0 && batch_.tex != 0 && batch_.tex != tex) ||
                        (atlas.serial && batch_.service_atlas.serial && (atlas.serial != batch_.service_atlas.serial ||
                            atlas.rendererEpoch != batch_.service_atlas.rendererEpoch)))) {
        FlushBatch();
    }

    batch_.open = true;
    batch_.src = blend_src;
    batch_.dst = blend_dst;
    batch_.clip_center = clip_center_;
    batch_.clip_radius = clip_r_;
    batch_.clip_square = clip_sq;

    if (atlas.serial) {
        batch_.service_atlas = atlas;
    }
    if (tex != 0) {
        batch_.tex = tex;
    }

    for (int c = 0; c < 4; c++) {
        batch_.verts.push_back(corners[c]);
    }
}

void RadarDrawContext::AddShapeQuad(const ShapeParams& p, GLenum blend_src, GLenum blend_dst, ncl_math::Aabb2 quad) {
    if (!CheckPrimsAvailable()) {
        return;
    }

    PrimVertex v = PrimVertex{};
    v.shape[0] = p.shape;
    v.shape[1] = p.feather;
    v.shape[2] = p.radius;
    v.shape[3] = p.len;
    v.center[0] = p.center.x;
    v.center[1] = p.center.y;
    v.center[2] = p.dir.x;
    v.center[3] = p.dir.y;
    ncl_math::ColorF cf = p.color.ToFloat();
    v.color[0] = cf.r;
    v.color[1] = cf.g;
    v.color[2] = cf.b;
    v.color[3] = p.alpha;
    v.tri[0] = p.t0.x;
    v.tri[1] = p.t0.y;
    v.tri[2] = p.t1.x;
    v.tri[3] = p.t1.y;
    v.tri2[0] = p.t2.x;
    v.tri2[1] = p.t2.y;
    v.tri2[2] = p.spread;
    v.tri2[3] = 0.0f;

    const ncl_math::Vector2 corner_pos[4] = {
        {quad.min.x, quad.min.y}, {quad.max.x, quad.min.y}, {quad.max.x, quad.max.y}, {quad.min.x, quad.max.y}};
    PrimVertex corners[4] = {v, v, v, v};

    for (int c = 0; c < 4; c++) {
        corners[c].pos[0] = corner_pos[c].x;
        corners[c].pos[1] = corner_pos[c].y;
    }

    AddQuad(blend_src, blend_dst, corners);
}
