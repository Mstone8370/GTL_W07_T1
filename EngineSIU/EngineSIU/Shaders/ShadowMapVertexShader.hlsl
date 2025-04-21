
#include "ShaderRegisters.hlsl"

cbuffer FLightConstants: register(b5)
{
    row_major matrix mLightView;
    row_major matrix mLightProj;
    float fShadowMapSize;
}

PS_INPUT_StaticMesh mainVS(VS_INPUT_StaticMesh Input)
{
    PS_INPUT_StaticMesh Output;

    Output.Position = float4(Input.Position, 1.0);
    Output.Position = mul(Output.Position, WorldMatrix);
    Output.WorldPosition = Output.Position.xyz;
    Output.Position = mul(Output.Position, mLightView);
    Output.Position = mul(Output.Position, mLightProj);
    
    return Output;
}
