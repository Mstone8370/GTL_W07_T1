
#include "ShaderRegisters.hlsl"

SamplerState NormalSampler : register(s1);

Texture2D NormalTexture : register(t1);

cbuffer MaterialConstants : register(b1)
{
    FMaterial Material;
}

float4 mainPS(PS_INPUT_StaticMesh Input) : SV_Target
{
    float3 FinalColor = normalize(Input.WorldTangent.xyz);
    
    FinalColor = (FinalColor + 1.f) / 2.f;
    
    return float4(FinalColor, 1.f);
}
