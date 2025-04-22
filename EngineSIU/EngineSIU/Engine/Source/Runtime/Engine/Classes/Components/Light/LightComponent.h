#pragma once
#include "LightComponentBase.h"
#include "UnrealClient.h"

class ULightComponent : public ULightComponentBase
{
    DECLARE_CLASS(ULightComponent, ULightComponentBase)

public:
    ULightComponent();
    virtual ~ULightComponent() override;
    virtual UObject* Duplicate(UObject* InOuter) override;

    virtual void GetProperties(TMap<FString, FString>& OutProperties) const override;
    virtual void SetProperties(const TMap<FString, FString>& InProperties) override;

    // virtual void TickComponent(float DeltaTime) override;
    virtual int CheckRayIntersection(FVector& rayOrigin, FVector& rayDirection, float& pfNearHitDistance) override;
    
protected:
    FBoundingBox AABB;
    FDepthStencilRHI ShadowDepthMap;
    virtual void InitializeShadowDepthMap() {}
    void ReleaseShadowDepthMap();
    
public:
    FBoundingBox GetBoundingBox() const {return AABB;}
    FDepthStencilRHI GetShadowDepthMap() const {return ShadowDepthMap;}
    float ShadowResolutionScale = 1024;
    float ShadowBias = 3;
    float ShadowSlopeBias = 3;
    float ShadowSharpen;
};

