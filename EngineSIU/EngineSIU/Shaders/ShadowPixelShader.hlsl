struct VS_OUTPUT
{
    float4 Position : SV_Position;
    float4 WorldPosition : TEXCOORD0;
};
cbuffer LightConstants : register(b0)
{
    row_major matrix LightViewMat;
    row_major matrix LightProjectMat;
    
    float3 LightPosition;
    float ShadowMapSize;

    float LightRange;
    float3 padding;
};

float4 mainPS(VS_OUTPUT Input) : SV_TARGET
{
    // 1) 월드 좌표
    float3 worldPos = Input.WorldPosition.xyz;

    // 2) 라이트까지 거리
    float  dist  = distance(worldPos, LightPosition);

    // 3) 0~1 정규화
    float  d01   = saturate(dist / LightRange);

    // 4) 모멘트 저장 (R=mean, G=mean²)
    return float4(d01, d01*d01, 0.0f, 0.0f);
}
