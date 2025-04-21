
#include "ShaderRegisters.hlsl"

SamplerState DiffuseSampler : register(s0);
SamplerState NormalSampler : register(s1);
SamplerComparisonState ShadowSampler : register(s2);

Texture2D DiffuseTexture : register(t0);
Texture2D NormalTexture : register(t1);
Texture2D ShadowTexture : register(t2);

cbuffer MaterialConstants : register(b1)
{
    FMaterial Material;
}

cbuffer FlagConstants : register(b2)
{
    bool IsLit;
    float3 flagPad0;
}

cbuffer SubMeshConstants : register(b3)
{
    bool IsSelectedSubMesh;
    float3 SubMeshPad0;
}

cbuffer TextureConstants : register(b4)
{
    float2 UVOffset;
    float2 TexturePad0;
}

cbuffer FLightConstants: register(b5)
{
    row_major matrix mLightView;
    row_major matrix mLightProj;
    float fShadowMapSize;
}

#include "Light.hlsl"

SamplerComparisonState ShadowPCF : register(s2);
TextureCube<float> ShadowMap[MAX_POINT_LIGHT] : register(t2);

cbuffer PointLightConstant : register(b5)
{
    row_major matrix viewMatrix[MAX_POINT_LIGHT*6];
    row_major matrix projectionMatrix[MAX_POINT_LIGHT];
}
int GetCubeFaceIndex(float3 dir)
{
    float3 a = abs(dir);
    if (a.x >= a.y && a.x >= a.z) return dir.x > 0 ? 0 : 1;  // +X:0, -X:1
    if (a.y >= a.x && a.y >= a.z) return dir.y > 0 ? 2 : 3;  // +Y:2, -Y:3
    return                dir.z > 0 ? 4 : 5;                // +Z:4, -Z:5
}
static const float SHADOW_BIAS = 0.01;
float ShadowOcclusion(float3 worldPos, uint lightIndex)
{
    float3 dir = normalize(worldPos - PointLights[lightIndex].Position);

    int face = GetCubeFaceIndex(dir);

    // (C) 그 face 에 맞는 뷰·프로젝션으로 깊이 계산
    float4 viewPos = mul( float4(worldPos, 1),viewMatrix[lightIndex * 6 + face]);
    float4 clipPos = mul(viewPos,projectionMatrix[lightIndex]);
    float  depthRef = clipPos.z / clipPos.w - SHADOW_BIAS;

    clipPos.xyz /= clipPos.w;

    float2 uv = clipPos.xy * 0.5 + 0.5;

    if (uv.x < 0.0 || uv.x > 1.0 ||
        uv.y < 0.0 || uv.y > 1.0)
    {
        return 1.0;
    }
    float shadow = ShadowMap[lightIndex].SampleCmpLevelZero(
        ShadowPCF,
        dir,
        clipPos.z - SHADOW_BIAS
    );
    
    return shadow;
}
float4 mainPS(PS_INPUT_StaticMesh Input) : SV_Target
{
    float4 FinalColor = float4(0.f, 0.f, 0.f, 1.f);

    // Diffuse
    float3 DiffuseColor = Material.DiffuseColor;
    if (Material.TextureFlag & (1 << 1))
    {
        DiffuseColor = DiffuseTexture.Sample(DiffuseSampler, Input.UV).rgb;
        DiffuseColor = SRGBToLinear(DiffuseColor);
    }

    // Normal
    float3 WorldNormal = Input.WorldNormal;
    if (Material.TextureFlag & (1 << 2))
    {
        float3 Normal = NormalTexture.Sample(NormalSampler, Input.UV).rgb;
        Normal = normalize(2.f * Normal - 1.f);
        WorldNormal = normalize(mul(mul(Normal, Input.TBN), (float3x3) InverseTransposedWorld));
    }



    // (1) 현재 픽셀이 속한 타일 계산 (input.position = 화면 픽셀좌표계)
    uint2 pixelCoord = uint2(Input.Position.xy);
    uint2 tileCoord = pixelCoord / TileSize; // 각 성분별 나눔
    uint tilesX = ScreenSize.x / TileSize.x; // 한 행에 존재하는 타일 수
    uint flatTileIndex = tileCoord.x + tileCoord.y * tilesX;
    
    // (2) 현재 타일의 조명 정보 읽기
    LightPerTiles tileLights = gLightPerTiles[flatTileIndex];
    
    // 조명 기여 누적 (예시: 단순히 조명 색상을 더함)
    float3 lightingAccum = float3(0, 0, 0);
    for (uint i = 0; i < tileLights.NumLights; ++i)
    {
        // tileLights.Indices[i] 는 전역 조명 인덱스
        uint gPointLightIndex = tileLights.Indices[i];
        //FPointLightInfo light = gPointLights[gPointLightIndex];
        
        float4 lightContribution = PointLight(gPointLightIndex, Input.WorldPosition, 
            normalize(Input.WorldNormal),
            Input.WorldViewPosition, DiffuseColor.rgb
        );

    }
    // lightingAccum += Ambient[0].AmbientColor.rgb;
    float NdotL = saturate(dot(normalize(Input.WorldNormal), normalize(PointLights[0].Position - Input.WorldPosition)));

    
    // Lighting
    if (IsLit)
    {
        // Shadow Mapping
        float4 PositionFromLight = float4(Input.WorldPosition, 1.0f);
        PositionFromLight = mul(PositionFromLight, mLightView);
        PositionFromLight = mul(PositionFromLight, mLightProj);
        PositionFromLight /= PositionFromLight.w;
        float2 shadowUV = {
            0.5f + PositionFromLight.x * 0.5f,
            0.5f - PositionFromLight.y * 0.5f
        };
        float shadowZ = PositionFromLight.z;
        shadowZ -= 0.0005f; // bias

        // Percentage Closer Filtering
        // float DepthFromLight = ShadowTexture.SampleCmpLevelZero(ShadowSampler, shadowUV, shadowZ).r;
        float DepthFromLight = 0.f;
        float PCFOffsetX = 1.f / fShadowMapSize;
        float PCFOffsetY = 1.f / fShadowMapSize;
        for (int i = -1; i <= 1; ++i)
        {
            for (int j = -1; j <= 1; ++j)
            {
                float2 SampleCoord = {
                    shadowUV.x + PCFOffsetX * i,
                    shadowUV.y + PCFOffsetY * j
                };
                if (0.f <= SampleCoord.x && SampleCoord.x <= 1.f && 0.f <= SampleCoord.y && SampleCoord.y <= 1.f)
                {
                    DepthFromLight += ShadowTexture.SampleCmpLevelZero(ShadowSampler, SampleCoord, shadowZ).r;
                }
                else
                {
                    DepthFromLight += 1.f;
                }
            }
        }
        DepthFromLight /= 9;

#ifdef LIGHTING_MODEL_GOURAUD
        FinalColor = float4(Input.Color.rgb * DiffuseColor, 1.0);
#else
        float3 LitColor = Lighting(Input.WorldPosition, WorldNormal, Input.WorldViewPosition, DiffuseColor).rgb;
        // 디버깅용 ---- PointLight 전역 배열에 대한 라이팅 테스팅
        //  LitColor = float3(0, 0, 0);
        //  LitColor += lightingAccum;
        // ------------------------------
        for (int PointlightIndex=0;PointlightIndex< PointLightsCount;++PointlightIndex)
        {
            float shadow = ShadowOcclusion(Input.WorldPosition, PointlightIndex);
            LitColor += LitColor.rgb* shadow;
        }
        FinalColor = float4(LitColor, 1) * (0.05f + DepthFromLight * 0.95f);
#endif
    }
    else
    {
        FinalColor = float4(DiffuseColor, 1);
    }

    
    if (bIsSelected)
    {
        FinalColor += float4(0.01, 0.01, 0.0, 1);
    }

    return FinalColor;
}
