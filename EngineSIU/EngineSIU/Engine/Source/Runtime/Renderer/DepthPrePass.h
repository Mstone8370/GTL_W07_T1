#pragma once

#include "StaticMeshRenderPassBase.h"


class FDepthPrePass : public FStaticMeshRenderPassBase
{
    friend class FRenderer; // 렌더러에서 접근 가능
    friend class DepthBufferDebugPass; // DepthBufferDebugPass에서 접근 가능
    
public:
    FDepthPrePass();
    virtual ~FDepthPrePass() override;

protected:
    virtual void PrepareRenderPass(const std::shared_ptr<FEditorViewportClient>& Viewport) override;

    virtual void CleanUpRenderPass(const std::shared_ptr<FEditorViewportClient>& Viewport) override;

    virtual void Render_Internal(const std::shared_ptr<FEditorViewportClient>& Viewport) override;
};

