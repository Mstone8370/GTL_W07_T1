#pragma once

#include "IRenderPass.h"
#include "EngineBaseTypes.h"
#include "Container/Set.h"
#include "Define.h"

#define MAX_POINTLIGHT_PER_TILE 256

class ULightComponent;
class FDXDShaderManager;
class UWorld;
class FEditorViewportClient;

class UPointLightComponent;
class USpotLightComponent;
class UDirectionalLightComponent;
class UAmbientLightComponent;
class UStaticMeshComponent;

struct PointLightPerTile {
    uint32 NumLights;
    uint32 Indices[MAX_POINTLIGHT_PER_TILE];
    uint32 Padding[3];
};

struct FStaticMeshRenderData;

class FUpdateLightBufferPass : public IRenderPass
{
public:
    FUpdateLightBufferPass();
    virtual ~FUpdateLightBufferPass();

    virtual void Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManager) override;
    virtual void PrepareRenderArr() override;
    virtual void Render(const std::shared_ptr<FEditorViewportClient>& Viewport) override;
    virtual void ClearRenderArr() override;
    void UpdateLightBuffer();

    void SetPointLightData(const TArray<UPointLightComponent*>& InPointLights, TArray<TArray<uint32>> InPointLightPerTiles);

    void SetTileConstantBuffer(ID3D11Buffer* InTileConstantBuffer);

    void CreatePointLightBuffer();

    void CreatePointLightPerTilesBuffer();

    void UpdatePointLightBuffer();

    void UpdatePointLightPerTilesBuffer();



private:
    TArray<USpotLightComponent*> SpotLights;
    TArray<UPointLightComponent*> PointLights;
    TArray<UDirectionalLightComponent*> DirectionalLights;
    TArray<UAmbientLightComponent*> AmbientLights;
    TArray<UStaticMeshComponent*> StaticMeshComponents;

    FDXDBufferManager* BufferManager;
    FGraphicsDevice* Graphics;
    FDXDShaderManager* ShaderManager;
    ID3D11VertexShader* VertexShader;
    ID3D11InputLayout* InputLayout;
    D3D11_VIEWPORT ShadowMapViewport;

    TArray<TArray<uint32>> PointLightPerTiles;
    TArray<PointLightPerTile> GPointLightPerTiles;


    ID3D11Buffer* PointLightBuffer;
    ID3D11ShaderResourceView* PointLightSRV;
    
    ID3D11Buffer* PointLightPerTilesBuffer;
    ID3D11ShaderResourceView* PointLightPerTilesSRV;

    ID3D11Buffer* TileConstantBuffer;

    FEditorViewportClient* ViewportClient;
    
    void RenderShadowMap(ULightComponent* InLightComponent);
    void PrepareRenderState(ULightComponent* InLightComponent);
    void RenderPrimitive(FStaticMeshRenderData* RenderData, TArray<FStaticMaterial*> Materials, TArray<UMaterial*> OverrideMaterials, int SelectedSubMeshIndex) const;
    void RenderAllStaticMeshes();
    void UpdateObjectConstant(const FMatrix& WorldMatrix, const FVector4& UUIDColor, bool bIsSelected) const;

    const uint32 MAX_NUM_POINTLIGHTS = 50000;
    const uint32 MAX_TILE = 10000;
};
