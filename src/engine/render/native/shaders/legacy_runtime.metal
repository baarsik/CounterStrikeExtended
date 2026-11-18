// Production legacy renderer. Offline compiled; no runtime source compiler.
#include <metal_stdlib>
using namespace metal;

struct VertexConstants {
    float4x4 ModelView;
    float4x4 Projection;
    float4x4 TextureMatrix[3];
    float4 Color;
    float4 UvOffset[3];
};

struct FragmentConstants {
    float4 Unit[3];
    float4 Components[3];
    float4 FogColor;
    float4 Fog;
    float4 FogAlpha;
    float4 Texture[3];
    float4 Sampling[3];
};

struct Input {
    float4 position [[attribute(0)]];
    float4 color [[attribute(1)]];
    float4 uv0 [[attribute(2)]];
    float4 uv1 [[attribute(3)]];
    float4 uv2 [[attribute(4)]];
};

struct SingleTextureInput {
    float4 position [[attribute(0)]];
    float4 color [[attribute(1)]];
    float4 uv [[attribute(2)]];
};

struct WorldInput {
    float3 position [[attribute(0)]];
    float2 uv0 [[attribute(1)]];
    float2 uv1 [[attribute(2)]];
};

struct Varying {
    float4 position [[position]];
    float4 color;
    float4 uv0;
    float4 uv1;
    float4 uv2;
    float eyeDepth;
};

static_assert(sizeof(VertexConstants) == 384, "Captured vertex uniform layout");
static_assert(sizeof(FragmentConstants) == 240, "Captured fragment uniform layout");

Varying TransformLegacyVertex(float4 position, float4 color, float4 uv0, float4 uv1, float4 uv2,
    constant VertexConstants& constants) {
    Varying output;
    float4 eye = constants.ModelView * position;
    output.position = constants.Projection * eye;
    // GL clip depth [-w,w] becomes SDL GPU [0,w], as in the HLSL counterpart.
    output.position.z = (output.position.z + output.position.w) * 0.5f;
    output.color = color * constants.Color;
    output.uv0 = constants.TextureMatrix[0] * (uv0 + constants.UvOffset[0]);
    output.uv1 = constants.TextureMatrix[1] * (uv1 + constants.UvOffset[1]);
    output.uv2 = constants.TextureMatrix[2] * (uv2 + constants.UvOffset[2]);
    output.eyeDepth = abs(eye.z / eye.w);
    return output;
}

vertex Varying LegacyVertex(Input input [[stage_in]],
    constant VertexConstants& constants [[buffer(0)]]) {
    return TransformLegacyVertex(input.position, input.color, input.uv0, input.uv1, input.uv2, constants);
}

vertex Varying LegacySingleTextureVertex(SingleTextureInput input [[stage_in]],
    constant VertexConstants& constants [[buffer(0)]]) {
    const float4 unusedUv = float4(0, 0, 0, 1);
    return TransformLegacyVertex(input.position, input.color, input.uv, unusedUv, unusedUv, constants);
}

vertex Varying LegacyWorldVertex(WorldInput input [[stage_in]],
    constant VertexConstants& constants [[buffer(0)]]) {
    const float4 uv0 = float4(input.uv0, 0, 1);
    return TransformLegacyVertex(float4(input.position, 1), float4(1), uv0, float4(input.uv1, 0, 1), uv0, constants);
}

float4 Environment(float4 previous, float4 sampleValue, float4 unit, float4 components) {
    if (unit.y == 1.0f) {
        return float4(components.x != 0.0f ? sampleValue.rgb : previous.rgb,
            components.y != 0.0f ? sampleValue.a : previous.a);
    }
    // ALPHA contributes RGB zero to COMBINE (ARB_texture_env_combine Table 3.23).
    // Plain MODULATE preserves the incoming RGB when that component is absent.
    float4 modulation = float4(components.x != 0.0f ? sampleValue.rgb : float3(unit.y == 2.0f ? 0 : 1),
        components.y != 0.0f ? sampleValue.a : 1.0f);
    float4 result = previous * modulation;
    if (unit.y == 2.0f) {
        result.rgb *= unit.z;
    }
    return saturate(result);
}

