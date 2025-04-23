
#include "ShaderRegisters.hlsl"

cbuffer LightConstants : register(b0)
{
    row_major matrix LightViewMat;
    row_major matrix LightProjectMat;
    
    float3 LightPosition;
    float ShadowMapSize;

    float LightRange;
    float3 padding;
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float4 WorldPosition : TEXCOORD0;
};

VS_OUTPUT mainVS(VS_INPUT_StaticMesh Input)
{
    VS_OUTPUT Output;

    Output.Position = float4(Input.Position, 1.0);
    Output.Position = mul(Output.Position, WorldMatrix);
    Output.WorldPosition = Output.Position;
    Output.Position = mul(Output.Position, LightViewMat);
    Output.Position = mul(Output.Position, LightProjectMat);
    
    return Output;
}
