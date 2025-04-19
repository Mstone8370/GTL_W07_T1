#pragma once
#include "IRenderPass.h"
#include "EngineBaseTypes.h"
#include "Container/Set.h"

#include "Define.h"
#include <d3d11.h>

class FDXDShaderManager;
class FGraphicsDevice;
class FDXDBufferManager;

class USpotLightComponent;
class UPointLightComponent;

struct FLightGPU {
    FVector Position;
    float Radius;
    FVector Direction;
    float Padding; // 16바이트 정렬용
};

struct FSpotLightGPU {
    FVector Position;
    float Radius;
    FVector Direction;
    float InnerRad; // cos(inner angle)
    float OuterRad; // cos(outer angle)
    float Color;
    float Padding; // 16바이트 정렬용
};

struct TileLightCullSettings
{
    UINT ScreenSize[2];
    UINT TileSize[2];

    float NearZ;
    float FarZ;

    FMatrix ViewMatrix;
    FMatrix ProjectionMatrix;
    FMatrix InvProjectionMatrix;

    UINT NumLights;
    UINT Enable25DCulling;
};

class FTileLightCullingPass : public IRenderPass
{
public:
    FTileLightCullingPass();
    ~FTileLightCullingPass();
    virtual void Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManage) override;
    virtual void PrepareRenderArr() override;
    virtual void Render(const std::shared_ptr<FEditorViewportClient>& Viewport) override;
    virtual void ClearRenderArr() override;

    void CreateShader();
    void CreateLightBufferGPU();
    void CreateViews();
    void CreateBuffers();
    void Release();
    void Dispatch(const std::shared_ptr<FEditorViewportClient>& Viewport);
    void ClearUAVs();
    void UpdateTileLightConstantBuffer(const std::shared_ptr<FEditorViewportClient>& Viewport);

    void ResizeViewBuffers();

    // UAV 결과를 파싱하여 타일별 영향을 주는 전역 조명 인덱스로 바꾸는 함수
    bool CopyTileLightMaskToCPU(TArray<uint32>& OutData);
    void ParseTileLightMaskData();
    void PrintLightTilesMapping();

    void SetDepthSRV(ID3D11ShaderResourceView* InDepthSRV) { DepthSRV = InDepthSRV; }
    ID3D11ShaderResourceView*& GetDebugHeatmapSRV() { return DebugHeatmapSRV; }
    ID3D11ShaderResourceView* GetLightStructuredBufferSRV() { return LightSRV; }
    ID3D11Buffer* GetTileConstantBuffer() { return TileLightConstantBuffer; }
    TArray<UPointLightComponent*> GetPointLights() { return PointLights; }
    TArray<TArray<uint32>> GetPointLightPerTiles() { return PointLightPerTiles;  }

    uint32 GetTotalTileCount() { return TILE_COUNT; }

private:
    TArray<USpotLightComponent*> SpotLights;
    TArray<UPointLightComponent*> PointLights;

    TArray<uint32> PointLightMaskData;  // 타일별 조명 마스크 데이터 (n번째 타일에 대한 전역 조명 인덱스 목록 벡터)
    TArray<TArray<uint32>> PointLightPerTiles;

    FDXDBufferManager* BufferManager;
    FGraphicsDevice* Graphics;
    FDXDShaderManager* ShaderManager;

    ID3D11ComputeShader* ComputeShader;         // 컴퓨트 셰이더

    ID3D11Buffer* TileUAVBuffer;
    ID3D11UnorderedAccessView* TileUAV;         // 각 타일별 조명 마스크 ID UAV

    ID3D11Texture2D* DebugHeatmapTexture;       // 디버그용 히트맵 텍스처
    ID3D11UnorderedAccessView* DebugHeatmapUAV; // 디버그용 히트맵 UAV
    ID3D11ShaderResourceView* DebugHeatmapSRV; // 디버그용 히트맵 SRV

    ID3D11Buffer* LightBufferGPU;               // GPU에서 사용할 라이트 버퍼    
    ID3D11ShaderResourceView* LightSRV;         // 라이트 버퍼 SRV (StructruredBuffer)
    
    ID3D11ShaderResourceView* DepthSRV;         // 깊이 버퍼 SRV
    
    ID3D11Buffer* TileLightConstantBuffer;

    const uint32 TILE_SIZE = 32;
    const uint32 MAX_LIGHTS_PER_TILE = 2048;
    
    uint32 TILE_COUNT_X;
    uint32 TILE_COUNT_Y;
    uint32 TILE_COUNT;                          // 타일의 총 개수 (몇개 타일로 나눌지에 따라 결정)
    uint32 SHADER_ENTITY_TILE_BUCKET_COUNT;     // 한 타일이 가질 수 있는 조명 ID를 비트마스크로 표현한 총 슬롯 수

    // 한 타일에 1024개의 라이트 인덱스를 마스크 비트로 저장하려면 : 1024 / sizeof(uint32) = 32
};

