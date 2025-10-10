//
//  Shaders.metal
//  MSDFTextRender
//
//  Created by Sihao Lu on 10/8/25.
//

// File for Metal kernel and shader functions

#include <metal_stdlib>
#include <simd/simd.h>

// Including header shared between this Metal shader code and Swift/C code executing Metal API commands
#import "ShaderTypes.h"

using namespace metal;

typedef struct
{
    float3 position [[attribute(VertexAttributePosition)]];
    float2 texCoord [[attribute(VertexAttributeTexcoord)]];
} Vertex;

typedef struct
{
    float4 position [[position]];
    float2 texCoord;
} ColorInOut;

vertex ColorInOut vertexShader(Vertex in [[stage_in]],
                               constant Uniforms & uniforms [[ buffer(BufferIndexUniforms) ]])
{
    ColorInOut out;

    float4 position = float4(in.position, 1.0);
    out.position = uniforms.projectionMatrix * uniforms.modelViewMatrix * position;
    out.texCoord = in.texCoord;

    return out;
}

fragment float4 fragmentShader(ColorInOut in [[stage_in]],
                               constant Uniforms & uniforms [[ buffer(BufferIndexUniforms) ]],
                               texture2d<float> colorMap     [[ texture(TextureIndexColor) ]])
{
    constexpr sampler colorSampler(address::clamp_to_edge,
                                   filter::bicubic);

    float3 sample = colorMap.sample(colorSampler, in.texCoord).rgb;
    float msdf = max(min(sample.r, sample.g), min(max(sample.r, sample.g), sample.b));
    float2 screenTexSize = 1.0f / fwidth(in.texCoord);
    float screenPxRange = max(0.5f * dot(uniforms.unitRange, screenTexSize), 1.0f);
    float screenPxDistance = screenPxRange * (msdf - 0.5f);
    float alpha = clamp(screenPxDistance + 0.5f, 0.0f, 1.0f);
    
    float4 color = uniforms.textColor;
    color.a *= alpha;
    return color;
}
