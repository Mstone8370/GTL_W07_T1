
#include "ShaderRegisters.hlsl"

cbuffer FLightConstants: register(b0)
{
    row_major matrix mLightView;
    row_major matrix mLightProj;
}

PS_INPUT_StaticMesh mainVS(VS_INPUT_StaticMesh Input)
{
    PS_INPUT_StaticMesh Output;

    Output.Position = float4(Input.Position, 1.0);
    Output.Position = mul(Output.Position, WorldMatrix);
    Output.WorldPosition = Output.Position.xyz;
    
    Output.Position = mul(Output.Position, mLightView);
    Output.Position = mul(Output.Position, mLightProj);

    Output.WorldViewPosition = float3(InvViewMatrix._41, InvViewMatrix._42, InvViewMatrix._43);
    
    Output.WorldNormal = mul(Input.Normal, (float3x3)InverseTransposedWorld);

    float3 BiTangent = cross(Input.Normal, Input.Tangent);
    matrix<float, 3, 3> TBN = {
        Input.Tangent.x, Input.Tangent.y, Input.Tangent.z,        // column 0
        BiTangent.x, BiTangent.y, BiTangent.z,                    // column 1
        Input.Normal.x, Input.Normal.y, Input.Normal.z            // column 2
    };
    Output.TBN = TBN;
    
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
