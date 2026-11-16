// Analytic expressions ported from the canonical radar GLSL shaders; retain their numeric constants.
#include <metal_stdlib>
using namespace metal;
struct RadarVertexIn {
    float2 position [[attribute(0)]];
    float4 shape [[attribute(1)]];
    float4 center [[attribute(2)]];
    float4 color [[attribute(3)]];
    float4 triangle [[attribute(4)]];
    float4 triangle2 [[attribute(5)]];
};
struct RadarVarying {
    float4 position [[position]];
    float2 screen;
    float4 shape;
    float4 center;
    float4 color;
    float4 triangle;
    float4 triangle2;
};
struct RadarUniforms {
    float4 transform; // HUD -> scene clip, including viewport
    float4 clip; // center, radius, square
    float4 fade;
    float4 effects;
    float4 effectMode;
    float4 color;
    float4 alpha;
    float4 depth;

};
vertex RadarVarying RadarVertex(RadarVertexIn input [[stage_in]], constant RadarUniforms& u [[buffer(0)]]) {
    RadarVarying out;
    out.position = float4(input.position * u.transform.xy + u.transform.zw, 0, 1);
    if (u.depth.z != 0) {
        out.position.z = (u.depth.y - input.triangle2.w) / (u.depth.y - u.depth.x);
    }
    out.screen = input.position;
    out.shape = input.shape;
    out.center = input.center;
    out.color = input.color;
    out.triangle = input.triangle;
    out.triangle2 = input.triangle2;
    return out;
}
float cross2(float2 u, float2 v) {
    return u.x * v.y - u.y * v.x;
}

float segDist(float2 p, float2 a, float2 b) {
    float2 e = b - a;
    float2 w = p - a;
    float t = clamp(dot(w, e) / max(dot(e, e), 1e-5f), 0.0f, 1.0f);
    return length(w - e * t);
}

float roundedBoxDist(float2 p, float2 halfSize, float radius) {
    float2 q = abs(p) - halfSize + float2(radius);
    return length(max(q, 0.0f)) + min(max(q.x, q.y), 0.0f) - radius;
}

float signalArcDist(float2 p, float radius) {
    p.x = abs(p.x);
    if (p.y <= -p.x) {
        return abs(length(p) - radius);
    }
    return distance(p, float2(0.70710678f, -0.70710678f) * radius);
}

float TriangleCoverage(float2 p, float2 t0, float2 t1, float2 t2, float feather) {
    float d0 = cross2(t1 - t0, p - t0);
    float d1 = cross2(t2 - t1, p - t1);
    float d2 = cross2(t0 - t2, p - t2);
    bool inside = (d0 >= 0.0f && d1 >= 0.0f && d2 >= 0.0f) || (d0 <= 0.0f && d1 <= 0.0f && d2 <= 0.0f);
    float dist = min(segDist(p, t0, t1), min(segDist(p, t1, t2), segDist(p, t2, t0)));
    return 1.0f - smoothstep(-feather, feather, inside ? -dist : dist);
}

float4 PlayerMarker(float2 p, float2 center, float2 direction, bool offscreen, float radius, float4 color) {
    bool directed = dot(direction, direction) > 0.5f;
    float2 perpendicular = float2(-direction.y, direction.x);
    if (offscreen && directed) {
        float2 base = center - direction * (radius * 0.8f);
        float coverage = TriangleCoverage(p, center + direction * (radius * 1.7f),
            base + perpendicular * (radius * 1.2f), base - perpendicular * (radius * 1.2f), 1.1f);
        float alpha = color.a * coverage;
        return float4(color.rgb * alpha, alpha);
    }
    float disc = color.a * (1.0f - smoothstep(radius - 1.2f, radius, distance(p, center)));
    float heading = 0.0f;
    if (directed) {
        float2 base = center + direction * (cos(0.62f) * radius);
        float2 width = perpendicular * (sin(0.62f) * radius);
        heading = color.a * TriangleCoverage(p, center + direction * (radius * 2.0f), base + width, base - width, 1.1f);
    }
    return float4(color.rgb * disc + float3(240.0f / 255.0f) * heading * (1.0f - disc), disc + heading * (1.0f - disc));
}

