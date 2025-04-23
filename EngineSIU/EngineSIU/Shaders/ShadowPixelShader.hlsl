struct VS_OUTPUT
{
    float4 Position : SV_Position;
};

float4 mainPS(VS_OUTPUT Input) : SV_TARGET
{
    float depth = Input.Position.z / Input.Position.w;
    
    return float4(depth, depth * depth,0.0f,0.0f);

    // return Input.Position;
}
