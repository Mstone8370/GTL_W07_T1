
#define MAX_LIGHTS 16 

#define MAX_DIRECTIONAL_LIGHT 16
#define MAX_POINT_LIGHT 5
#define MAX_SPOT_LIGHT 16
#define MAX_AMBIENT_LIGHT 16

#define POINT_LIGHT         1
#define SPOT_LIGHT          2
#define DIRECTIONAL_LIGHT   3
#define AMBIENT_LIGHT       4

#define MAX_LIGHT_PER_TILE 256

struct FAmbientLightInfo
{
    float4 AmbientColor;
};

struct FDirectionalLightInfo
{
    float4 LightColor;

    float3 Direction;
    float Intensity;

    row_major matrix ViewMatrix;
    row_major matrix ProjectionMatrix;

    float ShadowMapResolution;
    float3 DirPadding;
};

struct FPointLightInfo
{
    float4 LightColor;

    float3 Position;
    float Radius;
    
    int Type;
    float Intensity;
    float Attenuation;
    float Padding;

    row_major matrix ViewMatrix[6];
    row_major matrix ProjectionMatrix;
};

struct FSpotLightInfo
{
    float4 LightColor;

    float3 Position;
    float Radius;

    float3 Direction;
    float Intensity;

    int Type;
    float InnerRad;
    float OuterRad;
    float Attenuation;

    row_major matrix ViewMatrix;
    row_major matrix ProjectionMatrix;
};

cbuffer Lighting : register(b0)
{
    FAmbientLightInfo Ambient[MAX_AMBIENT_LIGHT];
    FDirectionalLightInfo Directional[MAX_DIRECTIONAL_LIGHT];
    FPointLightInfo PointLights[MAX_POINT_LIGHT];
    FSpotLightInfo SpotLights[MAX_SPOT_LIGHT];
    
    int DirectionalLightsCount;
    int PointLightsCount;
    int SpotLightsCount;
    int AmbientLightsCount;
};

cbuffer TileLightCullSettings : register(b10)
{
    uint2 ScreenSize; // 화면 해상도
    uint2 TileSize; // 한 타일의 크기 (예: 16x16)

    float NearZ; // 카메라 near plane
    float FarZ; // 카메라 far plane

    row_major matrix TileViewMatrix; // View 행렬
    row_major matrix TileProjectionMatrix; // Projection 행렬
    row_major matrix TileInverseProjection; // Projection^-1, 뷰스페이스 복원용

    uint NumLights; // 총 라이트 수
    uint Enable25DCulling; // 1이면 2.5D 컬링 사용
}

struct LightPerTiles
{
    uint NumLights;
    uint Indices[MAX_LIGHT_PER_TILE];
    uint Padding[3];
};
StructuredBuffer<FPointLightInfo> gPointLights : register(t50);
StructuredBuffer<LightPerTiles> gLightPerTiles : register(t60);

// Begin Shadow
SamplerComparisonState ShadowPCF : register(s13);

Texture2D ShadowTexture : register(t12); // directional
Texture2D SpotShadowMap : register(t13);    // spot
TextureCube<float> ShadowMap[MAX_POINT_LIGHT] : register(t14); // point

int GetCubeFaceIndex(float3 dir)
{
    float3 a = abs(dir);
    if (a.x >= a.y && a.x >= a.z) return dir.x > 0 ? 0 : 1;  // +X:0, -X:1
    if (a.y >= a.x && a.y >= a.z) return dir.y > 0 ? 2 : 3;  // +Y:2, -Y:3
    return dir.z > 0 ? 4 : 5;                                // +Z:4, -Z:5
}

static const float SHADOW_BIAS = 0.01;

