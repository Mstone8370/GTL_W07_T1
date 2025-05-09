#include "LightComponent.h"
#include "UObject/Casts.h"

ULightComponentBase::ULightComponentBase()
{
    AABB.Max = { 1.f,1.f,0.1f };
    AABB.Min = { -1.f,-1.f,-0.1f };
    bCastShadows = true;
}

ULightComponentBase::~ULightComponentBase()
{
  
}

UObject* ULightComponentBase::Duplicate(UObject* InOuter)
{
    ThisClass* NewComponent = Cast<ThisClass>(Super::Duplicate(InOuter));

    NewComponent->AABB = AABB;

    return NewComponent;
}

void ULightComponentBase::GetProperties(TMap<FString, FString>& OutProperties) const
{
    Super::GetProperties(OutProperties);
    OutProperties.Add(TEXT("AABB_Min"), AABB.Min.ToString());
    OutProperties.Add(TEXT("AABB_Max"), AABB.Max.ToString());
}

void ULightComponentBase::SetProperties(const TMap<FString, FString>& InProperties)
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
}

void ULightComponentBase::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);
}

int ULightComponentBase::CheckRayIntersection(FVector& rayOrigin, FVector& rayDirection, float& pfNearHitDistance)
{
    bool res = AABB.Intersect(rayOrigin, rayDirection, pfNearHitDistance);
    return res;
}

void ULightComponentBase::ReleaseShadowDepthMap()
{
}
