#pragma once
#include "StaticMeshRenderPass.h"

class UPointLightComponent;
class ULightComponentBase;
class UDirectionalLightComponent;

class FShadowMapPass : public FStaticMeshRenderPass
{
public:
    FShadowMapPass();
    virtual ~FShadowMapPass();
    
    virtual void Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManage) override;
    virtual void PrepareRenderArr() override;
    virtual void Render(const std::shared_ptr<FEditorViewportClient>& Viewport) override;
    virtual void ClearRenderArr() override;
    virtual void PrepareRenderState(const std::shared_ptr<FEditorViewportClient>& Viewport) override;
    
    void UpdateLightMatrixConstant(const FMatrix& LightView, const FMatrix& LightProjection, const float ShadowMapSize);

    virtual void CreateShader() override;
    void SetShadowViewports(float Width = 1024.f, float Height = 1024.f);

private:
    void RenderPointLight(const std::shared_ptr<FEditorViewportClient>& Viewport);
    void RenderSpotLight(const std::shared_ptr<FEditorViewportClient>& Viewport);
    void RenderDirectionalLight(const std::shared_ptr<FEditorViewportClient>& Viewport);
    
    TArray<UDirectionalLightComponent*> DirectionalLightComponents;
    TArray<USpotLightComponent*> SpotLightComponents;
    TArray<UPointLightComponent*> PointLightComponents;

    D3D11_VIEWPORT ShadowViewport;
};