float GetPointLightShadow(float3 worldPos, uint lightIndex)
{
    float3 dir = normalize(worldPos - PointLights[lightIndex].Position);

    int face = GetCubeFaceIndex(dir);

    // (C) 그 face 에 맞는 뷰·프로젝션으로 깊이 계산
    float4 viewPos = mul( float4(worldPos, 1), PointLights[lightIndex].ViewMatrix[face]);
    float4 clipPos = mul(viewPos, PointLights[lightIndex].ProjectionMatrix);
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

float GetSpotLightShadow(float3 worldPos, uint spotlightIdx, float shadowBias = 0.001 /* 기본 bias */)
{
    // 1) 월드→라이트 클립 공간
    float4 lp = mul(float4(worldPos, 1), SpotLights[spotlightIdx].ViewMatrix);
    lp = mul(lp, SpotLights[spotlightIdx].ProjectionMatrix);

    // 2) NDC→[0,1] uv, 깊이
    float2 uv;
    uv.x = (lp.x / lp.w) * 0.5 + 0.5;
    uv.y = (lp.y / lp.w) * -0.5 + 0.5;
    float depth = lp.z / lp.w;

    // 3) ShadowMap 비교 샘플링
    float s = SpotShadowMap.SampleCmp(ShadowPCF, uv, depth - shadowBias);

    return s;
}

float GetDirectionalLightShadow(float3 WorldPosition)
{
    // Shadow Mapping
    float4 PositionFromLight = float4(WorldPosition, 1.0f);
    PositionFromLight = mul(PositionFromLight, Directional[0].ViewMatrix);
    PositionFromLight = mul(PositionFromLight, Directional[0].ProjectionMatrix);
    PositionFromLight /= PositionFromLight.w;
    float2 shadowUV = {
        0.5f + PositionFromLight.x * 0.5f,
        0.5f - PositionFromLight.y * 0.5f
    };
    float shadowZ = PositionFromLight.z;
    shadowZ -= 0.0005f; // bias

    // Percentage Closer Filtering
    float DepthFromLight = 0.f;
    float PCFOffsetX = 1.f / Directional[0].ShadowMapResolution;
    float PCFOffsetY = 1.f / Directional[0].ShadowMapResolution;
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
                DepthFromLight += ShadowTexture.SampleCmpLevelZero(ShadowPCF, SampleCoord, shadowZ).r;
            }
            else
            {
                DepthFromLight += 1.f;
            }
        }
    }
    DepthFromLight /= 9;

    return (0.05f + DepthFromLight * 0.95f);
}
// End Shadow


float GetDistanceAttenuation(float Distance, float Radius)
{
    float  InvRadius = 1.0 / Radius;
    float  DistSqr = Distance * Distance;
    float  RadiusMask = saturate(1.0 - DistSqr * InvRadius * InvRadius);
    RadiusMask *= RadiusMask;
    
    return RadiusMask / (DistSqr + 1.0);
}

float GetSpotLightAttenuation(float Distance, float Radius, float3 LightDir, float3 SpotDir, float InnerRadius, float OuterRadius)
{
    float DistAtten = GetDistanceAttenuation(Distance, Radius);
    
    float  CosTheta = dot(SpotDir, -LightDir);
    float  SpotMask = saturate((CosTheta - cos(OuterRadius)) / (cos(InnerRadius) - cos(OuterRadius)));
    SpotMask *= SpotMask;
    
    return DistAtten * SpotMask;
}

////////
/// Diffuse
////////
#define PI 3.14159265359

inline float SchlickWeight(float u) // (1‑u)^5
{
    float m = saturate(1.0 - u);
    return m * m * m * m * m;
}

float DisneyDiffuse(float3 N, float3 L, float3 V, float Roughness)
{
    float3 H = normalize(L + V);
    float  NdotL = saturate(dot(N, L));
    float  NdotV = saturate(dot(N, V));
    float  LdotH2 = saturate(dot(L, H)) * saturate(dot(L, H));

    float  Fd90 = 0.5 + 2.0 * Roughness * LdotH2; // grazing boost
    float  Fd = (1.0 + (Fd90 - 1.0) * SchlickWeight(NdotL)) * (1.0 + (Fd90 - 1.0) * SchlickWeight(NdotV));

    return (Fd * NdotL / PI);
}

float LambertDiffuse(float3 N, float3 L)
{
    return saturate(dot(N, L)) / PI;
}

////////
/// Specular
////////
float  D_GGX(float NdotH, float alpha)
{
    float a2 = alpha * alpha;
    float d  = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);                 // Trowbridge‑Reitz
}

