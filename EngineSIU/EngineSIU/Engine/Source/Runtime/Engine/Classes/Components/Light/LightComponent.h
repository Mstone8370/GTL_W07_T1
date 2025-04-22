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
    float ShadowResolutionScale = 1024;
    float ShadowBias = 3;
    float ShadowSlopeBias = 3;
    float ShadowSharpen;
    virtual void InitializeShadowDepthMap() {}
    void ReleaseShadowDepthMap();
    
public:
    inline float GetShadowResolutionScale() { return ShadowResolutionScale; }
    inline float GetShadowBias()            { return ShadowBias; }
    inline float GetShadowSlopeBias()       { return ShadowSlopeBias; }
    inline float GetShadowSharpen()         { return ShadowSharpen; }

    // Setter
    inline void  SetShadowResolutionScale(float Value) { ShadowResolutionScale = Value; }
    inline void  SetShadowBias           (float Value) { ShadowBias            = Value; }
    inline void  SetShadowSlopeBias      (float Value) { ShadowSlopeBias       = Value; }
    inline void  SetShadowSharpen        (float Value) { ShadowSharpen         = Value; }

    FBoundingBox GetBoundingBox() const {return AABB;}
    FDepthStencilRHI GetShadowDepthMap() const {return ShadowDepthMap;}
 
};

