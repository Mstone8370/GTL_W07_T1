
#include "ShaderRegisters.hlsl"

SamplerState NormalSampler : register(s1);

Texture2D NormalTexture : register(t1);

cbuffer MaterialConstants : register(b1)
{
    FMaterial Material;
}

float4 mainPS(PS_INPUT_StaticMesh Input) : SV_Target
{
    // Normal
    float3 WorldNormal = Input.WorldNormal;
    if (Material.TextureFlag & (1 << 2))
    {
        float3 Tangent = normalize(Input.WorldTangent.xyz);
        float Sign = Input.WorldTangent.w;
        float3 BiTangent = cross(WorldNormal, Tangent) * Sign;

        float3x3 TBN = float3x3(Tangent, BiTangent, WorldNormal);
        
        float3 Normal = NormalTexture.Sample(NormalSampler, Input.UV).rgb;
        Normal = normalize(2.f * Normal - 1.f);
        WorldNormal = normalize(mul(Normal, TBN));
    }
    
    float4 FinalColor = float4(WorldNormal, 1.f);
    
    FinalColor = (FinalColor + 1.f) / 2.f;
    
    return FinalColor;
}
