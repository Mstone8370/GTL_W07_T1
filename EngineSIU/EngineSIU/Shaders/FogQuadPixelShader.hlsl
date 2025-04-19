
Texture2D<float4> HeatMap : register(t0); // Added : To use heatmap texture

SamplerState Sampler : register(s0);


struct PS_INPUT
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD;
};

float4 mainPS(PS_INPUT input) : SV_Target
{
    float4 FogColor = HeatMap.Sample(Sampler, input.UV);
    FogColor.a = 0.5f;
    
    float3 FinalColor = FogColor.rgb;
    
    return float4(FinalColor.rgb, 1.0);
}