float  G_Smith(float NdotV, float NdotL, float alpha)
{
    float k = alpha * 0.5 + 0.0001;           // Schlick‑GGX (≈α/2)
    float gV = NdotV / (NdotV * (1.0 - k) + k);
    float gL = NdotL / (NdotL * (1.0 - k) + k);
    return gV * gL;
}

float3 F_Schlick(float3 F0, float LdotH)
{
    return F0 + (1.0 - F0) * pow(1.0 - LdotH, 5.0);
}

float3 CookTorranceSpecular(
    float3  F0,        // base reflectance (metallic → albedo)
    float   Roughness, // 0 = mirror, 1 = diffuse
    float3  N, float3  V, float3  L)
{
    float3  H      = normalize(V + L);
    float   NdotL  = saturate(dot(N, L));
    float   NdotV  = saturate(dot(N, V));
    float   NdotH  = saturate(dot(N, H));
    float   LdotH  = saturate(dot(L, H));

    float   alpha  = max(0.001, Roughness * Roughness);

    float   D = D_GGX(NdotH, alpha);
    float   G = G_Smith(NdotV, NdotL, alpha);
    float3  F = F_Schlick(F0, LdotH);

    return (D * G * F) * (NdotL / (4.0 * NdotV * NdotL + 1e-5));
}

float3 BRDF(float3 L, float3 V, float3 N, float3 BaseColor, float Metallic, float Roughness)
{
    float3 F0 = lerp(0.04, BaseColor, Metallic);

    float KdScale = 1.0 - Metallic;
    float3 Kd = BaseColor * KdScale;
    
    float3 Diffuse = DisneyDiffuse(N, L, V, Roughness) * Kd;
    float3 Specular = CookTorranceSpecular(F0, Roughness, N, V, L);

    return Diffuse + Specular;
}



////////
/// Calculate Light
////////
float3 PointLight(int Index, float3 WorldPosition, float3 WorldNormal, float3 WorldViewPosition, float3 BaseColor, float Metallic, float Roughness)
{
    // FPointLightInfo LightInfo = gPointLights[Index];
    FPointLightInfo LightInfo = PointLights[Index];

    float3 ToLight = LightInfo.Position - WorldPosition;
    float Distance = length(ToLight);

    float Attenuation = GetDistanceAttenuation(Distance, LightInfo.Radius);
    if (Attenuation <= 0.0)
    {
        return float3(0.0, 0.0, 0.0);
    }
    
    float3 L = normalize(ToLight);
    float3 V = normalize(WorldViewPosition - WorldPosition);
    float3 BRDF_Term = BRDF(L, V, WorldNormal, BaseColor, Metallic, Roughness);
    
    return BRDF_Term * LightInfo.LightColor * LightInfo.Intensity * Attenuation;
}

float3 SpotLight(int Index, float3 WorldPosition, float3 WorldNormal, float3 WorldViewPosition, float3 BaseColor, float Metallic, float Roughness)
{
    FSpotLightInfo LightInfo = SpotLights[Index];

    float3 ToLight = LightInfo.Position - WorldPosition;
    float Distance = length(ToLight);
    float3 LightDir = normalize(ToLight);
    
    float SpotlightFactor = GetSpotLightAttenuation(Distance, LightInfo.Radius, LightDir, normalize(LightInfo.Direction), LightInfo.InnerRad, LightInfo.OuterRad);
    if (SpotlightFactor <= 0.0)
    {
        return float3(0.0, 0.0, 0.0);
    }
    
    float3 L = normalize(ToLight);
    float3 V = normalize(WorldViewPosition - WorldPosition);
    float3 BRDF_Term = BRDF(L, V, WorldNormal, BaseColor, Metallic, Roughness);
    
    return BRDF_Term * LightInfo.LightColor * LightInfo.Intensity * SpotlightFactor;
}

