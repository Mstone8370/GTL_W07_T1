
#include "ShaderRegisters.hlsl"

SamplerState DiffuseSampler : register(s0);
SamplerState NormalSampler : register(s1);

Texture2D DiffuseTexture : register(t0);
Texture2D NormalTexture : register(t1);

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

#ifdef LIGHTING_MODEL_PBR
#include "LightPBR.hlsl"
#else
#include "Light.hlsl"
#endif

SamplerComparisonState ShadowSampler : register(s12);
SamplerComparisonState ShadowPCF : register(s13);
Texture2D ShadowTexture : register(t12); // directional
TextureCube<float> ShadowMap[MAX_POINT_LIGHT] : register(t13); // point
Texture2D ShadowMap : register(t14);    // spot

cbuffer PointLightConstant : register(b6)
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
    float  depthRef = clipPos.z / clipPos.w;

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
        clipPos.z
    );
    
    return shadow;
}

float ComputeSpotShadow(
    float3 worldPos,
    uint spotlightIdx,
    float shadowBias = 0.002 // 기본 bias
)
{
    // 1) 월드→라이트 클립 공간
    float4 lp = mul(float4(worldPos, 1), LightViewMatrix);
    lp = mul(lp, LightProjectionMatrix);

    // 2) NDC→[0,1] uv, 깊이
    float2 uv;
    uv.x = (lp.x / lp.w) * 0.5 + 0.5;
    uv.y = (lp.y / lp.w) * -0.5 + 0.5;
    float depth = lp.z / lp.w;

    // 3) ShadowMap 비교 샘플링
    float s = ShadowMap.SampleCmp(ShadowSampler, uv, depth - shadowBias);

    return s;
}

float4 mainPS(PS_INPUT_StaticMesh Input) : SV_Target
{
    // Diffuse
    float3 DiffuseColor = Material.DiffuseColor;
    if (Material.TextureFlag & TEXTURE_FLAG_DIFFUSE)
    {
        DiffuseColor = MaterialTextures[TEXTURE_SLOT_DIFFUSE].Sample(MaterialSamplers[TEXTURE_SLOT_DIFFUSE], Input.UV).rgb;
    }

    // Normal
    float3 WorldNormal = normalize(Input.WorldNormal);
    if (Material.TextureFlag & TEXTURE_FLAG_NORMAL)
    {
        float3 Tangent = normalize(Input.WorldTangent.xyz);
        float Sign = Input.WorldTangent.w;
        float3 BiTangent = cross(WorldNormal, Tangent) * Sign;

        float3x3 TBN = float3x3(Tangent, BiTangent, WorldNormal);
        
        float3 Normal = MaterialTextures[TEXTURE_SLOT_NORMAL].Sample(MaterialSamplers[TEXTURE_SLOT_NORMAL], Input.UV).rgb;
        Normal = normalize(2.f * Normal - 1.f);
        WorldNormal = normalize(mul(Normal, TBN));
    }

#ifndef LIGHTING_MODEL_PBR
    // Specular Color
    float3 SpecularColor = Material.SpecularColor;
    if (Material.TextureFlag & TEXTURE_FLAG_SPECULAR)
    {
        SpecularColor = MaterialTextures[TEXTURE_SLOT_SPECULAR].Sample(MaterialSamplers[TEXTURE_SLOT_SPECULAR], Input.UV).rgb;
    }

    // Specular Exponent or Glossiness
    float Shininess = Material.Shininess;
    if (Material.TextureFlag & TEXTURE_FLAG_SHININESS)
    {
        Shininess = MaterialTextures[TEXTURE_SLOT_SHININESS].Sample(MaterialSamplers[TEXTURE_SLOT_SHININESS], Input.UV).r;
        Shininess = 1000 * Shininess * Shininess; // y = 1000 * x ^ 2
    }
#endif

    // Emissive Color
    float3 EmissiveColor = Material.EmissiveColor;
    if (Material.TextureFlag & TEXTURE_FLAG_EMISSIVE)
    {
        EmissiveColor = MaterialTextures[TEXTURE_SLOT_EMISSIVE].Sample(MaterialSamplers[TEXTURE_SLOT_EMISSIVE], Input.UV).rgb;
    }

#ifdef LIGHTING_MODEL_PBR
    // Metallic
    float Metallic = Material.Metallic;
    if (Material.TextureFlag & TEXTURE_FLAG_METALLIC)
    {
        Metallic = MaterialTextures[TEXTURE_SLOT_METALLIC].Sample(MaterialSamplers[TEXTURE_SLOT_METALLIC], Input.UV).r;
    }

    // Roughness
    float Roughness = Material.Roughness;
    if (Material.TextureFlag & TEXTURE_FLAG_ROUGHNESS)
    {
        Roughness = MaterialTextures[TEXTURE_SLOT_ROUGHNESS].Sample(MaterialSamplers[TEXTURE_SLOT_ROUGHNESS], Input.UV).r;
    }
#endif
    
    // Begin for Tile based light culled result
    // 현재 픽셀이 속한 타일 계산 (input.position = 화면 픽셀좌표계)
    uint2 PixelCoord = uint2(Input.Position.xy);
    uint2 TileCoord = PixelCoord / TileSize; // 각 성분별 나눔
    uint TilesX = ScreenSize.x / TileSize.x; // 한 행에 존재하는 타일 수
    uint FlatTileIndex = TileCoord.x + TileCoord.y * TilesX;
    // End for Tile based light culled result

    float4 FinalColor = float4(0.f, 0.f, 0.f, 1.f);
    // Lighting
    if (IsLit)
    {
        float3 LitColor = float3(0, 0, 0);
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
        LitColor = Input.Color.rgb;
#elif defined(LIGHTING_MODEL_PBR)
        LitColor = Lighting(Input.WorldPosition, WorldNormal, ViewWorldLocation, DiffuseColor, Metallic, Roughness);
#else
        LitColor = Lighting(Input.WorldPosition, WorldNormal, ViewWorldLocation, DiffuseColor, SpecularColor, Shininess);
        
        // 디버깅용 ---- PointLight 전역 배열에 대한 라이팅 테스팅
        //LitColor = float3(0, 0, 0);
        //LitColor += lightingAccum;
        // ------------------------------
        
        for (int PointlightIndex=0;PointlightIndex< PointLightsCount; ++PointlightIndex)
        {
            float shadow = ShadowOcclusion(Input.WorldPosition, PointlightIndex);
            LitColor += LitColor.rgb * shadow;
        }

        if (DirectionalLightsCount > 0)
            FinalColor = float4(LitColor, 1) * (0.05f + DepthFromLight * 0.95f);
        else
            FinalColor = float4(LitColor, 1);

        for (int i = 0; i < SpotLightsCount; i++)
        {
            float shadowFactor = ComputeSpotShadow(Input.WorldPosition, i);
            LitColor += LitColor.rgb * shadowFactor;
        }
            
        FinalColor = float4(LitColor, 1);
#endif
        LitColor += EmissiveColor * 5.f; // 5는 임의의 값
        // FinalColor = float4(LitColor, 1);
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
