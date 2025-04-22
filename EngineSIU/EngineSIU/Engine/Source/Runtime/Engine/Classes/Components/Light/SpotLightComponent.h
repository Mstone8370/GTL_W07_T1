#pragma once
#include "LightComponent.h"

struct FShadowDepthMap
{
    ID3D11Texture2D* Texture2D = nullptr;                // Shadow depth 텍스처
    ID3D11ShaderResourceView* SRV = nullptr;             // 셰이더에서 샘플링할 수 있는 뷰
    ID3D11DepthStencilView* DSV = nullptr;               // 렌더링 타겟에서 사용하는 뷰

    void Release()
    {
        if (Texture2D) { Texture2D->Release(); Texture2D = nullptr; }
        if (SRV) { SRV->Release();       SRV = nullptr; }
        if (DSV) { DSV->Release();       DSV = nullptr; }
    }
};

class USpotLightComponent :public ULightComponentBase
{
    DECLARE_CLASS(USpotLightComponent, ULightComponentBase)
public:
    USpotLightComponent();
    virtual ~USpotLightComponent();
    virtual UObject* Duplicate(UObject* InOuter) override;

    void GetProperties(TMap<FString, FString>& OutProperties) const override;
    void SetProperties(const TMap<FString, FString>& InProperties) override;
    FVector GetDirection();

    const FSpotLightInfo& GetSpotLightInfo() const;
    void SetSpotLightInfo(const FSpotLightInfo& InSpotLightInfo);

    float GetRadius() const;
    void SetRadius(float InRadius);

    FLinearColor GetLightColor() const;
    void SetLightColor(const FLinearColor& InColor);

    float GetIntensity() const;
    void SetIntensity(float InIntensity);

    int GetType() const;
    void SetType(int InType);

    float GetInnerRad() const;
    void SetInnerRad(float InInnerCos);

    float GetOuterRad() const;
    void SetOuterRad(float InOuterCos);

    float GetInnerDegree() const;
    void SetInnerDegree(float InInnerDegree);

    float GetOuterDegree() const;
    void SetOuterDegree(float InOuterDegree);

    FMatrix GetViewMatrix() const;
    FMatrix GetProjectionMatrix() const;

    void SetCastShadows(bool bCastShadows);

    void CreateShadowMapResources() override;
    void ReleaseShadowDepthMap() override;
    
    const FShadowDepthMap& GetShadowDepthMap() const { return ShadowDepthMap; }

private:
    FSpotLightInfo SpotLightInfo;
    FShadowDepthMap ShadowDepthMap;
};

