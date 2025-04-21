#include "ShaderRegisters.hlsl"
cbuffer LightMatrix : register(b5)
{
    row_major matrix LightViewMat;
    row_major matrix LightProjectMat;
    float fShadowMapSize;
};

struct VS_OUTPUT_Shadow
{
    float4 Position : SV_POSITION;
};

VS_OUTPUT_Shadow mainVS(VS_INPUT_StaticMesh Input) : SV_POSITION
{
    VS_OUTPUT_Shadow Output;

    Output.Position = float4(Input.Position, 1.0);
    Output.Position = mul(Output.Position, WorldMatrix);
    Output.Position = mul(Output.Position, LightViewMat);
    Output.Position = mul(Output.Position, LightProjectMat);
    
    return Output;
}
