#pragma once
#include "LightComponent.h"

class UPointLightComponent :public ULightComponent
{

    DECLARE_CLASS(UPointLightComponent, ULightComponent)
public:
    UPointLightComponent();
    virtual ~UPointLightComponent() override;


    virtual UObject* Duplicate(UObject* InOuter) override;
    
    virtual void GetProperties(TMap<FString, FString>& OutProperties) const override;
    virtual void SetProperties(const TMap<FString, FString>& InProperties) override;

    const FPointLightInfo& GetPointLightInfo() const;
    void SetPointLightInfo(const FPointLightInfo& InPointLightInfo);

    float GetRadius() const;
    void SetRadius(float InRadius);

    FLinearColor GetLightColor() const;
    void SetLightColor(const FLinearColor& InColor);


    float GetIntensity() const;
    void SetIntensity(float InIntensity);

    int32 GetType() const;
    void SetType(int InType);

    FMatrix* GetLightViewMatrix() { return PointLightInfo.ViewMatrix;};
    FMatrix GetLightProjectionMatrix() const { return PointLightInfo.ProjectionMatrix;};

private:
    FPointLightInfo PointLightInfo;
#pragma region PointLight Shadows
public:
    ID3D11Texture2D* PointDepthCubeTex = nullptr;
    ID3D11Texture2D* PointMomentCubeTex = nullptr;

    ID3D11ShaderResourceView*  PointShadowSRV = NULL;
    ID3D11ShaderResourceView*  faceSRVs[6] = {};
    ID3D11ShaderResourceView*  faceMomentSRVs[6] = {};
    ID3D11ShaderResourceView*  PointMomentSRV = NULL;
    ID3D11RenderTargetView*    PointMomentRTV[6];
    ID3D11DepthStencilView*    PointShadowDSV[6];
    ID3D11Buffer*              PointCBLightBuffer = nullptr;
    ID3D11RasterizerState*     PointShadowRasterizerState = nullptr;
    ID3D11VertexShader*        PointShadowVertexShader = nullptr;
    ID3D11VertexShader*        PointShadowInstanceVertexShader = nullptr;
    ID3D11SamplerState*        PointShadowVSMSampler = NULL;
#pragma endregion
#pragma region PointShadow
public:
    virtual void CreateShadowMapResources() override;
    
    void UpdateViewProjMatrix();
    
    FVector dirs[6] = {
        { 1,  0,  0}, {-1,  0,  0},
        { 0,  1,  0}, { 0, -1,  0},
        { 0,  0,  1}, { 0,  0, -1}
    };
    
    FVector ups[6] = {
        {0,1,0}, {0,1,0},
        {0,0,-1},{0,0,1},
        {0,1,0}, {0,1,0}
    };
#pragma endregion
};