float3 DirectionalLight(int nIndex, float3 WorldPosition, float3 WorldNormal, float3 WorldViewPosition, float3 BaseColor, float Metallic, float Roughness)
{
    FDirectionalLightInfo LightInfo = Directional[nIndex];
    
    float3 L = normalize(-LightInfo.Direction);
    float3 V = normalize(WorldViewPosition - WorldPosition);
    float3 BRDF_Term = BRDF(L, V, WorldNormal, BaseColor, Metallic, Roughness);
    
    return BRDF_Term * LightInfo.LightColor * LightInfo.Intensity;
}

float4 Lighting(float3 WorldPosition, float3 WorldNormal, float3 WorldViewPosition, float3 DiffuseColor, float Metallic, float Roughness, uint TileIndex)
{
    float3 FinalColor = float3(0.0, 0.0, 0.0);
    
    // 현재 타일의 조명 정보 읽기
    LightPerTiles TileLights = gLightPerTiles[TileIndex];
    
    // 다소 비효율적일 수도 있음.
    [unroll(MAX_POINT_LIGHT)]
    for (int i = 0; i < PointLightsCount; i++)
    {
        uint gPointLightIndex = TileLights.Indices[i];
        FinalColor += PointLight(i, WorldPosition, WorldNormal, WorldViewPosition, DiffuseColor, Metallic, Roughness);
    }

    [unroll(MAX_SPOT_LIGHT)]
    for (int j = 0; j < SpotLightsCount; j++)
    {
        FinalColor += SpotLight(j, WorldPosition, WorldNormal, WorldViewPosition, DiffuseColor, Metallic, Roughness);
    }
    [unroll(MAX_DIRECTIONAL_LIGHT)]
    for (int k = 0; k < DirectionalLightsCount; k++)
    {
        FinalColor += DirectionalLight(k, WorldPosition, WorldNormal, WorldViewPosition, DiffuseColor, Metallic, Roughness);
    }
    [unroll(MAX_AMBIENT_LIGHT)]
    for (int l = 0; l < AmbientLightsCount; l++)
    {
        FinalColor += Ambient[l].AmbientColor.rgb;
    }
    
    return float4(FinalColor, 1);
}

float4 Lighting(float3 WorldPosition, float3 WorldNormal, float3 WorldViewPosition, float3 DiffuseColor, float Metallic, float Roughness)
{
    float3 FinalColor = float3(0.0, 0.0, 0.0);

    // 다소 비효율적일 수도 있음.
    [unroll(MAX_POINT_LIGHT)]
    for (int i = 0; i < PointLightsCount; i++)
    {
        float3 Lit = PointLight(i, WorldPosition, WorldNormal, WorldViewPosition, DiffuseColor, Metallic, Roughness);
        float ShadowFactor = 1.0;
#ifndef LIGHTING_MODEL_GOURAUD
        ShadowFactor = GetPointLightShadow(WorldPosition, i);
#endif
        FinalColor += Lit * ShadowFactor;
    }

    [unroll(MAX_SPOT_LIGHT)]
    for (int j = 0; j < SpotLightsCount; j++)
    {
        float3 Lit = SpotLight(j, WorldPosition, WorldNormal, WorldViewPosition, DiffuseColor, Metallic, Roughness);
        float ShadowFactor = 1.0;
#ifndef LIGHTING_MODEL_GOURAUD
        ShadowFactor = GetSpotLightShadow(WorldPosition, j);
#endif
        FinalColor += Lit * ShadowFactor;
    }
    [unroll(MAX_DIRECTIONAL_LIGHT)]
    for (int k = 0; k < DirectionalLightsCount; k++)
    {
        float3 Lit = DirectionalLight(k, WorldPosition, WorldNormal, WorldViewPosition, DiffuseColor, Metallic, Roughness);
#ifndef LIGHTING_MODEL_GOURAUD
        if (k == 0)
        {
            Lit *= GetDirectionalLightShadow(WorldPosition);
        }
#endif
        FinalColor += Lit;
    }
    [unroll(MAX_AMBIENT_LIGHT)]
    for (int l = 0; l < AmbientLightsCount; l++)
    {
        FinalColor += Ambient[l].AmbientColor * DiffuseColor;
    }
    
    return float4(FinalColor, 1);
}
