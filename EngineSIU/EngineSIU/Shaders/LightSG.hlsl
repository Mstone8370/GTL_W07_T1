
#define MAX_LIGHTS 16 

#define MAX_DIRECTIONAL_LIGHT 16
#define MAX_POINT_LIGHT 16
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

cbuffer TileLightCullSettings : register(b8)
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
StructuredBuffer<FPointLightInfo> gPointLights : register(t10);
StructuredBuffer<LightPerTiles> gLightPerTiles : register(t20);

float CalculateAttenuation(float Distance, float AttenuationFactor, float Radius)
{
    if (Distance > Radius)
    {
        return 0.0;
    }

    float Falloff = 1.0 / (1.0 + AttenuationFactor * Distance * Distance);
    float SmoothFactor = (1.0 - (Distance / Radius)); // 부드러운 falloff

    return Falloff * SmoothFactor;
}

float CalculateSpotEffect(float3 LightDir, float3 SpotDir, float InnerRadius, float OuterRadius, float SpotFalloff)
{
    float Dot = dot(-LightDir, SpotDir); // [-1,1]
    
    float SpotEffect = smoothstep(cos(OuterRadius / 2), cos(InnerRadius / 2), Dot);
    
    return SpotEffect * pow(max(Dot, 0.0), SpotFalloff);
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
    float3  N,
    float3  V,
    float3  L)
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


////////
/// Calculate Light
////////
float3 PointLight(int Index, float3 WorldPosition, float3 WorldNormal, float3 WorldViewPosition, float3 DiffuseColor, float3 SpecularColor, float Glossiness)
{
    FPointLightInfo LightInfo = gPointLights[Index];

    float3 ToLight = LightInfo.Position - WorldPosition;
    float Distance = length(ToLight);

    float Attenuation = CalculateAttenuation(Distance, LightInfo.Attenuation, LightInfo.Radius);
    if (Attenuation <= 0.0)
    {
        return float3(0.0, 0.0, 0.0);
    }
    
    float3 L = normalize(ToLight);
    float3 V = normalize(WorldViewPosition - WorldPosition);
    float3 H = normalize(L + V);

    float Roughness = 1.0 - Glossiness;
    float3 F0 = SpecularColor;

    float KdScale = 1.0 - max(F0.r, max(F0.g, F0.b));
    float3 Kd = DiffuseColor * KdScale;
    
    float3 Diffuse = DisneyDiffuse(WorldNormal, L, V, Roughness) * Kd;
    float3 Specular = CookTorranceSpecular(F0, Roughness, WorldNormal, V, L);

    float3 BRDF_Term = Diffuse + Specular;
    
    return BRDF_Term * LightInfo.LightColor * LightInfo.Intensity;
}

float3 SpotLight(int Index, float3 WorldPosition, float3 WorldNormal, float3 WorldViewPosition, float3 DiffuseColor, float3 SpecularColor, float Glossiness)
{
    FSpotLightInfo LightInfo = SpotLights[Index];

    float3 ToLight = LightInfo.Position - WorldPosition;
    float Distance = length(ToLight);

    float Attenuation = CalculateAttenuation(Distance, LightInfo.Attenuation, LightInfo.Radius);
    if (Attenuation <= 0.0)
    {
        return float3(0.0, 0.0, 0.0);
    }
    
    float3 LightDir = normalize(ToLight);
    float SpotlightFactor = CalculateSpotEffect(LightDir, normalize(LightInfo.Direction), LightInfo.InnerRad, LightInfo.OuterRad, LightInfo.Attenuation);
    if (SpotlightFactor <= 0.0)
    {
        return float3(0.0, 0.0, 0.0);
    }
    
    float3 L = normalize(ToLight);
    float3 V = normalize(WorldViewPosition - WorldPosition);
    float3 H = normalize(L + V);

    float Roughness = 1.0 - Glossiness;
    float3 F0 = SpecularColor;

    float KdScale = 1.0 - max(F0.r, max(F0.g, F0.b));
    float3 Kd = DiffuseColor * KdScale;
    
    float3 Diffuse = DisneyDiffuse(WorldNormal, L, V, Roughness) * Kd;
    float3 Specular = CookTorranceSpecular(F0, Roughness, WorldNormal, V, L);

    float3 BRDF_Term = Diffuse + Specular;
    
    return BRDF_Term * LightInfo.LightColor * LightInfo.Intensity * SpotlightFactor;
}

