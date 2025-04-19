#pragma once
#include <memory>

#include "HAL/PlatformType.h"

class FViewportResource;
class UWorld;
class FDXDBufferManager;
class FGraphicsDevice;
class FDXDShaderManager;
class FEditorViewportClient;

class IRenderPass {
public:
    virtual ~IRenderPass() {}
    
    virtual void Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManage) = 0;

    virtual void PrepareRenderArr() = 0;
    
    virtual void Render(const std::shared_ptr<FEditorViewportClient>& Viewport) = 0;

    virtual void ClearRenderArr() = 0;
};