fragment float4 RadarPrimitive(RadarVarying input [[stage_in]],
    constant RadarUniforms& u [[buffer(0)]], texture2d<float> atlas [[texture(0)]], sampler sampling [[sampler(0)]]) {
    float uShape = input.shape.x; // 0 disc, 1 cone, 2 triangle, 3 cross, 4 ring, 5 solid, 6 glyph, 7 question mark, 8 bomb, 9 disconnected signal, 10 player marker fade
    float uFeather = input.shape.y;
    float uRadius = input.shape.z;
    float uLen = input.shape.w;
    float2 uCenter = input.center.xy;
    float2 uDir = input.center.zw;
    float4 uColor = input.color;
    float2 uT0 = input.triangle.xy;
    float2 uT1 = input.triangle.zw;
    float2 uT2 = input.triangle2.xy;
    float uSpread = input.triangle2.z;

    float2 p = input.screen;
    float a;
    float3 rgb = uColor.rgb;
    if (uShape < 0.5f) {
        a = 1.0f - smoothstep(uRadius - uFeather, uRadius, distance(p, uCenter));
    }
    else if (uShape < 1.5f) {
        float2 rel = p - uCenter;
        float2 perp = float2(-uDir.y, uDir.x);
        float t = dot(rel, uDir);

        if (t <= 0.0f) {
            discard_fragment();
        }

        float ang = atan(abs(dot(rel, perp)) / t);
        float edgeAng = 1.0f - smoothstep(uSpread - 0.10f, uSpread, ang);
        float edgeLen = 1.0f - smoothstep(uLen - uFeather, uLen, t);

        a = exp(-3.5f * (t / uLen)) * edgeAng * edgeLen;
    }
    else if (uShape < 2.5f) {
        a = TriangleCoverage(p, uT0, uT1, uT2, uFeather);
    }
    else if (uShape < 3.5f) {
        float2 c = uCenter; // cross: two diagonals as one shape (uRadius extent, uLen half-width)
        float dl = min(segDist(p, c + float2(-uRadius, -uRadius), c + float2(uRadius, uRadius)),
                       segDist(p, c + float2(-uRadius,  uRadius), c + float2(uRadius, -uRadius)));

        a = 1.0f - smoothstep(uLen - uFeather, uLen, dl);
    }
    else if (uShape < 4.5f) {
        float dd = distance(p, uCenter); // ring: uRadius outer, uLen inner

        a = (1.0f - smoothstep(uRadius - uFeather, uRadius, dd)) * smoothstep(uLen - uFeather, uLen, dd);
    }
    else if (uShape < 5.5f) {
        a = 1.0f; // solid: the quad geometry itself is the shape (polyline segments)
    }
    else if (uShape < 6.5f) {
        // glyph: LUMINANCE_ALPHA atlas cell with alpha = coverage (the fill plus the baked drop
        // shadow) and luminance = the glyph fill. Modulate the caller colour by luminance, so the
        // body takes the colour while the baked drop shadow (luminance 0) stays black, matching the
        // engine's own scheme text.
        float4 g = atlas.sample(sampling, uT0);
        a = g.a;
        rgb = uColor.rgb * g.r;
    }
    else if (uShape < 7.5f) {
        // Question mark: a circular hook, a short stem and a dot share the cross's edge profile.
        float2 q = (p - uCenter) / uRadius;
        float2 rel = q - float2(0.0f, -0.5f);
        float2 arcEnd = float2(0.35355339f, 0.35355339f);
        float hook = abs(length(rel) - 0.5f);
        if (rel.y > 0.0f && rel.x < rel.y) {
            hook = min(distance(rel, float2(-0.5f, 0.0f)), distance(rel, arcEnd));
        }
        float stem = segDist(q, arcEnd + float2(0.0f, -0.5f), float2(0.0f, 0.05f));
        float dotDist = distance(p, uCenter + float2(0.0f, uRadius)) + uLen * 0.25f;
        float dl = min(min(hook, stem) * uRadius, dotDist);
        a = 1.0f - smoothstep(uLen - uFeather, uLen, dl);
    }
    else if (uShape < 8.5f) {
        float2 q = (p - uCenter) / uRadius;
        float body = roundedBoxDist(q - float2(0.0f, 0.12f), float2(0.9f, 0.62f), 0.10f);
        float wire = min(segDist(q, float2(-0.52f, -0.42f), float2(-0.52f, -0.80f)),
                         min(segDist(q, float2(-0.52f, -0.80f), float2(0.12f, -0.80f)),
                             segDist(q, float2(0.12f, -0.80f), float2(0.12f, -0.42f)))) - 0.08f;
        a = 1.0f - smoothstep(-uFeather, 0.0f, min(body, wire) * uRadius);

        float detail = roundedBoxDist(q - float2(-0.20f, -0.20f), float2(0.40f, 0.12f), 0.02f);
        detail = min(detail, roundedBoxDist(q - float2(0.55f, 0.10f), float2(0.13f, 0.38f), 0.03f));
        for (int row = 0; row < 2; row++) {
            for (int column = 0; column < 3; column++) {
                float2 key = float2(-0.48f + float(column) * 0.24f, 0.14f + float(row) * 0.24f);
                detail = min(detail, roundedBoxDist(q - key, float2(0.075f, 0.055f), 0.01f));
            }
        }
        float ink = 1.0f - smoothstep(-0.4f, 0.4f, detail * uRadius);
        rgb = mix(uColor.rgb, uColor.rgb * 0.20f, ink);
    }
    else if (uShape < 9.5f) {
        float2 q = p - uCenter;
        float2 rel = q - float2(0.0f, 0.5f * uRadius);
        float arcs = min(signalArcDist(rel, 1.05f * uRadius), signalArcDist(rel, 0.60f * uRadius)) - uLen;
        float dotDist = length(rel) - max(1.65f, 0.14f * uRadius);
        float slash = segDist(q, float2(-0.85f, 0.8f) * uRadius, float2(0.85f, -0.85f) * uRadius) - uLen;
        float signal = max(min(arcs, dotDist), 0.65f - slash);
        a = 1.0f - smoothstep(-uFeather, uFeather, min(signal, slash));
    }
    else {
        bool currentOffscreen = (uT2.x > 0.5f && uT2.x < 1.5f) || uT2.x > 2.5f;
        bool previousOffscreen = uT2.x > 1.5f;
        float4 current = PlayerMarker(p, uCenter, uT0, currentOffscreen, uRadius, uColor) * uSpread;
        float4 previous = PlayerMarker(p, uDir, uT1, previousOffscreen, uRadius, float4(uColor.rgb, uLen)) * uT2.y;
        // Blend the two premultiplied marker images once, including their shared pixels.
        float4 marker = current + previous;
        a = marker.a;
        rgb = a > 0.0f ? marker.rgb / a : uColor.rgb;
        uColor.a = 1.0f;
    }

    float2 cd = abs(input.screen - u.clip.xy);
    float cdist = (u.clip.w > 0.5f) ? max(cd.x, cd.y) : length(cd);

    a *= 1.0f - smoothstep(u.clip.z - 1.0f, u.clip.z, cdist);

    return float4(mix(rgb, u.fade.rgb, u.fade.a), uColor.a * a);
}

