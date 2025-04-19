
Texture2D SceneTexture : register(t100);
Texture2D PP_PostProcessTexture : register(t101);
Texture2D EditorTexture : register(t102);
Texture2D DebugTexture : register(t104);

SamplerState CompositingSampler : register(s0);

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
    if (ViewMode == 7)
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
