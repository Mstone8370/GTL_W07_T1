// ShadowDepthVertexShader.hlsl

#include "ShaderRegisters.hlsl"

cbuffer ShadowConstants : register(b0)
{
    row_major matrix LightViewMatrix;
    row_major matrix LightProjectionMatrix;
};

struct VS_OUTPUT_Shadow
{
    float4 Position : SV_POSITION;
};

VS_OUTPUT_Shadow mainVS(VS_INPUT_StaticMesh input)
{
    VS_OUTPUT_Shadow output;

    output.Position = float4(input.Position, 1.0);
    output.Position = mul(output.Position, WorldMatrix);
    output.Position = mul(output.Position, LightViewMatrix);
    output.Position = mul(output.Position, LightProjectionMatrix);
    return output;
}