bool RadarAlpha(float alpha, constant RadarUniforms& u) {
    if (u.alpha.x == 0) { return true; }
    int comparison = int(u.alpha.y);
    float reference = u.alpha.z;
    switch (comparison) {
    case 0: return false;
    case 1: return alpha < reference;
    case 2: return alpha == reference;
    case 3: return alpha <= reference;
    case 4: return alpha > reference;
    case 5: return alpha != reference;
    case 6: return alpha >= reference;
    default: return true;
    }
}

fragment float4 RadarMask(RadarVarying input [[stage_in]], constant RadarUniforms& u [[buffer(0)]],
    texture2d<float> image [[texture(0)]], sampler sampling [[sampler(0)]]) {
    float2 delta = abs(input.screen - u.clip.xy);
    float distance = u.clip.w > 0.5f ? max(delta.x, delta.y) : length(delta);
    float4 result;
    if (u.effectMode.x == 3) {
        float coverage = smoothstep(u.effects.z - u.effects.x, u.effects.z + u.effects.x, distance) *
            (1.0f - smoothstep(u.clip.z - u.effects.x, u.clip.z + u.effects.x, distance));
        result = float4(mix(u.color.rgb, u.fade.rgb, u.fade.a), u.color.a * coverage);
    } else {
        float2 uv = input.triangle.xy;
        result = image.sample(sampling, uv);
        if (u.effects.w != 0) {
            float2 dx = dfdx(uv);
            // GL derivatives are bottom-origin. Preserve its signed four-sample rotation.
            float2 dy = -dfdy(uv);
            float4 s0 = image.sample(sampling, uv - 0.125f * dx - 0.375f * dy);
            float4 s1 = image.sample(sampling, uv + 0.375f * dx - 0.125f * dy);
            float4 s2 = image.sample(sampling, uv - 0.375f * dx + 0.125f * dy);
            float4 s3 = image.sample(sampling, uv + 0.125f * dx + 0.375f * dy);
            float alpha = 0.25f * (s0.a + s1.a + s2.a + s3.a);
            if (result.a == 0) {
                result = s0;
                if (s1.a > result.a) { result = s1; }
                if (s2.a > result.a) { result = s2; }
                if (s3.a > result.a) { result = s3; }
            }
            result = result.a > 0 ? float4(result.rgb * (alpha / result.a), alpha) : float4(0);
        }
        result *= u.color;
        if (u.effectMode.x != 0) {
            result.rgb *= mix(1.15f, 0.575f, clamp(distance / u.clip.z, 0.0f, 1.0f));
            float coverage = 1.0f - smoothstep(u.clip.z - u.effects.x, u.clip.z, distance);
            result.a *= coverage;
            if (u.effectMode.x == 2) { result.rgb *= coverage; }
            if (u.effects.y > 0) { result.rgb = pow(result.rgb, float3(u.effects.y)); }
            result.rgb = mix(result.rgb, u.fade.rgb * (u.effectMode.x == 2 ? result.a : 1.0f), u.fade.a);
        }
    }
    if (!RadarAlpha(result.a, u)) { discard_fragment(); }
    return result;
}

