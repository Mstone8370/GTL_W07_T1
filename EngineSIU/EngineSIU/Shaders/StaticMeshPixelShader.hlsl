
#include "ShaderRegisters.hlsl"

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

#ifdef LIGHTING_MODEL_SG
#include "LightSG.hlsl"
#else
#include "Light.hlsl"
#endif

float4 mainPS(PS_INPUT_StaticMesh Input) : SV_Target
{
    float4 FinalColor = float4(0.f, 0.f, 0.f, 1.f);

    // Diffuse
    float3 DiffuseColor = Material.DiffuseColor;
    if (Material.TextureFlag & TEXTURE_FLAG_DIFFUSE)
    {
        DiffuseColor = MaterialTextures[TEXTURE_SLOT_DIFFUSE].Sample(MaterialSamplers[TEXTURE_SLOT_DIFFUSE], Input.UV).rgb;
        DiffuseColor = SRGBToLinear(DiffuseColor);
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

    // Specular Color
    float3 SpecularColor = Material.SpecularColor;
    if (Material.TextureFlag & TEXTURE_FLAG_SPECULAR)
    {
        SpecularColor = MaterialTextures[TEXTURE_SLOT_SPECULAR].Sample(MaterialSamplers[TEXTURE_SLOT_SPECULAR], Input.UV).rgb;
        SpecularColor = SRGBToLinear(SpecularColor);
    }

    // Specular Exponent or Glossiness
    float SpecularExponent = Material.SpecularExponent;
    if (Material.TextureFlag & TEXTURE_FLAG_SHININESS)
    {
        SpecularExponent = MaterialTextures[TEXTURE_SLOT_SHININESS].Sample(MaterialSamplers[TEXTURE_SLOT_SHININESS], Input.UV).r;
#ifdef LIGHTING_MODEL_BLINN_PHONG
        SpecularExponent = 1000 * SpecularExponent * SpecularExponent; // y = 1000 * x ^ 2
#endif
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
        
        float3 lightContribution = PointLight(gPointLightIndex, Input.WorldPosition, 
            normalize(Input.WorldNormal),
            ViewWorldLocation, DiffuseColor.rgb,
            SpecularColor, SpecularExponent
        );
        lightingAccum += lightContribution;
    }
    //lightingAccum += Ambient[0].AmbientColor.rgb;
    
    // Lighting
    if (IsLit)
    {
#ifdef LIGHTING_MODEL_GOURAUD
        FinalColor = float4(Input.Color.rgb, 1.0);
#else
        float3 LitColor = Lighting(Input.WorldPosition, WorldNormal, ViewWorldLocation, DiffuseColor, SpecularColor, SpecularExponent);
        FinalColor = float4(LitColor, 1);
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
