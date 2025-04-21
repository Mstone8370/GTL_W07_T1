#pragma once
#include "LightComponentBase.h"

class ULightComponent : public ULightComponentBase
{
    DECLARE_CLASS(ULightComponent, ULightComponentBase)

public:
    ULightComponent();
    virtual ~ULightComponent() override;
    virtual UObject* Duplicate(UObject* InOuter) override;

    virtual void GetProperties(TMap<FString, FString>& OutProperties) const override;
    virtual void SetProperties(const TMap<FString, FString>& InProperties) override;

    float ShadowResolutionScale;
    float ShadowBias;
    float ShadowSlopeBias;
    float ShadowSharpen;
};

