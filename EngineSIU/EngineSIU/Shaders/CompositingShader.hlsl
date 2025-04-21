
Texture2D SceneTexture : register(t100);
Texture2D PP_PostProcessTexture : register(t101);
Texture2D EditorTexture : register(t102);
Texture2D DebugTexture : register(t104);

SamplerState CompositingSampler : register(s0);

#define VMI_Lit_Gouraud      0
#define VMI_Lit_Lambert      1
#define VMI_Lit_BlinnPhong   2
#define VMI_Lit_SG           3
#define VMI_Unlit            4
#define VMI_Wireframe        5
#define VMI_SceneDepth       6
#define VMI_WorldNormal      7
#define VMI_WorldTangent     8
#define VMI_LightHeatMap     9

cbuffer ViewMode : register(b0)
{
    uint ViewMode; 
    float3 Padding;
}

struct PS_Input
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD;
};

PS_Input mainVS(uint VertexID : SV_VertexID)
{
    PS_Input Output;

    float2 QuadPositions[6] = {
        float2(-1,  1),  // Top Left
        float2(1,  1),  // Top Right
        float2(-1, -1),  // Bottom Left
        float2(1,  1),  // Top Right
        float2(1, -1),  // Bottom Right
        float2(-1, -1)   // Bottom Left
    };

    float2 UVs[6] = {
        float2(0, 0), float2(1, 0), float2(0, 1),
        float2(1, 0), float2(1, 1), float2(0, 1)
    };

    Output.Position = float4(QuadPositions[VertexID], 0, 1);
    Output.UV = UVs[VertexID];

    return Output;
}

float4 mainPS(PS_Input Input) : SV_TARGET
{
    float4 Scene = SceneTexture.Sample(CompositingSampler, Input.UV);
    float4 PostProcess = PP_PostProcessTexture.Sample(CompositingSampler, Input.UV);
    float4 Editor = EditorTexture.Sample(CompositingSampler, Input.UV);
    float4 Debug = DebugTexture.Sample(CompositingSampler, Input.UV);

    float4 FinalColor = float4(0, 0, 0, 0);
    if (ViewMode == VMI_LightHeatMap)
    {
        FinalColor = lerp(Scene, Debug, 0.5);
        FinalColor = lerp(FinalColor, Editor, Editor.a);
    }
    else
    {
        FinalColor = lerp(Scene, PostProcess, PostProcess.a);
        FinalColor = lerp(FinalColor, Editor, Editor.a);
    }

    return FinalColor;
}