bool AlphaPass(float alpha, uint comparison, float reference) {
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

// SDL_GPU exposes edge clamp only. GL_CLAMP needs transparent-black border taps.
// Fetch the selected mip levels explicitly on that path, retaining edge/repeat per axis.
int Wrapped(int value, int size, uint wrap, thread bool& border) {
    if (wrap == 0) {
        return (value % size + size) % size;
    }
    if (wrap == 1 && (value < 0 || value >= size)) {
        border = true;
    }
    return clamp(value, 0, size - 1);
}
float4 Tap(texture2d<float> image, int2 pixel, uint level, uint2 wrap) {
    bool border = false;
    pixel.x = Wrapped(pixel.x, int(image.get_width(level)), wrap.x, border);
    pixel.y = Wrapped(pixel.y, int(image.get_height(level)), wrap.y, border);
    return border ? float4(0) : image.read(uint2(pixel), level);
}
float4 BorderLevel(texture2d<float> image, float2 uv, uint level, uint2 wrap, bool linear) {
    float2 size = float2(image.get_width(level), image.get_height(level));
    if (!linear) {
        int2 pixel = int2(floor(uv * size));
        // GL nearest clamp never selects border texels, including the normalized upper endpoint.
        if (wrap.x == 1) {
            pixel.x = clamp(pixel.x, 0, int(size.x) - 1);
        }
        if (wrap.y == 1) {
            pixel.y = clamp(pixel.y, 0, int(size.y) - 1);
        }
        return Tap(image, pixel, level, wrap);
    }
    float2 pixel = uv * size - 0.5f;
    int2 base = int2(floor(pixel));
    float2 weight = fract(pixel);
    return mix(mix(Tap(image, base, level, wrap), Tap(image, base + int2(1, 0), level, wrap), weight.x),
        mix(Tap(image, base + int2(0, 1), level, wrap), Tap(image, base + int2(1), level, wrap), weight.x), weight.y);
}
float4 BorderSample(texture2d<float> image, float2 uv, float lod, float mipLod, float4 dimensions, float4 sampling) {
    uint2 wrap = uint2(sampling.xy);
    if (wrap.x == 1) { uv.x = clamp(uv.x, 0.0f, 1.0f); }
    if (wrap.y == 1) { uv.y = clamp(uv.y, 0.0f, 1.0f); }
    uint filter = uint(sampling.z);
    bool linear = lod <= 0 ? sampling.w == 1 : (filter == 1 || filter == 3 || filter == 5);
    lod = filter < 2 || dimensions.z != 0 ? 0 : mipLod;
    lod = clamp(lod, 0.0f, float(image.get_num_mip_levels() - 1));
    if (filter == 4 || filter == 5) {
        uint low = uint(floor(lod));
        uint high = min(low + 1, image.get_num_mip_levels() - 1);
        return mix(BorderLevel(image, uv, low, wrap, linear), BorderLevel(image, uv, high, wrap, linear), fract(lod));
    }
    return BorderLevel(image, uv, uint(floor(lod + 0.5f)), wrap, linear);
}

template<bool SoftwareBorder>
float4 Sample(texture2d<float> image, sampler state, float4 coordinate, float4 dimensions, float4 sampling) {
    float2 uv = coordinate.xy / coordinate.w;
    if (dimensions.z != 0) {
        uv /= max(dimensions.xy, float2(1));
    }
    if (!SoftwareBorder || (sampling.x != 1 && sampling.y != 1)) {
        return image.sample(state, uv);
    }
    float2 size = float2(image.get_width(), image.get_height());
    float2 dx = dfdx(uv);
    float2 dy = dfdy(uv);
    float xLength = length(dx * size);
    float yLength = length(dy * size);
    float major = max(xLength, yLength);
    float minor = max(min(xLength, yLength), 0.000001f);
    // Bounded anisotropic reconstruction for the border path. Ordinary repeat/edge uses hardware anisotropy.
    uint taps = uint(ceil(clamp(major / minor, 1.0f, max(1.0f, dimensions.w))));
    // Unclamped LOD selects min/mag; clamped LOD also applies the device's nearest-mip selection.
    // An analytic log2 plus rounding disagrees with hardware near half-level boundaries.
    // The explicit multi-tap path still reconstructs its own reduced anisotropic footprint.
    float lod = taps == 1 ? image.calculate_unclamped_lod(state, uv) :
        log2(max(major / float(taps), 0.000001f));
    float mipLod = taps == 1 ? image.calculate_clamped_lod(state, uv) : lod;
    float2 axis = xLength > yLength ? dx : dy;
    float4 result = float4(0);
    for (uint tap = 0; tap < taps; ++tap) {
        float offset = (float(tap) + 0.5f) / float(taps) - 0.5f;
        result += BorderSample(image, uv + axis * offset, lod, mipLod, dimensions, sampling);
    }
    return result / float(taps);
}

template<bool SoftwareBorder>
float4 ApplyTexture(float4 previous, texture2d<float> image, sampler state, float4 coordinate,
    constant FragmentConstants& constants, uint unit) {
    // The same enable bit applies to every fragment of the draw, including derivative-based sampling.
    if (constants.Unit[unit].x == 0.0f) {
        return previous;
    }
    return Environment(previous, Sample<SoftwareBorder>(image, state, coordinate, constants.Texture[unit], constants.Sampling[unit]),
        constants.Unit[unit], constants.Components[unit]);
}

template<bool SoftwareBorder>
float4 LegacyColor(Varying input, constant FragmentConstants& constants,
    texture2d<float> texture0, texture2d<float> texture1, texture2d<float> texture2,
    sampler sampler0, sampler sampler1, sampler sampler2) {
    float4 result = ApplyTexture<SoftwareBorder>(input.color, texture0, sampler0, input.uv0, constants, 0);
    result = ApplyTexture<SoftwareBorder>(result, texture1, sampler1, input.uv1, constants, 1);
    result = ApplyTexture<SoftwareBorder>(result, texture2, sampler2, input.uv2, constants, 2);
    if (constants.Fog.x != 0.0f) {
        float distance = input.eyeDepth;
        float factor;
        if (constants.Fog.y == 2.0f) {
            factor = (constants.FogAlpha.x - distance) / (constants.FogAlpha.x - constants.Fog.w);
        } else if (constants.Fog.y == 1.0f) {
            float densityDistance = constants.Fog.z * distance;
            factor = exp(-densityDistance * densityDistance);
        } else {
            factor = exp(-constants.Fog.z * distance);
        }
        result.rgb = mix(constants.FogColor.rgb, result.rgb, saturate(factor));
    }
    return result;
}

// This entry point cannot discard fragments, allowing early depth testing.
fragment float4 LegacyFragment(Varying input [[stage_in]],
    constant FragmentConstants& constants [[buffer(0)]],
    texture2d<float> Texture0 [[texture(0)]], texture2d<float> Texture1 [[texture(1)]],
    texture2d<float> Texture2 [[texture(2)]], sampler Sampler0 [[sampler(0)]],
    sampler Sampler1 [[sampler(1)]], sampler Sampler2 [[sampler(2)]]) {
    return LegacyColor<true>(input, constants, Texture0, Texture1, Texture2, Sampler0, Sampler1, Sampler2);
}

fragment float4 LegacyFragmentAlphaTest(Varying input [[stage_in]],
    constant FragmentConstants& constants [[buffer(0)]],
    texture2d<float> Texture0 [[texture(0)]], texture2d<float> Texture1 [[texture(1)]],
    texture2d<float> Texture2 [[texture(2)]], sampler Sampler0 [[sampler(0)]],
    sampler Sampler1 [[sampler(1)]], sampler Sampler2 [[sampler(2)]]) {
    const float4 result = LegacyColor<true>(input, constants, Texture0, Texture1, Texture2, Sampler0, Sampler1, Sampler2);
    if (!AlphaPass(result.a, uint(constants.FogAlpha.z), constants.FogAlpha.w)) {
        discard_fragment();
    }
    return result;
}

// This entry point cannot discard fragments, allowing early depth testing.
fragment float4 LegacyFragmentHardwareSampling(Varying input [[stage_in]],
    constant FragmentConstants& constants [[buffer(0)]],
    texture2d<float> Texture0 [[texture(0)]], texture2d<float> Texture1 [[texture(1)]],
    texture2d<float> Texture2 [[texture(2)]], sampler Sampler0 [[sampler(0)]],
    sampler Sampler1 [[sampler(1)]], sampler Sampler2 [[sampler(2)]]) {
    return LegacyColor<false>(input, constants, Texture0, Texture1, Texture2, Sampler0, Sampler1, Sampler2);
}

fragment float4 LegacyFragmentAlphaTestHardwareSampling(Varying input [[stage_in]],
    constant FragmentConstants& constants [[buffer(0)]],
    texture2d<float> Texture0 [[texture(0)]], texture2d<float> Texture1 [[texture(1)]],
    texture2d<float> Texture2 [[texture(2)]], sampler Sampler0 [[sampler(0)]],
    sampler Sampler1 [[sampler(1)]], sampler Sampler2 [[sampler(2)]]) {
    const float4 result = LegacyColor<false>(input, constants, Texture0, Texture1, Texture2, Sampler0, Sampler1, Sampler2);
    if (!AlphaPass(result.a, uint(constants.FogAlpha.z), constants.FogAlpha.w)) {
        discard_fragment();
    }
    return result;
}
