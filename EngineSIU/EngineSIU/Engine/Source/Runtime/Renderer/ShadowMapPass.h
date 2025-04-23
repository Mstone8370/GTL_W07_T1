#pragma once
#include "StaticMeshRenderPassBase.h"

#include <d3d11.h>

#include "Math/Vector.h"

class USpotLightComponent;
class UPointLightComponent;
class UDirectionalLightComponent;

class FShadowMapPass : public FStaticMeshRenderPassBase
{
public:
    FShadowMapPass();
    virtual ~FShadowMapPass() override;
    
    virtual void PrepareRenderArr() override;

    virtual void ClearRenderArr() override;

protected:
    virtual void CreateResource() override;
    
    virtual void PrepareRenderPass(const std::shared_ptr<FEditorViewportClient>& Viewport) override;

    virtual void CleanUpRenderPass(const std::shared_ptr<FEditorViewportClient>& Viewport) override;

    virtual void Render_Internal(const std::shared_ptr<FEditorViewportClient>& Viewport) override;
    
    void UpdateLightMatrixConstant(const FMatrix& LightView, const FMatrix& LightProjection, const FVector LightPos,const float ShadowMapSize, const float LightRange);

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
