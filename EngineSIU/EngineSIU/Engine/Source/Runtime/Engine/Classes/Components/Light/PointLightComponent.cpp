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

int UPointLightComponent::GetType() const
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
    texDesc.Width              = ShadowResolutionScale;
    texDesc.Height             = ShadowResolutionScale;
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
        GEngineLoop.GraphicDevice.Device->CreateDepthStencilView(PointDepthCubeTex, &dsvDesc, &PointShadowDSV[i]);
    }
    // Texture Cube SRV
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format         = DXGI_FORMAT_R32_FLOAT;            // 원본은 R32_TYPELESS → 쉐이더에서 float 로 읽음
    srvDesc.ViewDimension  = D3D11_SRV_DIMENSION_TEXTURECUBE;  // 큐브맵 SRV
    srvDesc.TextureCube.MostDetailedMip = 0;  // 가장 상위 MIP 레벨
    srvDesc.TextureCube.MipLevels       = 1;  // 생성할 때 MipLevels = 1 이므로 1
    hr = GEngineLoop.GraphicDevice.Device->CreateShaderResourceView(
        PointDepthCubeTex, 
        &srvDesc, 
        &PointShadowSRV
    );
    if (FAILED(hr)) {
        MessageBox(NULL, L"Failed to create PointShadowSRV", L"Error", MB_OK);
    }
    
    
    for(int face = 0; face < 6; ++face) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format                    = DXGI_FORMAT_R32_FLOAT;              // R32_TYPELESS -> R32_FLOAT
        srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        srvDesc.Texture2DArray.MostDetailedMip = 0;
        srvDesc.Texture2DArray.MipLevels       = 1;
        srvDesc.Texture2DArray.FirstArraySlice = face;                           // 각 페이스
        srvDesc.Texture2DArray.ArraySize       = 1;
        hr = GEngineLoop.GraphicDevice.Device->CreateShaderResourceView(
        PointDepthCubeTex, &srvDesc, &faceSRVs[face]);
        if (FAILED(hr)) {
            MessageBox(NULL, L"Failed to create SRV", L"Error", MB_OK);
        }
    }
    
    D3D11_SAMPLER_DESC comparisonSamplerDesc;
    ZeroMemory(&comparisonSamplerDesc, sizeof(D3D11_SAMPLER_DESC));
    comparisonSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    comparisonSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    comparisonSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    comparisonSamplerDesc.BorderColor[0] = 1.0f;
    comparisonSamplerDesc.BorderColor[1] = 1.0f;
    comparisonSamplerDesc.BorderColor[2] = 1.0f;
    comparisonSamplerDesc.BorderColor[3] = 1.0f;
    comparisonSamplerDesc.MinLOD = 0.f;
    comparisonSamplerDesc.MaxLOD = 0.f;
    comparisonSamplerDesc.MipLODBias = 0.f;
    comparisonSamplerDesc.MaxAnisotropy = 0;
    comparisonSamplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    comparisonSamplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
    hr = GEngineLoop.GraphicDevice.Device->CreateSamplerState(&comparisonSamplerDesc, &PointShadowComparisonSampler);
    if (FAILED(hr)) {
        MessageBox(NULL, L"Failed to create Sampler", L"Error", MB_OK);
    }
    // VSM
    D3D11_TEXTURE2D_DESC momentTexDesc = {};
    texDesc.Width            = ShadowResolutionScale;
    texDesc.Height           = ShadowResolutionScale;
    texDesc.MipLevels        = 0;                         // mipmap 생성할 거면 0
    texDesc.ArraySize        = 6;                         // 큐브맵 6면
    texDesc.Format           = DXGI_FORMAT_R32G32_FLOAT;  // <--- 2채널(1차,2차)
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage            = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags        = D3D11_BIND_RENDER_TARGET   // RTV로도 쓰고
                            | D3D11_BIND_SHADER_RESOURCE; // SRV로도 씀
    // (깊이 전용 뷰는 더 이상 안 만듭니다)
    hr = GEngineLoop.GraphicDevice.Device->CreateTexture2D(&momentTexDesc, nullptr, &PointMomentCubeTex);
    // 슬라이스별 RTV
    for(int i = 0; i < 6; ++i) {
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format             = DXGI_FORMAT_R32G32_FLOAT;
        rtvDesc.ViewDimension      = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        rtvDesc.Texture2DArray.FirstArraySlice = i;
        rtvDesc.Texture2DArray.ArraySize       = 1;
        GEngineLoop.GraphicDevice.Device->CreateRenderTargetView(
            PointMomentCubeTex, &rtvDesc, &PointMomentRTV[i]);
    }

    // 큐브맵 SRV (픽셀 셰이더 샘플링용)
    D3D11_SHADER_RESOURCE_VIEW_DESC momentSrvDesc = {};
    srvDesc.Format               = DXGI_FORMAT_R32G32_FLOAT;
    srvDesc.ViewDimension        = D3D11_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MipLevels = -1;  // 모든 레벨(mipmap) 사용
    GEngineLoop.GraphicDevice.Device->CreateShaderResourceView(
        PointMomentCubeTex, &srvDesc, &PointMomentSRV);
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
