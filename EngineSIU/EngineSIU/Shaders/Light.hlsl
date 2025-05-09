// TODO: structured buffer로 변경
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

cbuffer LightInfo : register(b0)
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

cbuffer ShowFlagBuffer : register(b5)
{
    uint2 ShowFlag; // ShowFlag가 64비트여서 uint2 사용. ShowFlag를 검사하는 CheckShowFlag함수 사용해야 함.

    float2 ShowFlagPadding;
}

bool CheckShowFlag(uint BitPosition)
{
    if (BitPosition < 32)
    {
        return (ShowFlag.x & (1 << BitPosition)) != 0;
    }
    return (ShowFlag.y & (1 << (BitPosition - 32))) != 0;
}

#define SF_AABB          (0)
#define SF_PRIMITIVES    (1)
#define SF_BillboardText (2)
#define SF_UUIDText      (3)
#define SF_Fog           (4)
#define SF_Shadow        (5)

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
SamplerComparisonState DirectionShadowSampler : register(s12);
SamplerComparisonState SpotShadowSampler : register(s13);
SamplerComparisonState PointShadowSampler : register(s14);
SamplerState VsmSampler : register(s15);

Texture2D ShadowTexture : register(t12); // directional
Texture2DArray<float> SpotShadowArray : register(t13);    // spot
TextureCube<float> ShadowMap[MAX_POINT_LIGHT] : register(t14); // point
TextureCube<float> Momentum[MAX_POINT_LIGHT] : register(t20); // PointMoment
int GetCubeFaceIndex(float3 dir)
{
    float3 a = abs(dir);
    if (a.x >= a.y && a.x >= a.z) return dir.x > 0 ? 0 : 1;  // +X:0, -X:1
    if (a.y >= a.x && a.y >= a.z) return dir.y > 0 ? 2 : 3;  // +Y:2, -Y:3
    return dir.z > 0 ? 4 : 5;                                // +Z:4, -Z:5
}

static const float SHADOW_BIAS = 0.01;
float VSM_Shadow(float2 moments, float depthRef)
{
    float mu       = moments.x;        // E[X]
    float m2       = moments.y;        // E[X²]
    float variance = max(m2 - mu*mu, 0.0);  

    float d        = depthRef - mu;    // 편차
    float p        = variance / (variance + d*d); 

    // depthRef <= mu 면 조명(그림자 아님), 아니면 1-p 만큼 lit 확률
    return (depthRef<= mu + 0.01) ? 1.0 : 1.0 - clamp(p, 0.0, 1.0);
}
float ShadowOcclusionVSM(float3 worldPos, uint lightIndex)
{
    // (A) 라이트 기준 월드 공간 거리
    float3 toLight = worldPos - PointLights[lightIndex].Position;
    float3 dir     = normalize(toLight);    // 큐브맵 샘플용 정규화 방향
    float  dist    = length(toLight);

    // (B) 모멘트 샘플링
    float2 moments = Momentum[lightIndex].Sample(VsmSampler, dir);
    float depthRef = saturate(dist / PointLights[lightIndex].Radius);
    // (C) VSM 계산
    return pow(VSM_Shadow(moments, depthRef),3);
}
float GetPointLightShadow(float3 worldPos, uint lightIndex)
{
    float3 dir = normalize(worldPos - PointLights[lightIndex].Position);

    int face = GetCubeFaceIndex(dir);

    // (C) 그 face 에 맞는 뷰·프로젝션으로 깊이 계산
    float4 viewPos = mul( float4(worldPos, 1), PointLights[lightIndex].ViewMatrix[face]);
    float4 clipPos = mul(viewPos, PointLights[lightIndex].ProjectionMatrix);

    clipPos.xyz /= clipPos.w;

    float2 uv = clipPos.xy * 0.5 + 0.5;

    if (uv.x < 0.0 || uv.x > 1.0 ||
        uv.y < 0.0 || uv.y > 1.0)
    {
        return 1.0;
    }
    float shadow = ShadowMap[lightIndex].SampleCmpLevelZero(
        PointShadowSampler,
        dir,
        clipPos.z
    );
    
    return shadow;
}

