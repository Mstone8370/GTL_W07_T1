#include "DirectionalLightComponent.h"
#include "Components/SceneComponent.h"
#include "Math/JungleMath.h"
#include "Math/Rotator.h"
#include "Math/Quat.h"
#include "UObject/Casts.h"

UDirectionalLightComponent::UDirectionalLightComponent()
{
    // DirectionalLight Info
    DirectionalLightInfo.Direction = GetForwardVector();
    DirectionalLightInfo.Intensity = 10.0f;
    DirectionalLightInfo.LightColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

    // DepthStencil for Shadow Mapping
    InitializeShadowDepthMap();
}

UDirectionalLightComponent::~UDirectionalLightComponent()
{
    ReleaseShadowDepthMap();
}

UObject* UDirectionalLightComponent::Duplicate(UObject* InOuter)
{
    ThisClass* NewComponent = Cast<ThisClass>(Super::Duplicate(InOuter));
    if (NewComponent)
    {
        NewComponent->DirectionalLightInfo = DirectionalLightInfo;
    }
    
    return NewComponent;
}

void UDirectionalLightComponent::GetProperties(TMap<FString, FString>& OutProperties) const
{
    Super::GetProperties(OutProperties);
    OutProperties.Add(TEXT("LightColor"), *DirectionalLightInfo.LightColor.ToString());
    OutProperties.Add(TEXT("Intensity"), FString::Printf(TEXT("%f"), DirectionalLightInfo.Intensity));
    OutProperties.Add(TEXT("Direction"), *DirectionalLightInfo.Direction.ToString());
}

void UDirectionalLightComponent::SetProperties(const TMap<FString, FString>& InProperties)
{
    Super::SetProperties(InProperties);
    const FString* TempStr = nullptr;
    TempStr = InProperties.Find(TEXT("LightColor"));
    if (TempStr)
    {
        DirectionalLightInfo.LightColor.InitFromString(*TempStr);
    }
    TempStr = InProperties.Find(TEXT("Intensity"));
    if (TempStr)
    {
        DirectionalLightInfo.Intensity = FString::ToFloat(*TempStr);
    }
    TempStr = InProperties.Find(TEXT("Direction"));
    if (TempStr)
    {
        DirectionalLightInfo.Direction.InitFromString(*TempStr);
    }
}


FVector UDirectionalLightComponent::GetDirection()  
{
    FRotator rotator = GetWorldRotation();
    FVector WorldDown= rotator.ToQuaternion().RotateVector(GetForwardVector());
    return WorldDown;  
}

const FDirectionalLightInfo& UDirectionalLightComponent::GetDirectionalLightInfo() const
{
    return DirectionalLightInfo;
}

void UDirectionalLightComponent::SetDirectionalLightInfo(const FDirectionalLightInfo& InDirectionalLightInfo)
{
    DirectionalLightInfo = InDirectionalLightInfo;
}

float UDirectionalLightComponent::GetIntensity() const
{
    return DirectionalLightInfo.Intensity;
}

void UDirectionalLightComponent::SetIntensity(float InIntensity)
{
    DirectionalLightInfo.Intensity = InIntensity;
}

FLinearColor UDirectionalLightComponent::GetLightColor() const
{
    return DirectionalLightInfo.LightColor;
}

void UDirectionalLightComponent::SetLightColor(const FLinearColor& InColor)
{
    DirectionalLightInfo.LightColor = InColor;
}

FMatrix UDirectionalLightComponent::GetLightViewMatrix(const FVector& CamPosition)
{
    FVector Forward = FMatrix::TransformVector(FVector::ForwardVector, GetRotationMatrix());
    FVector psuedoUp = FVector::UpVector;
    // if Forward similar with UpVector or DownVector
    if (abs(Forward.Dot(psuedoUp) - 1.f) < 1e-6f || abs(Forward.Dot(psuedoUp) + 1.f) < 1e-6f)
    {
        psuedoUp = FVector::ForwardVector;
    }
    FVector Position = CamPosition - GetDirection() * farPlane / 2.f;
    return  JungleMath::CreateViewMatrix(Position, Position + Forward, psuedoUp);
}

FMatrix UDirectionalLightComponent::GetLightProjMatrix()
{
    return JungleMath::CreateOrthoProjectionMatrix(ShadowFrustumWidth, ShadowFrustumHeight, nearPlane, farPlane);
}

void UDirectionalLightComponent::InitializeShadowDepthMap()
{
    ShadowResolutionScale = 4096;
    
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
    shadowMapTextureDesc.Width = static_cast<UINT>(ShadowResolutionScale);
    shadowMapTextureDesc.Height = static_cast<UINT>(ShadowResolutionScale);
    hr = device->CreateTexture2D(&shadowMapTextureDesc, nullptr, &ShadowDepthMap.Texture2D);
    assert(SUCCEEDED(hr));

    D3D11_SHADER_RESOURCE_VIEW_DESC shadowMapSRVDesc = {};
    shadowMapSRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
    shadowMapSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    shadowMapSRVDesc.Texture2D.MipLevels = 1;
    hr = device->CreateShaderResourceView(ShadowDepthMap.Texture2D, &shadowMapSRVDesc, &ShadowDepthMap.SRV);
    assert(SUCCEEDED(hr));

    D3D11_DEPTH_STENCIL_VIEW_DESC shadowMapDSVDesc = {};
    shadowMapDSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
    shadowMapDSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    shadowMapDSVDesc.Texture2D.MipSlice = 0;
    hr = device->CreateDepthStencilView(ShadowDepthMap.Texture2D, &shadowMapDSVDesc, &ShadowDepthMap.DSV);
    assert(SUCCEEDED(hr));
    
}
