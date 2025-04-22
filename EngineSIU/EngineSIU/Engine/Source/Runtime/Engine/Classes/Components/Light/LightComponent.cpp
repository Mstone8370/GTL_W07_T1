#include "LightComponent.h"
#include "UObject/Casts.h"

ULightComponent::ULightComponent()
{
    AABB.Max = { 1.f,1.f,0.1f };
    AABB.Min = { -1.f,-1.f,-0.1f };
}


ULightComponent::~ULightComponent()
{
}

UObject* ULightComponent::Duplicate(UObject* InOuter)
{
    ThisClass* NewComponent = Cast<ThisClass>(Super::Duplicate(InOuter));
    return NewComponent;
}

void ULightComponent::GetProperties(TMap<FString, FString>& OutProperties) const
{
    Super::GetProperties(OutProperties);
    OutProperties.Add(TEXT("AABB_Min"), AABB.Min.ToString());
    OutProperties.Add(TEXT("AABB_Max"), AABB.Max.ToString());

    OutProperties.Add(TEXT("ShadowResolutionScale"), FString::Printf(TEXT("%f"), ShadowResolutionScale));
    OutProperties.Add(TEXT("ShadowBias"), FString::Printf(TEXT("%f"), ShadowBias));
    OutProperties.Add(TEXT("ShadowSlopeBias"), FString::Printf(TEXT("%f"), ShadowSlopeBias));
    OutProperties.Add(TEXT("ShadowSharpen"), FString::Printf(TEXT("%f"), ShadowSharpen));
}

void ULightComponent::SetProperties(const TMap<FString, FString>& InProperties)
{
    Super::SetProperties(InProperties);
    const FString* TempStr = nullptr;
    TempStr = InProperties.Find(TEXT("AABB_Min"));
    if (TempStr)
    {
        AABB.Min.InitFromString(*TempStr);
    }
    TempStr = InProperties.Find(TEXT("AABB_Max"));
    if (TempStr)
    {
        AABB.Max.InitFromString(*TempStr);
    }

    if (const FString* Val = InProperties.Find(TEXT("ShadowResolutionScale")))
        ShadowResolutionScale = FCString::Atof(**Val);
    if (const FString* Val = InProperties.Find(TEXT("ShadowBias")))
        ShadowBias = FCString::Atof(**Val);
    if (const FString* Val = InProperties.Find(TEXT("ShadowSlopeBias")))
        ShadowSlopeBias = FCString::Atof(**Val);
    if (const FString* Val = InProperties.Find(TEXT("ShadowSharpen")))
        ShadowSharpen = FCString::Atof(**Val);
}

int ULightComponent::CheckRayIntersection(FVector& rayOrigin, FVector& rayDirection, float& pfNearHitDistance)
{
    bool res = AABB.Intersect(rayOrigin, rayDirection, pfNearHitDistance);
    return res;
}

void ULightComponent::ReleaseShadowDepthMap()
{
    ShadowDepthMap.Release();
}