fragment float4 RadarMesh(RadarVarying input [[stage_in]], constant RadarUniforms& u [[buffer(0)]],
    texture2d<float> base [[texture(0)]], sampler baseSampling [[sampler(0)]],
    texture2d<float> lightmap [[texture(1)]], sampler lightSampling [[sampler(1)]]) {
    float4 color = input.color;
    if (u.effects.x != 0) { color *= base.sample(baseSampling, input.triangle.xy); }
    if (u.effects.y != 0) { color *= lightmap.sample(lightSampling, input.triangle.zw); }
    return color;
}

fragment float4 RadarTone(RadarVarying input [[stage_in]], constant RadarUniforms& u [[buffer(0)]],
    texture2d<float> source [[texture(0)]], sampler sampling [[sampler(0)]]) {
    float4 color = source.sample(sampling, input.triangle.xy);
    // The original blend pass quantizes to RGBA8 before the optional lift.
    color.rgb = round(color.rgb * color.a * 255.0f) / 255.0f;
    if (u.effects.x != 0) { color.rgb = 0.17f * color.a + 0.83f * color.rgb; }
    return color;
}

fragment float4 RadarFilter(RadarVarying input [[stage_in]], constant RadarUniforms& u [[buffer(0)]],
    texture2d<float> source [[texture(0)]], sampler sampling [[sampler(0)]]) {
    float3 result(0);
    if (u.effects.z != 0) {
        constexpr float weights[] = {0.2270270270f, 0.3162162162f, 0.3162162162f, 0.0702702703f, 0.0702702703f};
        constexpr float offsets[] = {0.0f, 1.3846153846f, -1.3846153846f, 3.2307692308f, -3.2307692308f};
        for (uint tap = 0; tap < 5; ++tap) {
            result += source.sample(sampling, input.triangle.xy + u.effects.xy * (offsets[tap] * 1.15f)).rgb * weights[tap];
            // Legacy Smoked accumulates each additive tap into an RGBA8 target.
            result = round(result * 255.0f) / 255.0f;
        }
    } else {
        float sum = 0;
        for (int offset = -16; offset <= 16; ++offset) {
            float weight = exp(-float(offset * offset) / 72.0f);
            result += source.sample(sampling, input.triangle.xy + u.effects.xy * float(offset)).rgb * weight;
            sum += weight;
        }
        result /= sum;
    }
    return float4(result, 1);
}

fragment float4 RadarBackdrop(RadarVarying input [[stage_in]], constant RadarUniforms& u [[buffer(0)]],
    texture2d<float> source [[texture(0)]], sampler sampling [[sampler(0)]]) {
    float3 rgb = source.sample(sampling, input.triangle.xy).rgb;
    float alpha;
    if (u.effectMode.x == 5) {
        rgb *= input.color.rgb;
        alpha = input.color.a;
    } else {
        float2 delta = abs(input.screen - u.clip.xy);
        float distance = u.clip.w > 0 ? max(delta.x, delta.y) : length(delta);
        alpha = (1.0f - smoothstep(u.clip.z - 1.5f, u.clip.z, distance)) * u.color.a;
        rgb *= u.color.r;
        rgb = mix(rgb, u.fade.rgb, u.fade.a);
    }
    if (!RadarAlpha(alpha, u)) { discard_fragment(); }
    return float4(rgb, alpha);
}
