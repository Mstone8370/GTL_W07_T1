#include "SpotLightComponent.h"
#include "Math/Rotator.h"
#include "Math/Quat.h"
#include "UObject/Casts.h"
#include "Math/JungleMath.h"
#include "GameFramework/Actor.h"
USpotLightComponent::USpotLightComponent()
{
    // SpotLightInfo
    SpotLightInfo.Position = GetWorldLocation();
    SpotLightInfo.Radius = 30.0f;
    SpotLightInfo.Direction = GetForwardVector();
    SpotLightInfo.LightColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
    SpotLightInfo.Intensity = 1000.0f;
    SpotLightInfo.Type = ELightType::SPOT_LIGHT;
    SpotLightInfo.InnerRad = 0.2618;
    SpotLightInfo.OuterRad = 0.5236;
    SpotLightInfo.Attenuation = 20.0f;

    // DepthStencil for Shadow Mapping
    CreateShadowMapResources();
    
}

USpotLightComponent::~USpotLightComponent()
{
    ReleaseShadowDepthMap();
}

UObject* USpotLightComponent::Duplicate(UObject* InOuter)
{
    ThisClass* NewComponent = Cast<ThisClass>(Super::Duplicate(InOuter));
    if (NewComponent)
    {
        NewComponent->SpotLightInfo = SpotLightInfo;
    }
    
    return NewComponent;
}

