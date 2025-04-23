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
    float depth = Input.Position.z / Input.Position.w;
    
    return float4(depth, depth * depth,0.0f,0.0f);
    // (A) world-space 거리 (0 ~ LightRange 사이)
    // float dist     = distance(Input.WorldPosition, LightPosition);
    // // float d01     = saturate(dist / 25);  
    //
    // // (B) 1차/2차 모멘트
    // float moment1  = dist;
    // float moment2  = dist * dist;
    //
    // // (C) 4채널 리턴, .xy 에만 기록
    // return float4(moment1, moment2, 0.0f, 0.0f);
}
