// Original private counterpart of legacy_first.hlsl; compiled offline, never at runtime.
#include <metal_stdlib>
using namespace metal;

struct VertexConstants {
    float4x4 ModelView;
    float4x4 Projection;
    float4x4 TextureMatrix[3];
};

struct FragmentConstants {
    float4 Unit[3];
    float4 Components[3];
    float4 FogColor;
    float4 Fog;
    float4 FogAlpha;
};

struct Input {
    float4 position [[attribute(0)]];
    float4 color [[attribute(1)]];
    float4 uv0 [[attribute(2)]];
    float4 uv1 [[attribute(3)]];
    float4 uv2 [[attribute(4)]];
};

struct Varying {
    float4 position [[position]];
    float4 color;
    float2 uv0;
    float2 uv1;
    float2 uv2;
    float eyeDepth;
};

static_assert(sizeof(VertexConstants) == 320, "Captured vertex uniform layout");
static_assert(sizeof(FragmentConstants) == 144, "Captured fragment uniform layout");

vertex Varying LegacyVertex(Input input [[stage_in]], constant VertexConstants& constants [[buffer(0)]]) {
    Varying output;
    float4 eye = constants.ModelView * input.position;
    output.position = constants.Projection * eye;
    // GL clip depth [-w,w] becomes SDL GPU [0,w], as in the HLSL counterpart.
    output.position.z = (output.position.z + output.position.w) * 0.5f;
    output.color = input.color;
    output.uv0 = (constants.TextureMatrix[0] * input.uv0).xy;
    output.uv1 = (constants.TextureMatrix[1] * input.uv1).xy;
    output.uv2 = (constants.TextureMatrix[2] * input.uv2).xy;
    output.eyeDepth = abs(eye.z / eye.w);
    return output;
}

float4 Environment(float4 previous, float4 sampleValue, float4 unit, float4 components) {
    float4 result = previous;
    if (unit.x != 0.0f) {
        if (unit.y == 1.0f) {
            result = float4(components.x != 0.0f ? sampleValue.rgb : previous.rgb,
                components.y != 0.0f ? sampleValue.a : previous.a);
        } else {
            result = previous * sampleValue;
            if (unit.y == 2.0f) {
                result.rgb *= unit.z;
            }
            result = saturate(result);
        }
    }
    return result;
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

fragment float4 LegacyFragment(Varying input [[stage_in]],
    constant FragmentConstants& constants [[buffer(0)]],
    texture2d<float> Texture0 [[texture(0)]], texture2d<float> Texture1 [[texture(1)]],
    texture2d<float> Texture2 [[texture(2)]], sampler Sampler0 [[sampler(0)]],
    sampler Sampler1 [[sampler(1)]], sampler Sampler2 [[sampler(2)]]) {
    float4 result = Environment(input.color, Texture0.sample(Sampler0, input.uv0),
        constants.Unit[0], constants.Components[0]);
    result = Environment(result, Texture1.sample(Sampler1, input.uv1), constants.Unit[1], constants.Components[1]);
    result = Environment(result, Texture2.sample(Sampler2, input.uv2), constants.Unit[2], constants.Components[2]);
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
    if (constants.FogAlpha.y != 0.0f &&
        !AlphaPass(result.a, uint(constants.FogAlpha.z), constants.FogAlpha.w)) {
        discard_fragment();
    }
    return result;
}
