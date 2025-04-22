#pragma once
#include "StaticMeshRenderPass.h"

class UPointLightComponent;
class ULightComponentBase;
class UDirectionalLightComponent;

class FShadowMapPass : public FStaticMeshRenderPass
{
public:
    friend class FRenderer; // 렌더러에서 접근 가능
    friend class DepthBufferDebugPass; // DepthBufferDebugPass에서 접근 가능
public:
    FShadowMapPass();
    ~FShadowMapPass();
    
    virtual void Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManage) override;
    virtual void PrepareRenderArr() override;
    virtual void Render(const std::shared_ptr<FEditorViewportClient>& Viewport) override;
    virtual void ClearRenderArr() override;
    
    void PrepareRenderState(const std::shared_ptr<FEditorViewportClient>& Viewport);
    void UpdateLightMatrixConstant(const FMatrix& LightView, const FMatrix& LightProjection, const float ShadowMapSize);
    void CraeteShadowShader();
    void SetShadowViewports();

private:
    TArray<UDirectionalLightComponent*> DirectionalLightComponents;
    TArray<USpotLightComponent*> SpotLightComponents;
    TArray<UPointLightComponent*> PointLightComponents;

    D3D11_VIEWPORT ShadowViewport;
    D3D11_VIEWPORT DirShadowViewport;
    float ShadowMapSize = 1024.0f;
    float DirShadowMapSize = 4096.0f;
};