float GetSpotLightShadow(float3 worldPos, uint spotlightIdx, float shadowBias = 0.0001)
{
    // 1) 월드→라이트 클립 공간
    float4 lp = mul(float4(worldPos, 1), SpotLights[spotlightIdx].ViewMatrix);
    lp = mul(lp, SpotLights[spotlightIdx].ProjectionMatrix);

    // 2) NDC→[0,1] uv, 깊이
    float2 uv;
    uv.x = (lp.x / lp.w) * 0.5 + 0.5;
    uv.y = (lp.y / lp.w) * -0.5 + 0.5;
    float depth = lp.z / lp.w;

    // 3) Array 텍스쳐로 비교 샘플링
    //    uv.xy: 텍스쳐 좌표, uv.z: array 슬라이스 (spotlightIdx)
    float shadow = SpotShadowArray.SampleCmpLevelZero(
        SpotShadowSampler,
        float3(uv, spotlightIdx),  // uvw
        depth - shadowBias // cmp reference
    );
    
    return shadow;
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
                DepthFromLight += ShadowTexture.SampleCmpLevelZero(DirectionShadowSampler, SampleCoord, shadowZ).r;
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


float CalculateDiffuse(float3 WorldNormal, float3 LightDir)
{
    return max(dot(WorldNormal, LightDir), 0.0);
}

float CalculateSpecular(float3 WorldNormal, float3 ToLightDir, float3 ViewDir, float Shininess, float SpecularStrength = 0.5f)
{
#ifdef LIGHTING_MODEL_GOURAUD
    float3 ReflectDir = reflect(-ToLightDir, WorldNormal);
    float Spec = pow(max(dot(ViewDir, ReflectDir), 0.0), Shininess);
#else
    float3 HalfDir = normalize(ToLightDir + ViewDir); // Blinn-Phong
    float Spec = pow(max(dot(WorldNormal, HalfDir), 0.0), Shininess);
#endif
    return Spec * SpecularStrength;
}



////////
/// Calculate Light
////////
float3 PointLight(int Index, float3 WorldPosition, float3 WorldNormal, float3 WorldViewPosition, float3 DiffuseColor, float3 SpecularColor, float Shininess)
{
    // FPointLightInfo LightInfo = gPointLights[Index];
    FPointLightInfo LightInfo = PointLights[Index];
    
    float3 ToLight = LightInfo.Position - WorldPosition;
    float Distance = length(ToLight);
    
    float Attenuation = GetDistanceAttenuation(Distance, LightInfo.Radius);
    if (Attenuation <= 0.0)
    {
        return float3(0.f, 0.f, 0.f);
    }
    
    float3 LightDir = normalize(ToLight);
    float DiffuseFactor = CalculateDiffuse(WorldNormal, LightDir);

    float3 Lit = (DiffuseFactor * DiffuseColor);
#ifndef LIGHTING_MODEL_LAMBERT
    float3 ViewDir = normalize(WorldViewPosition - WorldPosition);
    float SpecularFactor = CalculateSpecular(WorldNormal, LightDir, ViewDir, Shininess);
    Lit += SpecularFactor * SpecularColor;
#endif
    
    return Lit * Attenuation * LightInfo.Intensity * LightInfo.LightColor;
}

float3 SpotLight(int Index, float3 WorldPosition, float3 WorldNormal, float3 WorldViewPosition, float3 DiffuseColor, float3 SpecularColor, float Shininess)
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
    
    float DiffuseFactor = CalculateDiffuse(WorldNormal, LightDir);
    
    float3 Lit = DiffuseFactor * DiffuseColor;
#ifndef LIGHTING_MODEL_LAMBERT
    float3 ViewDir = normalize(WorldViewPosition - WorldPosition);
    float SpecularFactor = CalculateSpecular(WorldNormal, LightDir, ViewDir, Shininess);
    Lit += SpecularFactor * SpecularColor;
#endif
    
    return Lit * SpotlightFactor * LightInfo.Intensity * LightInfo.LightColor;
}

float3 DirectionalLight(int nIndex, float3 WorldPosition, float3 WorldNormal, float3 WorldViewPosition, float3 DiffuseColor, float3 SpecularColor, float Shininess)
{
    FDirectionalLightInfo LightInfo = Directional[nIndex];
    
    float3 LightDir = normalize(-LightInfo.Direction);
    float3 ViewDir = normalize(WorldViewPosition - WorldPosition);
    float DiffuseFactor = CalculateDiffuse(WorldNormal, LightDir);
    
    float3 Lit = DiffuseFactor * DiffuseColor;
#ifndef LIGHTING_MODEL_LAMBERT
    float SpecularFactor = CalculateSpecular(WorldNormal, LightDir, ViewDir, Shininess);
    Lit += SpecularFactor * SpecularColor;
#endif
    return Lit * LightInfo.Intensity * LightInfo.LightColor;
}

