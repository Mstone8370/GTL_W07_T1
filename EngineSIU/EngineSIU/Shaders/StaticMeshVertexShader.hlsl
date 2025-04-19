
#include "ShaderRegisters.hlsl"

#ifdef LIGHTING_MODEL_GOURAUD
SamplerState DiffuseSampler : register(s0);

Texture2D DiffuseTexture : register(t0);

cbuffer MaterialConstants : register(b1)
{
    FMaterial Material;
}

#include "Light.hlsl"
#endif


PS_INPUT_StaticMesh mainVS(VS_INPUT_StaticMesh Input)
{
    PS_INPUT_StaticMesh Output;

    Output.Position = float4(Input.Position, 1.0);
    Output.Position = mul(Output.Position, WorldMatrix);
    Output.WorldPosition = Output.Position.xyz;
    
    Output.Position = mul(Output.Position, ViewMatrix);
    Output.Position = mul(Output.Position, ProjectionMatrix);

    // Output.WorldViewPosition = float3(InvViewMatrix._41, InvViewMatrix._42, InvViewMatrix._43);
    
    Output.WorldNormal = mul(Input.Normal, (float3x3)InverseTransposedWorld);

    float3 WorldTangent = mul(Input.Tangent.xyz, (float3x3)WorldMatrix);
    WorldTangent = normalize(WorldTangent);
    WorldTangent = normalize(WorldTangent - Output.WorldNormal * dot(Output.WorldNormal, WorldTangent));

    Output.WorldTangent = float4(WorldTangent, Input.Tangent.w);
    
    Output.UV = Input.UV;
    Output.MaterialIndex = Input.MaterialIndex;

#ifdef LIGHTING_MODEL_GOURAUD
    float4 Diffuse = Lighting(Output.WorldPosition, Output.WorldNormal, ViewWorldLocation, float3(1,1,1));
    Output.Color = float4(Diffuse.rgb, 1.0);
#else
    Output.Color = Input.Color;
#endif
    
    return Output;
}
