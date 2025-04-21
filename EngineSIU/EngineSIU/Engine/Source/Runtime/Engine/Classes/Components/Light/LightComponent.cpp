#include "LightComponent.h"
#include "UObject/Casts.h"

ULightComponent::ULightComponent()
{
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

    OutProperties.Add(TEXT("ShadowResolutionScale"), FString::Printf(TEXT("%f"), ShadowResolutionScale));
    OutProperties.Add(TEXT("ShadowBias"), FString::Printf(TEXT("%f"), ShadowBias));
    OutProperties.Add(TEXT("ShadowSlopeBias"), FString::Printf(TEXT("%f"), ShadowSlopeBias));
    OutProperties.Add(TEXT("ShadowSharpen"), FString::Printf(TEXT("%f"), ShadowSharpen));
}

void ULightComponent::SetProperties(const TMap<FString, FString>& InProperties)
{
    Super::SetProperties(InProperties);

    if (const FString* Val = InProperties.Find(TEXT("ShadowResolutionScale")))
        ShadowResolutionScale = FCString::Atof(**Val);
    if (const FString* Val = InProperties.Find(TEXT("ShadowBias")))
        ShadowBias = FCString::Atof(**Val);
    if (const FString* Val = InProperties.Find(TEXT("ShadowSlopeBias")))
        ShadowSlopeBias = FCString::Atof(**Val);
    if (const FString* Val = InProperties.Find(TEXT("ShadowSharpen")))
        ShadowSharpen = FCString::Atof(**Val);
}
