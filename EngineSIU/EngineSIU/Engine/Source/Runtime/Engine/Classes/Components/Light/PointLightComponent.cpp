#include "PointLightComponent.h"

#include "Math/JungleMath.h"
#include "UObject/Casts.h"

UPointLightComponent::UPointLightComponent()
{
    PointLightInfo.Position = GetWorldLocation();
    PointLightInfo.Radius = 30.f;

    PointLightInfo.LightColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

    PointLightInfo.Intensity = 1000.f;
    PointLightInfo.Type = ELightType::POINT_LIGHT;
    PointLightInfo.Attenuation = 20.0f;
    
    CreateShadowMapResources();
}

UPointLightComponent::~UPointLightComponent()
{
    if (PointShadowSRV)
    {
        PointShadowSRV->Release();
        PointShadowSRV = nullptr;       
    }
    
    for (int i = 0; i < 6; i++)
    {
        if (faceSRVs[i])
        {
            faceSRVs[i]->Release();
            faceSRVs[i] = nullptr;
        }
        if (PointShadowDSV[i])
        {
            PointShadowDSV[i]->Release();
            PointShadowDSV[i] = nullptr;
        }
    }

    if (PointDepthCubeTex)
    {
        PointDepthCubeTex->Release();
        PointDepthCubeTex = nullptr;
    }
}

UObject* UPointLightComponent::Duplicate(UObject* InOuter)
{

    ThisClass* NewComponent = Cast<ThisClass>(Super::Duplicate(InOuter));
    if (NewComponent)
    {
        NewComponent->PointLightInfo = PointLightInfo;
    }
    return NewComponent;
}

void UPointLightComponent::GetProperties(TMap<FString, FString>& OutProperties) const
{
    Super::GetProperties(OutProperties);
    OutProperties.Add(TEXT("Radius"), FString::Printf(TEXT("%f"), PointLightInfo.Radius));
    OutProperties.Add(TEXT("LightColor"), FString::Printf(TEXT("%s"), *PointLightInfo.LightColor.ToString()));
    OutProperties.Add(TEXT("Intensity"), FString::Printf(TEXT("%f"), PointLightInfo.Intensity));
    OutProperties.Add(TEXT("Type"), FString::Printf(TEXT("%d"), PointLightInfo.Type));
    OutProperties.Add(TEXT("Attenuation"), FString::Printf(TEXT("%f"), PointLightInfo.Attenuation));
    OutProperties.Add(TEXT("Position"), FString::Printf(TEXT("%s"), *PointLightInfo.Position.ToString()));
}

void UPointLightComponent::SetProperties(const TMap<FString, FString>& InProperties)
{
    Super::SetProperties(InProperties);
    const FString* TempStr = nullptr;
    TempStr = InProperties.Find(TEXT("Radius"));
    if (TempStr)
    {
        PointLightInfo.Radius = FString::ToFloat(*TempStr);
    }
    TempStr = InProperties.Find(TEXT("LightColor"));
    if (TempStr)
    {
        PointLightInfo.LightColor.InitFromString(*TempStr);
    }
    TempStr = InProperties.Find(TEXT("Intensity"));
    if (TempStr)
    {
        PointLightInfo.Intensity = FString::ToFloat(*TempStr);
    }
    TempStr = InProperties.Find(TEXT("Type"));
    if (TempStr)
    {
        PointLightInfo.Type = FString::ToInt(*TempStr);
    }
    TempStr = InProperties.Find(TEXT("Attenuation"));
    if (TempStr)
    {
        PointLightInfo.Attenuation = FString::ToFloat(*TempStr);
    }
    TempStr = InProperties.Find(TEXT("Position"));
    if (TempStr)
    {
        PointLightInfo.Position.InitFromString(*TempStr);
    }
    
}

const FPointLightInfo& UPointLightComponent::GetPointLightInfo() const
{
    return PointLightInfo;
}

void UPointLightComponent::SetPointLightInfo(const FPointLightInfo& InPointLightInfo)
{
    PointLightInfo = InPointLightInfo;
}


float UPointLightComponent::GetRadius() const
{
    return PointLightInfo.Radius;
}

