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
    texDesc.Width              = static_cast<UINT>(ShadowResolutionScale);
    texDesc.Height             = static_cast<UINT>(ShadowResolutionScale);
    texDesc.MipLevels          = 1;
    texDesc.ArraySize          = 6; // 큐브맵의 6면
    texDesc.Format             = DXGI_FORMAT_R32_TYPELESS;
    texDesc.SampleDesc.Count   = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage              = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags          = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    texDesc.MiscFlags          = D3D11_RESOURCE_MISC_TEXTURECUBE; // <-- 반드시!
    FEngineLoop::GraphicDevice.Device->CreateTexture2D(&texDesc, nullptr, &PointDepthCubeTex);

    for(int i = 0; i < 6; ++i)
    {
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
    HRESULT hr = FEngineLoop::GraphicDevice.Device->CreateShaderResourceView(
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