void USpotLightComponent::CreateShadowMapResources()
{
    ShadowResolutionScale = 2048;

    if (ShadowDepthMap.Texture2D || ShadowDepthMap.SRV || ShadowDepthMap.DSV)
        return;

    ID3D11Device* device = FEngineLoop::Renderer.Graphics->Device;
    HRESULT hr;
    
    D3D11_TEXTURE2D_DESC shadowMapTextureDesc = {};
    shadowMapTextureDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    shadowMapTextureDesc.MipLevels = 0;
    shadowMapTextureDesc.ArraySize = 1;
    shadowMapTextureDesc.Usage = D3D11_USAGE_DEFAULT;
    shadowMapTextureDesc.SampleDesc.Count = 1;
    shadowMapTextureDesc.SampleDesc.Quality = 0;
    shadowMapTextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;
    shadowMapTextureDesc.Width = ShadowResolutionScale;
    shadowMapTextureDesc.Height = ShadowResolutionScale;
    hr = device->CreateTexture2D(&shadowMapTextureDesc, nullptr, &ShadowDepthMap.Texture2D);
    if (FAILED(hr))
    {
        std::wstring message = L"[SpotLight] CreateTexture2D failed: HRESULT = 0x" + std::to_wstring(hr) + L"\n";
        OutputDebugStringW(message.c_str());
        return;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC shadowMapSRVDesc = {};
    shadowMapSRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
    shadowMapSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    shadowMapSRVDesc.Texture2D.MipLevels = 1;
    hr = device->CreateShaderResourceView(ShadowDepthMap.Texture2D, &shadowMapSRVDesc, &ShadowDepthMap.SRV);
    if (FAILED(hr))
    {
        std::wstring message = L"[SpotLight] CreateShaderResourceView failed: HRESULT = 0x" + std::to_wstring(hr) + L"\n";
        OutputDebugStringW(message.c_str());
        return;
    }
    
    D3D11_DEPTH_STENCIL_VIEW_DESC shadowMapDSVDesc = {};
    shadowMapDSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
    shadowMapDSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    shadowMapDSVDesc.Texture2D.MipSlice = 0;
    hr = device->CreateDepthStencilView(ShadowDepthMap.Texture2D, &shadowMapDSVDesc, &ShadowDepthMap.DSV);
    if (FAILED(hr))
    {
        std::wstring message = L"[SpotLight] CreateDepthStencilView failed: HRESULT = 0x" + std::to_wstring(hr) + L"\n";
        OutputDebugStringW(message.c_str());
        return;
    }
}

void USpotLightComponent::ReleaseShadowDepthMap()
{
    ShadowDepthMap.Release();
}

void USpotLightComponent::GetProperties(TMap<FString, FString>& OutProperties) const
{
    Super::GetProperties(OutProperties);
    OutProperties.Add(TEXT("Position"), FString::Printf(TEXT("%s"), *SpotLightInfo.Position.ToString()));
    OutProperties.Add(TEXT("Radius"), FString::Printf(TEXT("%f"), SpotLightInfo.Radius));
    OutProperties.Add(TEXT("Direction"), FString::Printf(TEXT("%s"), *SpotLightInfo.Direction.ToString()));
    OutProperties.Add(TEXT("LightColor"), FString::Printf(TEXT("%s"), *SpotLightInfo.LightColor.ToString()));
    OutProperties.Add(TEXT("Intensity"), FString::Printf(TEXT("%f"), SpotLightInfo.Intensity));
    OutProperties.Add(TEXT("Type"), FString::Printf(TEXT("%d"), SpotLightInfo.Type));
    OutProperties.Add(TEXT("InnerRad"), FString::Printf(TEXT("%f"), SpotLightInfo.InnerRad));
    OutProperties.Add(TEXT("OuterRad"), FString::Printf(TEXT("%f"), SpotLightInfo.OuterRad));
    OutProperties.Add(TEXT("Attenuation"), FString::Printf(TEXT("%f"), SpotLightInfo.Attenuation));
    
}

void USpotLightComponent::SetProperties(const TMap<FString, FString>& InProperties)
{
    Super::SetProperties(InProperties);
    const FString* TempStr = nullptr;
    TempStr = InProperties.Find(TEXT("Position"));
    if (TempStr)
    {
        SpotLightInfo.Position.InitFromString(*TempStr);
    }
    TempStr = InProperties.Find(TEXT("Radius"));
    if (TempStr)
    {
        SpotLightInfo.Radius = FString::ToFloat(*TempStr);
    }
    TempStr = InProperties.Find(TEXT("Direction"));
    if (TempStr)
    {
        SpotLightInfo.Direction.InitFromString(*TempStr);
    }
    TempStr = InProperties.Find(TEXT("LightColor"));
    if (TempStr)
    {
        SpotLightInfo.LightColor.InitFromString(*TempStr);
    }
    TempStr = InProperties.Find(TEXT("Intensity"));
    if (TempStr)
    {
        SpotLightInfo.Intensity = FString::ToFloat(*TempStr);
    }
    TempStr = InProperties.Find(TEXT("Type"));
    if (TempStr)
    {
        SpotLightInfo.Type = FString::ToInt(*TempStr);
    }
    TempStr = InProperties.Find(TEXT("InnerRad"));
    if (TempStr)
    {
        SpotLightInfo.InnerRad = FString::ToFloat(*TempStr);
    }
    TempStr = InProperties.Find(TEXT("OuterRad"));
    if (TempStr)
    {
        SpotLightInfo.OuterRad = FString::ToFloat(*TempStr);
    }
    TempStr = InProperties.Find(TEXT("Attenuation"));
    if (TempStr)
    {
        SpotLightInfo.Attenuation = FString::ToFloat(*TempStr);
    }
}

FVector USpotLightComponent::GetDirection()
{
    FRotator rotator = GetWorldRotation();
    FVector WorldForward = rotator.ToQuaternion().RotateVector(GetForwardVector());
    return WorldForward;
}

const FSpotLightInfo& USpotLightComponent::GetSpotLightInfo() const
{
    return SpotLightInfo;
}

void USpotLightComponent::SetSpotLightInfo(const FSpotLightInfo& InSpotLightInfo)
{
    SpotLightInfo = InSpotLightInfo;
}

float USpotLightComponent::GetRadius() const
{
    return SpotLightInfo.Radius;
}

void USpotLightComponent::SetRadius(float InRadius)
{
    SpotLightInfo.Radius = InRadius;
}

FLinearColor USpotLightComponent::GetLightColor() const
{
    return SpotLightInfo.LightColor;
}

void USpotLightComponent::SetLightColor(const FLinearColor& InColor)
{
    SpotLightInfo.LightColor = InColor;
}

float USpotLightComponent::GetIntensity() const
{
    return SpotLightInfo.Intensity;
}

void USpotLightComponent::SetIntensity(float InIntensity)
{
    SpotLightInfo.Intensity = InIntensity;
}

int USpotLightComponent::GetType() const
{
    return SpotLightInfo.Type;
}

void USpotLightComponent::SetType(int InType)
{
    SpotLightInfo.Type = InType;
}

float USpotLightComponent::GetInnerRad() const
{
    return SpotLightInfo.InnerRad;
}

void USpotLightComponent::SetInnerRad(float InInnerCos)
{
    SpotLightInfo.InnerRad = InInnerCos;
}

float USpotLightComponent::GetOuterRad() const
{
    return SpotLightInfo.OuterRad;
}

void USpotLightComponent::SetOuterRad(float InOuterCos)
{
    SpotLightInfo.OuterRad = InOuterCos;
}

float USpotLightComponent::GetInnerDegree() const
{
    return FMath::RadiansToDegrees(SpotLightInfo.InnerRad);
}

void USpotLightComponent::SetInnerDegree(float InInnerDegree)
{
    SpotLightInfo.InnerRad = FMath::DegreesToRadians(FMath::Max(InInnerDegree, 0.f));
    SpotLightInfo.OuterRad = FMath::Clamp(SpotLightInfo.OuterRad, SpotLightInfo.InnerRad, FMath::DegreesToRadians(80.0f));
}   

float USpotLightComponent::GetOuterDegree() const
{
    return FMath::RadiansToDegrees(SpotLightInfo.OuterRad);
}

void USpotLightComponent::SetOuterDegree(float InOuterDegree)
{
    SpotLightInfo.OuterRad = FMath::DegreesToRadians(FMath::Max(InOuterDegree, 0.f));
    SpotLightInfo.InnerRad = FMath::Clamp(SpotLightInfo.InnerRad, 0.0f, SpotLightInfo.OuterRad);
}

FMatrix USpotLightComponent::GetLightViewMatrix()
{
    FVector Eye = GetWorldLocation();
    FVector At = Eye + GetOwner()->GetActorForwardVector();
    FVector Up = FVector(0.0f, 0.0f, 1.0f);

    SpotLightInfo.ViewMatrix = JungleMath::CreateViewMatrix(Eye, At, Up);
    return SpotLightInfo.ViewMatrix;
}

FMatrix USpotLightComponent::GetLightProjectionMatrix() const
{
    float FOVRad = SpotLightInfo.OuterRad * 2.0f;
    float AspectRatio = 1.0f; 
    float NearZ = 0.1f;
    float FarZ = SpotLightInfo.Radius;

    return JungleMath::CreateProjectionMatrix(FOVRad, AspectRatio, NearZ, FarZ);
}

void USpotLightComponent::SetCastShadows(bool bEnable)
{
    bCastShadows = bEnable;
    if (bCastShadows)
        CreateShadowMapResources();
    else
        ReleaseShadowDepthMap();
}