float3 Lighting(float3 WorldPosition, float3 WorldNormal, float3 WorldViewPosition, float3 DiffuseColor, float3 SpecularColor, float Shininess, uint TileIndex)
{
    float3 FinalColor = float3(0.0, 0.0, 0.0);

    // 현재 타일의 조명 정보 읽기
    LightPerTiles TileLights = gLightPerTiles[TileIndex];
    
    // 조명 기여 누적 (예시: 단순히 조명 색상을 더함)
    for (uint i = 0; i < TileLights.NumLights; ++i)
    {
        // tileLights.Indices[i] 는 전역 조명 인덱스
        uint gPointLightIndex = TileLights.Indices[i];
        FinalColor += PointLight(gPointLightIndex, WorldPosition, WorldNormal, WorldViewPosition, DiffuseColor, SpecularColor, Shininess);
    }
    [unroll(MAX_SPOT_LIGHT)]
    for (int j = 0; j < SpotLightsCount; j++)
    {
        FinalColor += SpotLight(j, WorldPosition, WorldNormal, WorldViewPosition, DiffuseColor, SpecularColor, Shininess);
    }
    [unroll(MAX_DIRECTIONAL_LIGHT)]
    for (int k = 0; k < DirectionalLightsCount; k++)
    {
        FinalColor += DirectionalLight(k, WorldPosition, WorldNormal, WorldViewPosition, DiffuseColor, SpecularColor, Shininess);
    }
    [unroll(MAX_AMBIENT_LIGHT)]
    for (int l = 0; l < AmbientLightsCount; l++)
    {
        FinalColor += Ambient[l].AmbientColor.rgb * DiffuseColor;
    }
    
    return FinalColor;
}

float3 Lighting(float3 WorldPosition, float3 WorldNormal, float3 WorldViewPosition, float3 DiffuseColor, float3 SpecularColor, float Shininess)
{
    float3 FinalColor = float3(0.0, 0.0, 0.0);

    // 다소 비효율적일 수도 있음.

    [unroll(MAX_POINT_LIGHT)]
    for (int i = 0; i < PointLightsCount; i++)
    {
        float3 Lit = PointLight(i, WorldPosition, WorldNormal, WorldViewPosition, DiffuseColor, SpecularColor, Shininess);
        float ShadowFactor = 1.0;
#ifndef LIGHTING_MODEL_GOURAUD
        if (CheckShowFlag(SF_Shadow))
        {
        // ShadowFactor = ShadowOcclusionVSM(WorldPosition, i);
            ShadowFactor = GetPointLightShadow(WorldPosition, i);
        }
#endif
        FinalColor += Lit * ShadowFactor;
    }
    
    [unroll(MAX_SPOT_LIGHT)]
    for (int j = 0; j < SpotLightsCount; j++)
    {
        float3 Lit = SpotLight(j, WorldPosition, WorldNormal, WorldViewPosition, DiffuseColor, SpecularColor, Shininess);
        float ShadowFactor = 1.0;
#ifndef LIGHTING_MODEL_GOURAUD
        if (CheckShowFlag(SF_Shadow))
        {
            ShadowFactor = GetSpotLightShadow(WorldPosition, j);
        }
#endif
        FinalColor += Lit * ShadowFactor;
    }
    
    [unroll(MAX_DIRECTIONAL_LIGHT)]
    for (int k = 0; k < DirectionalLightsCount; k++)
    {
        float3 Lit = DirectionalLight(k, WorldPosition, WorldNormal, WorldViewPosition, DiffuseColor, SpecularColor, Shininess);
#ifndef LIGHTING_MODEL_GOURAUD
        if (k == 0 && CheckShowFlag(SF_Shadow))
        {
            Lit *= GetDirectionalLightShadow(WorldPosition);
        }
#endif
        FinalColor += Lit;
    }
    [unroll(MAX_AMBIENT_LIGHT)]
    for (int l = 0; l < AmbientLightsCount; l++)
    {
        FinalColor += Ambient[l].AmbientColor.rgb * DiffuseColor;
    }
    
    return FinalColor;
}