float3 DirectionalLight(int nIndex, float3 WorldPosition, float3 WorldNormal, float3 WorldViewPosition, float3 DiffuseColor, float3 SpecularColor, float Glossiness)
{
    FDirectionalLightInfo LightInfo = Directional[nIndex];
    
    float3 L = normalize(-LightInfo.Direction);
    float3 V = normalize(WorldViewPosition - WorldPosition);
    float3 H = normalize(L + V);

    float Roughness = 1.0 - Glossiness;
    float3 F0 = SpecularColor;

    float KdScale = 1.0 - max(F0.r, max(F0.g, F0.b));
    float3 Kd = DiffuseColor * KdScale;
    
    float3 Diffuse = DisneyDiffuse(WorldNormal, L, V, Roughness) * Kd;
    float3 Specular = CookTorranceSpecular(F0, Roughness, WorldNormal, V, L);

    float3 BRDF_Term = Diffuse + Specular;
    
    return BRDF_Term * LightInfo.LightColor * LightInfo.Intensity;
}

float4 Lighting(float3 WorldPosition, float3 WorldNormal, float3 WorldViewPosition, float3 DiffuseColor, float3 SpecularColor, float Glossiness, uint TileIndex)
{
    float3 FinalColor = float3(0.0, 0.0, 0.0);

    if (Glossiness > 1.0)
    {
        Glossiness = 0.5;
    }
    
    // 현재 타일의 조명 정보 읽기
    LightPerTiles TileLights = gLightPerTiles[TileIndex];
    
    // 다소 비효율적일 수도 있음.
    [unroll(MAX_POINT_LIGHT)]
    for (int i = 0; i < PointLightsCount; i++)
    {
        uint gPointLightIndex = TileLights.Indices[i];
        FinalColor += PointLight(i, WorldPosition, WorldNormal, WorldViewPosition, DiffuseColor, SpecularColor, Glossiness);
    }

    [unroll(MAX_SPOT_LIGHT)]
    for (int j = 0; j < SpotLightsCount; j++)
    {
        FinalColor += SpotLight(j, WorldPosition, WorldNormal, WorldViewPosition, DiffuseColor, SpecularColor, Glossiness);
    }
    [unroll(MAX_DIRECTIONAL_LIGHT)]
    for (int k = 0; k < DirectionalLightsCount; k++)
    {
        FinalColor += DirectionalLight(k, WorldPosition, WorldNormal, WorldViewPosition, DiffuseColor, SpecularColor, Glossiness);
    }
    [unroll(MAX_AMBIENT_LIGHT)]
    for (int l = 0; l < AmbientLightsCount; l++)
    {
        FinalColor += Ambient[l].AmbientColor.rgb;
    }
    
    return float4(FinalColor, 1);
}

float4 Lighting(float3 WorldPosition, float3 WorldNormal, float3 WorldViewPosition, float3 DiffuseColor, float3 SpecularColor, float Glossiness)
{
    float3 FinalColor = float3(0.0, 0.0, 0.0);

    if (Glossiness > 1.0)
    {
        Glossiness = 0.5;
    }
    
    // 다소 비효율적일 수도 있음.
    [unroll(MAX_POINT_LIGHT)]
    for (int i = 0; i < PointLightsCount; i++)
    {
        FinalColor += PointLight(i, WorldPosition, WorldNormal, WorldViewPosition, DiffuseColor, SpecularColor, Glossiness);
    }

    [unroll(MAX_SPOT_LIGHT)]
    for (int j = 0; j < SpotLightsCount; j++)
    {
        FinalColor += SpotLight(j, WorldPosition, WorldNormal, WorldViewPosition, DiffuseColor, SpecularColor, Glossiness);
    }
    [unroll(MAX_DIRECTIONAL_LIGHT)]
    for (int k = 0; k < DirectionalLightsCount; k++)
    {
        FinalColor += DirectionalLight(k, WorldPosition, WorldNormal, WorldViewPosition, DiffuseColor, SpecularColor, Glossiness);
    }
    [unroll(MAX_AMBIENT_LIGHT)]
    for (int l = 0; l < AmbientLightsCount; l++)
    {
        FinalColor += Ambient[l].AmbientColor.rgb;
    }
    
    return float4(FinalColor, 1);
}
