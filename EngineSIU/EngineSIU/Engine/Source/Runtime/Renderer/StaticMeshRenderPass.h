#pragma once
#include "StaticMeshRenderPassBase.h"

#include "Define.h"
#include "EngineBaseTypes.h"
#include "Components/Light/SpotLightComponent.h"

class UPointLightComponent;
class UWorld;
class UMaterial;
class FEditorViewportClient;
class UStaticMeshComponent;
struct FStaticMaterial;
struct FStaticMeshRenderData;
struct FMatrix;
struct FVector4;
struct ID3D11SamplerState;

class ID3D11Buffer;
class ID3D11VertexShader;
class ID3D11InputLayout;
class ID3D11PixelShader;

class FStaticMeshRenderPass : public FStaticMeshRenderPassBase
{
public:
    FStaticMeshRenderPass();
    virtual ~FStaticMeshRenderPass() override;

protected:
    virtual void CreateResource() override;

    void CreateSamplers();

    virtual void PrepareRenderPass(const std::shared_ptr<FEditorViewportClient>& Viewport) override;

    virtual void CleanUpRenderPass(const std::shared_ptr<FEditorViewportClient>& Viewport) override;

    virtual void PrepareRenderState(const std::shared_ptr<FEditorViewportClient>& Viewport);

    void ChangeViewMode(EViewModeIndex ViewMode);
  
    void UpdateLitUnlitConstant(int32 bIsLit) const;

    void UpdateShadowConstant(const std::shared_ptr<FEditorViewportClient>& Viewport);
    
    // Shadow
    ID3D11SamplerState* PointShadowComparisonSampler = nullptr;
    ID3D11SamplerState* SpotShadowComparisonSampler = nullptr;
    ID3D11SamplerState* DirectionalShadowComparisonSampler = nullptr;
};
