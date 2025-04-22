
#ifndef SHADER_REGISTER_INCLUDE
#define SHADER_REGISTER_INCLUDE

float LinearToSRGB(float val)
{
    float low  = 12.92 * val;
    float high = 1.055 * pow(val, 1.0 / 2.4) - 0.055;
    // linear가 임계값보다 큰지 판별 후 선형 보간
    float t = step(0.0031308, val); // linear >= 0.0031308이면 t = 1, 아니면 t = 0
    return lerp(low, high, t);
}

float3 LinearToSRGB(float3 color)
{
    color.r = LinearToSRGB(color.r);
    color.g = LinearToSRGB(color.g);
    color.b = LinearToSRGB(color.b);
    return color;
}

float SRGBToLinear(float val)
{
    float low  = val / 12.92;
    float high = pow((val + 0.055) / 1.055, 2.4);
    float t = step(0.04045, val); // srgb가 0.04045 이상이면 t = 1, 아니면 t = 0
    return lerp(low, high, t);
}

float3 SRGBToLinear(float3 color)
{
    color.r = SRGBToLinear(color.r);
    color.g = SRGBToLinear(color.g);
    color.b = SRGBToLinear(color.b);
    return color;
}

struct FMaterial
{
    uint TextureFlag;
    float3 DiffuseColor;
    
    float3 SpecularColor;
    float Shininess;

    float3 EmissiveColor;
    float Opacity;

    float Metallic;
    float Roughness;
    float2 MaterialPadding;
};

#define TEXTURE_FLAG_DIFFUSE       (1 << 0)
#define TEXTURE_FLAG_SPECULAR      (1 << 1)
#define TEXTURE_FLAG_NORMAL        (1 << 2)
#define TEXTURE_FLAG_EMISSIVE      (1 << 3)
#define TEXTURE_FLAG_ALPHA         (1 << 4)
#define TEXTURE_FLAG_AMBIENT       (1 << 5)
#define TEXTURE_FLAG_SHININESS     (1 << 6)
#define TEXTURE_FLAG_METALLIC      (1 << 7)
#define TEXTURE_FLAG_ROUGHNESS     (1 << 8)

#define TEXTURE_SLOT_DIFFUSE       (0)
#define TEXTURE_SLOT_SPECULAR      (1)
#define TEXTURE_SLOT_NORMAL        (2)
#define TEXTURE_SLOT_EMISSIVE      (3)
#define TEXTURE_SLOT_ALPHA         (4)
#define TEXTURE_SLOT_AMBIENT       (5)
#define TEXTURE_SLOT_SHININESS     (6)
#define TEXTURE_SLOT_METALLIC      (7)
#define TEXTURE_SLOT_ROUGHNESS     (8)

Texture2D MaterialTextures[9] : register(t0);
SamplerState MaterialSamplers[9] : register(s0);

struct VS_INPUT_StaticMesh
{
    float3 Position : POSITION;
    float4 Color : COLOR;
    float3 Normal : NORMAL;
    float4 Tangent : TANGENT;
    float2 UV : TEXCOORD;
    uint MaterialIndex : MATERIAL_INDEX;
};

struct PS_INPUT_StaticMesh
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float4 WorldTangent : TEXCOORD2;
    float3 WorldPosition : TEXCOORD3;
    nointerpolation uint MaterialIndex : MATERIAL_INDEX;
};

////////
/// 공용: 11 ~ 13
///////
cbuffer ViewportSizeBuffer : register(b11)
{
    float2 ViewportSize;
    float2 ViewportPadding;
}

cbuffer ObjectBuffer : register(b12)
{
    row_major matrix WorldMatrix;
    row_major matrix InverseTransposedWorld;
    
    float4 UUID;
    
    bool bIsSelected;
    float3 ObjectPadding;
};

/**
 * 기존에는 View 버퍼와 Projection 버퍼의
 * 업데이트 시기가 달라서 분리했지만, 
 * 여러 뷰포트를 렌더하는 경우 다음 뷰포트로 넘어가면
 * Projection 버퍼를 업데이트해야 하므로,
 * 버퍼를 분리하는 의미가 사라졌으므로 통합.
 */
cbuffer CameraBuffer : register(b13)
{
    row_major matrix ViewMatrix;
    row_major matrix InvViewMatrix;
    
    row_major matrix ProjectionMatrix;
    row_major matrix InvProjectionMatrix;
    
    float3 ViewWorldLocation;
    float ViewPadding;
    
    float NearClip;
    float FarClip;
    float2 ProjectionPadding;
}

#endif
