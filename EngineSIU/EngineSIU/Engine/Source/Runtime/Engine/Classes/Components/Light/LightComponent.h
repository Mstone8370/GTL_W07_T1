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

public:
    FBoundingBox GetBoundingBox() const {return AABB;}

#pragma region PointLight Shadows
public:
    ID3D11Texture2D* PointDepthCubeTex = nullptr;
    ID3D11ShaderResourceView*  PointShadowSRV = NULL;
    ID3D11ShaderResourceView*  faceSRVs[6] = {};
    ID3D11DepthStencilView*    PointShadowDSV[6];
    ID3D11SamplerState*        PointShadowComparisonSampler = NULL;
    ID3D11Buffer*              PointCBLightBuffer = NULL;
    ID3D11RasterizerState*     PointShadowRasterizerState = NULL;
    ID3D11VertexShader*        PointShadowVertexShader = NULL;
    ID3D11VertexShader*        PointShadowInstanceVertexShader = NULL;
    int                        ShadowMapWidth = 1024;
    int                        ShadowMapHeight = 1024;
#pragma endregion
};