void UPointLightComponent::SetRadius(float InRadius)
{
    PointLightInfo.Radius = InRadius;
}

FLinearColor UPointLightComponent::GetLightColor() const
{
    return PointLightInfo.LightColor;
}

void UPointLightComponent::SetLightColor(const FLinearColor& InColor)
{
    PointLightInfo.LightColor = InColor;
}


float UPointLightComponent::GetIntensity() const
{
    return PointLightInfo.Intensity;
}

void UPointLightComponent::SetIntensity(float InIntensity)
{
    PointLightInfo.Intensity = InIntensity;
}

int32 UPointLightComponent::GetType() const
{
    return PointLightInfo.Type;
}

void UPointLightComponent::SetType(int InType)
{
    PointLightInfo.Type = InType;
}

void UPointLightComponent::CreateShadowMapResources()
{
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width              =  static_cast<UINT>(ShadowResolutionScale);
    texDesc.Height             = static_cast<UINT>(ShadowResolutionScale);
    texDesc.MipLevels          = 1;
    texDesc.ArraySize          = 6; // 큐브맵의 6면
    texDesc.Format             = DXGI_FORMAT_R32_TYPELESS;
    texDesc.SampleDesc.Count   = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage              = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags          = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    texDesc.MiscFlags          = D3D11_RESOURCE_MISC_TEXTURECUBE; // <-- 반드시!
    HRESULT hr = GEngineLoop.GraphicDevice.Device->CreateTexture2D(&texDesc, nullptr, &PointDepthCubeTex);
    if (FAILED(hr)) {
        MessageBox(NULL, L"Failed to create PointShadowTex", L"Error", MB_OK);
    }
    
    for(int i = 0; i < 6; ++i) {
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format               = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension        = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        dsvDesc.Texture2DArray.FirstArraySlice = i;
        dsvDesc.Texture2DArray.ArraySize       = 1;
        FEngineLoop::GraphicDevice.Device->CreateDepthStencilView(PointDepthCubeTex, &dsvDesc, &PointShadowDSV[i]);
    }

    
    // Texture Cube SRV
    D3D11_SHADER_RESOURCE_VIEW_DESC CubeSrvDesc = {};
    CubeSrvDesc.Format         = DXGI_FORMAT_R32_FLOAT;            // 원본은 R32_TYPELESS → 쉐이더에서 float 로 읽음
    CubeSrvDesc.ViewDimension  = D3D11_SRV_DIMENSION_TEXTURECUBE;  // 큐브맵 SRV
    CubeSrvDesc.TextureCube.MostDetailedMip = 0;  // 가장 상위 MIP 레벨
    CubeSrvDesc.TextureCube.MipLevels       = 1;  // 생성할 때 MipLevels = 1 이므로 1
    hr = FEngineLoop::GraphicDevice.Device->CreateShaderResourceView(
        PointDepthCubeTex, 
        &CubeSrvDesc, 
        &PointShadowSRV
    );

    
    
    if (FAILED(hr))
    {
        MessageBox(GEngineLoop.AppWnd, L"Failed to create PointShadowSRV", L"Error", MB_OK);
    }
    
    for(int face = 0; face < 6; ++face)
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC FaceSrvDesc = {};
        FaceSrvDesc.Format                    = DXGI_FORMAT_R32_FLOAT;              // R32_TYPELESS -> R32_FLOAT
        FaceSrvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        FaceSrvDesc.Texture2DArray.MostDetailedMip = 0;
        FaceSrvDesc.Texture2DArray.MipLevels       = 1;
        FaceSrvDesc.Texture2DArray.FirstArraySlice = face;                           // 각 페이스
        FaceSrvDesc.Texture2DArray.ArraySize       = 1;
        FEngineLoop::GraphicDevice.Device->CreateShaderResourceView(PointDepthCubeTex, &FaceSrvDesc, &faceSRVs[face]);
    }
    // VSM

    D3D11_TEXTURE2D_DESC momentTexDesc = {};
    momentTexDesc.Width         = static_cast<UINT>(ShadowResolutionScale);
    momentTexDesc.Height        = static_cast<UINT>(ShadowResolutionScale);
    momentTexDesc.MipLevels     = 0;                          // ← 0 → 1
    momentTexDesc.ArraySize     = 6;
    momentTexDesc.Format        = DXGI_FORMAT_R32G32_FLOAT;
    momentTexDesc.SampleDesc    = { 1, 0 };
    momentTexDesc.Usage         = D3D11_USAGE_DEFAULT;
    momentTexDesc.BindFlags     = D3D11_BIND_RENDER_TARGET
                                | D3D11_BIND_SHADER_RESOURCE;
    momentTexDesc.MiscFlags     = D3D11_RESOURCE_MISC_TEXTURECUBE
                                | D3D11_RESOURCE_MISC_GENERATE_MIPS;

    hr = GEngineLoop.GraphicDevice.Device->CreateTexture2D(&momentTexDesc, nullptr, &PointMomentCubeTex);
    if (FAILED(hr)) {
        MessageBox(NULL, L"Failed to create Moment Texture", L"Error", MB_OK);
    }
    // 슬라이스별 RTV
    for(int i = 0; i < 6; ++i) {
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format             = DXGI_FORMAT_R32G32_FLOAT;
        rtvDesc.ViewDimension      = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        rtvDesc.Texture2DArray.FirstArraySlice = i;
        rtvDesc.Texture2DArray.ArraySize       = 1;
        hr =GEngineLoop.GraphicDevice.Device->CreateRenderTargetView(
            PointMomentCubeTex, &rtvDesc, &PointMomentRTV[i]);
        if (FAILED(hr)) {
            MessageBox(NULL, L"Failed to Moment RTV", L"Error", MB_OK);
        }
    }

    // 큐브맵 SRV (픽셀 셰이더 샘플링용)
    D3D11_SHADER_RESOURCE_VIEW_DESC momentSrvDesc = {};
    momentSrvDesc.Format              = DXGI_FORMAT_R32G32_FLOAT;
    momentSrvDesc.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURECUBE;
    momentSrvDesc.TextureCube.MostDetailedMip = 0;
    momentSrvDesc.TextureCube.MipLevels       = -1;          // ← -1 → 1

    hr = GEngineLoop.GraphicDevice.Device->CreateShaderResourceView(PointMomentCubeTex, &momentSrvDesc, &PointMomentSRV);

    if (FAILED(hr)) {
        MessageBox(NULL, L"Failed to Moment SRV", L"Error", MB_OK);
    }

    D3D11_SAMPLER_DESC sd = {};
    sd.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;

    sd.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MinLOD         = 0;
    sd.MaxLOD         = D3D11_FLOAT32_MAX;               // ← MaxLOD도 0으로!
    sd.MipLODBias     = 0;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;

    GEngineLoop.GraphicDevice.Device->CreateSamplerState(&sd, &PointShadowVSMSampler);

    // 3) 픽셀 셰이더에 바인딩 (예: t5, s5 사용 시)
    for(int face = 0; face < 6; ++face) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format                    = DXGI_FORMAT_R32G32_FLOAT;  
        srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        srvDesc.Texture2DArray.MostDetailedMip = 0;
        srvDesc.Texture2DArray.MipLevels       = 1;                // ← 1 레벨만
        srvDesc.Texture2DArray.FirstArraySlice = face;
        srvDesc.Texture2DArray.ArraySize       = 1;
        GEngineLoop.GraphicDevice.Device->CreateShaderResourceView(PointMomentCubeTex, &srvDesc, &faceMomentSRVs[face]);
        if (FAILED(hr)) {
            MessageBox(NULL, L"Failed to create SRV", L"Error", MB_OK);
        }
    }
}

void UPointLightComponent::UpdateViewProjMatrix()
{
    for (int i=0; i < 6; i++)
    {
        FVector target = GetWorldLocation() + dirs[i];
        FVector up = ups[i];
        PointLightInfo.ViewMatrix[i] = JungleMath::CreateViewMatrix(GetWorldLocation(),target, up);
    }
    PointLightInfo.ProjectionMatrix = JungleMath::CreateProjectionMatrix(FMath::DegreesToRadians(90.0f), 1.0f, 0.1f, GetRadius());
}
