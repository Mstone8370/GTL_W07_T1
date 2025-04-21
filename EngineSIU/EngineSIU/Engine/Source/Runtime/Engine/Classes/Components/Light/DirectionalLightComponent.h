#pragma once
#include "LightComponent.h"
#include "UObject/ObjectMacros.h"

class UDirectionalLightComponent : public ULightComponentBase
{
    DECLARE_CLASS(UDirectionalLightComponent, ULightComponentBase)

public:
    UDirectionalLightComponent();
    virtual ~UDirectionalLightComponent() override;

    virtual UObject* Duplicate(UObject* InOuter) override;
    
    virtual void GetProperties(TMap<FString, FString>& OutProperties) const override;
    virtual void SetProperties(const TMap<FString, FString>& InProperties) override;
    FVector GetDirection();

    const FDirectionalLightInfo& GetDirectionalLightInfo() const;
    void SetDirectionalLightInfo(const FDirectionalLightInfo& InDirectionalLightInfo);

    float GetIntensity() const;
    void SetIntensity(float InIntensity);

    FLinearColor GetLightColor() const;
    void SetLightColor(const FLinearColor& InColor);

    const float ShadowMapSize = 4096;
    const float ShadowFrustumWidth = 500;
    const float ShadowFrustumHeight = 500;
    const float farPlane = 1000.f;
    const float nearPlane = 0.1f;
    FMatrix GetLightViewMatrix(const FVector& CamPosition);
    FMatrix GetLightProjMatrix();
private:
    FDirectionalLightInfo DirectionalLightInfo;
    virtual void InitializeShadowDepthMap() override;
};

