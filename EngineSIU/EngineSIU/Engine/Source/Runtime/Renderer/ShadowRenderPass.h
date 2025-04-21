#pragma once
#include "IRenderPass.h"
#include "Define.h"
#include "Renderer/StaticMeshRenderPass.h"
#include "Components/Light/SpotLightComponent.h"

class FShadowRenderPass : public FStaticMeshRenderPass
{
public:
    FShadowRenderPass();
    virtual ~FShadowRenderPass();

    virtual void Initialize(FDXDBufferManager* BufferManager, FGraphicsDevice* Graphics, FDXDShaderManager* ShaderManager) override;
    virtual void PrepareRenderArr() override;
    virtual void Render(const std::shared_ptr<FEditorViewportClient>& Viewport) override;
    virtual void ClearRenderArr() override;
    
    void PrepareRenderState(const std::shared_ptr<FEditorViewportClient>& Viewport);
    void CreateShader();

private:
    TArray<USpotLightComponent*> SpotLights;

    void RenderShadowDepthMesh(UStaticMeshComponent* Mesh);
};
