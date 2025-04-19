
Texture2D Texture : register(t0);
SamplerState Sampler : register(s0);

cbuffer SubUVConstant : register(b1)
{
    float2 uvOffset;
    float2 uvScale; // sub UV 셀의 크기 (예: 1/CellsPerColumn, 1/CellsPerRow)
}

struct PS_Input
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD;
};

float4 main(PS_Input input) : SV_TARGET
{
    float4 FinalColor = float4(0.f, 0.f, 0.f, 1.f);
    
    float2 UV = input.UV * uvScale + uvOffset;
    float4 Color = Texture.Sample(Sampler, UV);
    float threshold = 0.1f;

    if (max(max(Color.r, Color.g), Color.b) < threshold || Color.a < 0.5f)
    {
        discard;
    }
    
    FinalColor = Color;
    
    return FinalColor;
}
