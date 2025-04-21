#pragma once
#include "Components/SceneComponent.h"


class ULightComponentBase : public USceneComponent
{
    DECLARE_CLASS(ULightComponentBase, USceneComponent)

public:
    ULightComponentBase();
    virtual ~ULightComponentBase() override;
    virtual UObject* Duplicate(UObject* InOuter) override;
    
    virtual void GetProperties(TMap<FString, FString>& OutProperties) const override;
    virtual void SetProperties(const TMap<FString, FString>& InProperties) override;

    virtual void TickComponent(float DeltaTime) override;
    virtual int CheckRayIntersection(FVector& rayOrigin, FVector& rayDirection, float& pfNearHitDistance) override;

protected:
    FBoundingBox AABB;
    virtual void CreateShadowMapResources() {};
    virtual void ReleaseShadowDepthMap();
    bool bCastShadows;

public:
    bool IsCastShadows() const { return bCastShadows; }
    FBoundingBox GetBoundingBox() const {return AABB;}
};
