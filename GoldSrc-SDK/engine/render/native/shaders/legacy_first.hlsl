// Original private shader for the bounded captured-state adapter; no runtime source compilation.
cbuffer VertexConstants : register(b0, space1) {
    column_major float4x4 ModelView;
    column_major float4x4 Projection;
    column_major float4x4 TextureMatrix[3];
};

Texture2D<float4> Texture0 : register(t0, space2);
Texture2D<float4> Texture1 : register(t1, space2);
Texture2D<float4> Texture2 : register(t2, space2);
SamplerState Sampler0 : register(s0, space2);
SamplerState Sampler1 : register(s1, space2);
SamplerState Sampler2 : register(s2, space2);

cbuffer FragmentConstants : register(b0, space3) {
    float4 Unit[3];             // enabled, environment mode, RGB scale, unused
    float4 Components[3];       // color present, alpha present, unused, unused
    float4 FogColor;
    float4 Fog;                 // enabled, mode, density, start
    float4 FogAlpha;            // end, alpha enabled, comparison, reference
};

struct Input {
    float4 position : TEXCOORD0;
    float4 color : TEXCOORD1;
    float4 uv0 : TEXCOORD2;
    float4 uv1 : TEXCOORD3;
    float4 uv2 : TEXCOORD4;
};

struct Varying {
    float4 position : SV_Position;
    float4 color : TEXCOORD0;
    float2 uv0 : TEXCOORD1;
    float2 uv1 : TEXCOORD2;
    float2 uv2 : TEXCOORD3;
    float eyeDepth : TEXCOORD4;
};

Varying LegacyVertex(Input input) {
    Varying output;
    float4 eye = mul(ModelView, input.position);
    output.position = mul(Projection, eye);
    // GL clip depth [-w,w] becomes SDL GPU [0,w]. XY/color/gameplay matrices are unchanged.
    output.position.z = (output.position.z + output.position.w) * 0.5f;
    output.color = input.color;
    output.uv0 = mul(TextureMatrix[0], input.uv0).xy;
    output.uv1 = mul(TextureMatrix[1], input.uv1).xy;
    output.uv2 = mul(TextureMatrix[2], input.uv2).xy;
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

float4 LegacyFragment(Varying input) : SV_Target0 {
    float4 result = Environment(input.color, Texture0.Sample(Sampler0, input.uv0), Unit[0], Components[0]);
    result = Environment(result, Texture1.Sample(Sampler1, input.uv1), Unit[1], Components[1]);
    result = Environment(result, Texture2.Sample(Sampler2, input.uv2), Unit[2], Components[2]);
    if (Fog.x != 0.0f) {
        float distance = input.eyeDepth;
        float factor;
        if (Fog.y == 2.0f) {
            factor = (FogAlpha.x - distance) / (FogAlpha.x - Fog.w);
        } else if (Fog.y == 1.0f) {
            float densityDistance = Fog.z * distance;
            factor = exp(-densityDistance * densityDistance);
        } else {
            factor = exp(-Fog.z * distance);
        }
        result.rgb = lerp(FogColor.rgb, result.rgb, saturate(factor));
    }
    if (FogAlpha.y != 0.0f && !AlphaPass(result.a, (uint)FogAlpha.z, FogAlpha.w)) {
        discard;
    }
    return result;
}
