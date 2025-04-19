#include "TileLightCullingPass.h"
#include "D3D11RHI/DXDBufferManager.h"
#include "D3D11RHI/GraphicDevice.h"
#include "D3D11RHI/DXDShaderManager.h"

//#include "World/World.h"
#include "UnrealClient.h"
#include "UObject/Casts.h"
#include "Engine/EditorEngine.h"
#include "Components/Light/LightComponent.h"
#include "Components/Light/PointLightComponent.h"
#include "Components/Light/SpotLightComponent.h"
#include "UObject/UObjectIterator.h"

#include "UnrealEd/EditorViewportClient.h"

#define SAFE_RELEASE(p) if (p) { p->Release(); p = nullptr; }

#define _PRINTDEBUG FALSE

FTileLightCullingPass::FTileLightCullingPass()
{
}

FTileLightCullingPass::~FTileLightCullingPass()
{
}

void FTileLightCullingPass::Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManage)
{
    BufferManager = InBufferManager;
    Graphics = InGraphics;
    ShaderManager = InShaderManage;

    TILE_COUNT_X = (Graphics->ScreenWidth + TILE_SIZE - 1) / TILE_SIZE;
    TILE_COUNT_Y = (Graphics->ScreenHeight + TILE_SIZE - 1) / TILE_SIZE;
    TILE_COUNT = TILE_COUNT_X * TILE_COUNT_Y;
    SHADER_ENTITY_TILE_BUCKET_COUNT = MAX_LIGHTS_PER_TILE / 32;
    // 한 타일이 가질 수 있는 조명 ID 목록을 비트마스크로 표현한 총 슬롯 수

    CreateShader();
    CreateViews();
    CreateBuffers();
}

void FTileLightCullingPass::PrepareRenderArr()
{
    for (const auto iter : TObjectRange<ULightComponentBase>())
    {
        if (iter->GetWorld() == GEngine->ActiveWorld)
        {
            if (UPointLightComponent* PointLight = Cast<UPointLightComponent>(iter))
            {
                PointLights.Add(PointLight);
            }
            /*else if (USpotLightComponent* SpotLight = Cast<USpotLightComponent>(iter))
            {
                SpotLights.Add(SpotLight);
            }*/
        }
    }
    CreateLightBufferGPU();
    //ClearUAVs();
    
}

void FTileLightCullingPass::Render(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    DepthSRV = Viewport->GetViewportResource()->GetDepthStencil(
        EResourceType::ERT_Debug
    )->SRV;
    ComputeShader = ShaderManager->GetComputeShaderByKey(L"TileLightCullingComputeShader");
    UpdateTileLightConstantBuffer(Viewport);
    Dispatch(Viewport);

    ParseTileLightMaskData();
}

void FTileLightCullingPass::Dispatch(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    // 한 스레드 그룹(groupSizeX, groupSizeY)은 16x16픽셀 영역처리
    const UINT groupSizeX = (Graphics->ScreenWidth  + TILE_SIZE - 1) / TILE_SIZE;
    const UINT groupSizeY = (Graphics->ScreenHeight + TILE_SIZE - 1) / TILE_SIZE; 

    Graphics->DeviceContext->CSSetConstantBuffers(0, 1, &TileLightConstantBuffer);

    // 1. SRV (전역 Light 정보) 바인딩
    if (LightSRV)
    {
        Graphics->DeviceContext->CSSetShaderResources(0, 1, &LightSRV);                  // register(t0)
    }
    if (DepthSRV)
    {
        Graphics->DeviceContext->CSSetShaderResources(1, 1, &DepthSRV);                  // register(t1)
    }

    // 2. UAV 바인딩
    Graphics->DeviceContext->CSSetUnorderedAccessViews(0, 1, &TileUAV, nullptr);         // register(u0)
    Graphics->DeviceContext->CSSetUnorderedAccessViews(3, 1, &DebugHeatmapUAV, nullptr); // register(u3)

    // 3. 셰이더 바인딩
    Graphics->DeviceContext->CSSetShader(ComputeShader, nullptr, 0);

    // 4. 디스패치
    Graphics->DeviceContext->Dispatch(groupSizeX, groupSizeY, 1);

    // 5-1. UAV 바인딩 해제 (다른 렌더패스에서 사용하기 위함)
    ID3D11UnorderedAccessView* nullUAV = nullptr;
    Graphics->DeviceContext->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
    Graphics->DeviceContext->CSSetUnorderedAccessViews(3, 1, &nullUAV, nullptr);

    // 5-2. SRV 해제
    ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
    Graphics->DeviceContext->CSSetShaderResources(0, 2, nullSRVs);

    // 5-3. 상수버퍼 해제
    ID3D11Buffer* nullBuffer[1] = { nullptr };
    Graphics->DeviceContext->CSSetConstantBuffers(0, 1, nullBuffer);
}



void FTileLightCullingPass::ClearRenderArr()
{
    ClearUAVs();

    PointLights.Empty();
    //SpotLights.Empty();
}

void FTileLightCullingPass::CreateShader()
{
    // Compute Shader 생성
    HRESULT hr = ShaderManager->AddComputeShader(L"TileLightCullingComputeShader", L"Shaders/TileLightCullingComputeShader.hlsl", "mainCS");
    if (FAILED(hr))
    {
        UE_LOG(LogLevel::Error, TEXT("Failed to Compile Compute Shader!"));
    }
    ComputeShader = ShaderManager->GetComputeShaderByKey(L"TileLightCullingComputeShader");

}

void FTileLightCullingPass::CreateLightBufferGPU()
{
    if (PointLights.Num() == 0)
        return;

    TArray<FLightGPU> lights;

    for (UPointLightComponent* LightComp : PointLights)
    {
        if (!LightComp) continue;

        FLightGPU LightData;
        LightData.Position = LightComp->GetWorldLocation();
        LightData.Radius = LightComp->GetRadius();
        LightData.Direction = LightComp->GetUpVector(); // Linear color
        LightData.Padding = 0.0f;

        lights.Add(LightData);
    }

    D3D11_BUFFER_DESC desc = {};
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.ByteWidth = sizeof(FLightGPU) * lights.Num();
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.StructureByteStride = sizeof(FLightGPU);
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = lights.GetData();

    SAFE_RELEASE(LightBufferGPU);
    SAFE_RELEASE(LightSRV);

    HRESULT hr = Graphics->Device->CreateBuffer(&desc, &initData, &LightBufferGPU);
    if (FAILED(hr))
    {
        UE_LOG(LogLevel::Error, TEXT("Failed to create Light Structured Buffer!"));
        return;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = lights.Num();

    hr = Graphics->Device->CreateShaderResourceView(LightBufferGPU, &srvDesc, &LightSRV);
    if (FAILED(hr))
    {
        UE_LOG(LogLevel::Error, TEXT("Failed to create Light Buffer SRV!"));
    }
}


void FTileLightCullingPass::CreateViews()
{
    // 2. entityTiles UAV buffer (tile mask 결과용)
    D3D11_BUFFER_DESC tileBufferDesc = {};
    tileBufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    tileBufferDesc.ByteWidth = sizeof(uint32) * TILE_COUNT * SHADER_ENTITY_TILE_BUCKET_COUNT;
    tileBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    tileBufferDesc.StructureByteStride = sizeof(uint32);
    tileBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

    HRESULT hr = Graphics->Device->CreateBuffer(&tileBufferDesc, nullptr, &TileUAVBuffer);

    if (FAILED(hr))
    {
        UE_LOG(LogLevel::Error, TEXT("Failed to create Tile UAV Buffer!"));
    }

    D3D11_UNORDERED_ACCESS_VIEW_DESC tileUAVDesc = {};
    tileUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    tileUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
    tileUAVDesc.Buffer.FirstElement = 0;
    tileUAVDesc.Buffer.NumElements = TILE_COUNT * SHADER_ENTITY_TILE_BUCKET_COUNT;

    hr = Graphics->Device->CreateUnorderedAccessView(TileUAVBuffer, &tileUAVDesc, &TileUAV);
    if (FAILED(hr))
    {
        UE_LOG(LogLevel::Error, TEXT("Failed to create Tile UAV!"));
    }
}

void FTileLightCullingPass::CreateBuffers()
{
    // 3. Debug heatmap 텍스처 + UAV (디버깅용)
    D3D11_TEXTURE2D_DESC heatMapDesc = {};
    heatMapDesc.Width = Graphics->ScreenWidth;
    heatMapDesc.Height = Graphics->ScreenHeight;
    heatMapDesc.MipLevels = 1;
    heatMapDesc.ArraySize = 1;
    heatMapDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; // R32G32B32A32_FLOAT
    heatMapDesc.SampleDesc.Count = 1;
    heatMapDesc.Usage = D3D11_USAGE_DEFAULT;
    heatMapDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = Graphics->Device->CreateTexture2D(&heatMapDesc, nullptr, &DebugHeatmapTexture);

    if (FAILED(hr))
    {
        UE_LOG(LogLevel::Error, TEXT("Failed to create Heatmap Texture!"));
    }

    D3D11_UNORDERED_ACCESS_VIEW_DESC debugUAVDesc = {};
    debugUAVDesc.Format = heatMapDesc.Format;
    debugUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    hr = Graphics->Device->CreateUnorderedAccessView(DebugHeatmapTexture, &debugUAVDesc, &DebugHeatmapUAV);

    if (FAILED(hr))
    {
        UE_LOG(LogLevel::Error, TEXT("Failed to create Heatmap UAV!"));
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = heatMapDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    Graphics->Device->CreateShaderResourceView(DebugHeatmapTexture, &srvDesc, &DebugHeatmapSRV);

    //// 4. TileLight Culling 설정용 ConstantBuffer (2.5D 켜기/끄기 등)
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.ByteWidth = (sizeof(TileLightCullSettings) + 0xf) & 0xfffffff0; // struct 정의 필요
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = Graphics->Device->CreateBuffer(&cbDesc, nullptr, &TileLightConstantBuffer);
    if (FAILED(hr))
    {
        UE_LOG(LogLevel::Error, TEXT("Failed to create TileLight Constant Buffer!"));
    }
}



void FTileLightCullingPass::Release()
{
    // Compute Shader Release는 ShaderManager에서 관리
    SAFE_RELEASE(TileUAVBuffer);
    SAFE_RELEASE(TileUAV);
    
    SAFE_RELEASE(DebugHeatmapTexture);
    SAFE_RELEASE(DebugHeatmapUAV);
    SAFE_RELEASE(DebugHeatmapSRV);

    SAFE_RELEASE(TileLightConstantBuffer);
    
    SAFE_RELEASE(LightBufferGPU);
    SAFE_RELEASE(LightSRV);
}

void FTileLightCullingPass::ClearUAVs()
{
    // UAV 초기화용 zero값
    UINT clearColor[4] = { 0, 0, 0, 0 };

    // 1. 타일 마스크 초기화
    Graphics->DeviceContext->ClearUnorderedAccessViewUint(TileUAV, clearColor);

    // 2. 히트맵 초기화
    float clearColorF[4] = { 0, 0, 0, 0 };
    Graphics->DeviceContext->ClearUnorderedAccessViewFloat(DebugHeatmapUAV, clearColorF);
}

void FTileLightCullingPass::UpdateTileLightConstantBuffer(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    // 1. Constant Buffer 업데이트
    TileLightCullSettings settings;
    settings.ScreenSize[0] = Graphics->ScreenWidth;
    settings.ScreenSize[1] = Graphics->ScreenHeight;
    settings.TileSize[0] = TILE_SIZE;
    settings.TileSize[1] = TILE_SIZE;
    settings.NearZ = Viewport->NearClip;
    settings.FarZ = Viewport->FarClip;
    settings.ViewMatrix = Viewport->GetViewMatrix();
    settings.ProjectionMatrix = Viewport->GetProjectionMatrix();
    settings.InvProjectionMatrix = FMatrix::Inverse(Viewport->GetProjectionMatrix());
    settings.NumLights = PointLights.Num();
    settings.Enable25DCulling = 1;                      // TODO : IMGUI 연결!

    D3D11_MAPPED_SUBRESOURCE msr;
    HRESULT hr = Graphics->DeviceContext->Map(TileLightConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
    if (FAILED(hr)) {
        UE_LOG(LogLevel::Error, TEXT("Failed to map TileLightConstantBuffer"));
        return;
    }
    memcpy(msr.pData, &settings, sizeof(TileLightCullSettings));
    
    Graphics->DeviceContext->Unmap(TileLightConstantBuffer, 0);
}

// Compute Shader에 사용되는 모든 SRV와 UAV를 해제
void FTileLightCullingPass::ResizeViewBuffers()
{
    Release();

    //CreateShader();
    CreateViews();
    CreateBuffers();
}

bool FTileLightCullingPass::CopyTileLightMaskToCPU(TArray<uint32>& OutData)
{
    // TileUAVBuffer = 조명 비트마스크 결과를 저장 UAV 
    // UAV -> StagingBuffer로 복사하는 코드를 만들 것임 (UAV는 USAGE_DEFAULT라서 CPU에서 읽을 수 없음)

    D3D11_BUFFER_DESC bufferDesc = {};
    TileUAVBuffer->GetDesc(&bufferDesc);
    bufferDesc.Usage = D3D11_USAGE_STAGING;
    bufferDesc.BindFlags = 0; 
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; // CPU 읽기 가능

    ID3D11Buffer* StagingBuffer = nullptr;
    HRESULT hr = Graphics->Device->CreateBuffer(&bufferDesc, nullptr, &StagingBuffer);

    if (FAILED(hr))
    {
        UE_LOG(LogLevel::Error, TEXT("Failed to create staging buffer for TileUAVBuffer"));
        return false;
    }

    Graphics->DeviceContext->CopyResource(StagingBuffer, TileUAVBuffer);
    D3D11_MAPPED_SUBRESOURCE MSR = {};
    hr = Graphics->DeviceContext->Map(StagingBuffer, 0, D3D11_MAP_READ, 0, &MSR);
    if (FAILED(hr))
    {
        UE_LOG(LogLevel::Error, TEXT("TileUAVBuffer Staging Buffer Mapping Failed"));
        SAFE_RELEASE(StagingBuffer);
        return false;
    }

    uint32 count = bufferDesc.ByteWidth / sizeof(uint32);
    OutData.SetNum(count);
    memcpy(OutData.GetData(), MSR.pData, bufferDesc.ByteWidth);

    Graphics->DeviceContext->Unmap(StagingBuffer, 0);
    SAFE_RELEASE(StagingBuffer);

    return true;
}

void FTileLightCullingPass::ParseTileLightMaskData()
{
    // UAV 버퍼 데이터를 CPU 메모리로 복사
    if (!CopyTileLightMaskToCPU(PointLightMaskData))
    {
        UE_LOG(LogLevel::Error, TEXT("TileLightMask 데이터를 CPU로 복사 실패"));
        return;
    }

    // 화면 타일 수: TileUAVBuffer의 데이터는 각 타일마다 SHADER_ENTITY_TILE_BUCKET_COUNT개의 uint32로 구성

    uint32 TotalTiles = TILE_COUNT_X * TILE_COUNT_Y; // MaskData사이즈 = TILECNT_X * TILECNT_Y * 32비트
    PointLightPerTiles.SetNum(TotalTiles);

    uint32 BucketsPerTile = SHADER_ENTITY_TILE_BUCKET_COUNT;
    uint32 TotalLightCount = PointLights.Num(); 

    // 각 타일을 순회하면서 각 타일별 비트마스크를 파싱
    for (uint32 tileIndex = 0; tileIndex < TotalTiles; ++tileIndex)
    {
        TArray<uint32> LightIndices;

        uint32 startIndex = tileIndex * BucketsPerTile;
        for (uint32 bucket = 0; bucket < BucketsPerTile; ++bucket)
        {
            uint32 mask = PointLightMaskData[startIndex + bucket];
            for (uint32 bit = 0; bit < 32; ++bit)
            {
                if (mask & (1u << bit))
                {
                    // 전역 조명 인덱스는 bucket * 32 + bit 로 계산됨.
                    uint32 globalLightIndex = bucket * 32 + bit;
                    // 전역 조명 인덱스가 총 조명 수보다 작은 경우에만 추가
                    if (globalLightIndex < TotalLightCount)
                    {
                        LightIndices.Add(globalLightIndex);
                    }
                }
            }
        }
        PointLightPerTiles[tileIndex] = LightIndices;
    }

#if _PRINTDEBUG
    PrintLightTilesMapping();
#endif
    //UE_LOG(LogLevel::Error, TEXT("타일별 조명 파싱 완료. 총 타일 수: %d"), TotalTiles);

}

void FTileLightCullingPass::PrintLightTilesMapping()
{
    // 총 전역 조명 수
    uint32 TotalLights = PointLights.Num();

    TArray<TArray<uint32>> LightTiles;
    LightTiles.SetNum(TotalLights);

    uint32 TotalTiles = PointLightPerTiles.Num();
    for (uint32 tileIndex = 0; tileIndex < TotalTiles; ++tileIndex)
    {
        // 해당 타일에 영향을 주는 조명 인덱스 목록
        const TArray<uint32>& LightIndices = PointLightPerTiles[tileIndex];
        for (uint32 idx = 0; idx < LightIndices.Num(); ++idx)
        {
            uint32 globalLightIndex = LightIndices[idx]; // 전역 조명 인덱스 (예: 0, 4, 7 등)
            if (globalLightIndex < TotalLights)
            {
                LightTiles[globalLightIndex].Add(tileIndex);
            }
        }
    }

    // 각 조명별로 어떤 타일이 영향을 받는지 출력
    for (uint32 lightIndex = 0; lightIndex < TotalLights; ++lightIndex)
    {
        const TArray<uint32>& Tiles = LightTiles[lightIndex];
        if (Tiles.Num() == 0)
        {
            continue; // 해당 조명이 어느 타일에도 영향을 주지 않으면 생략
        }

        // 출력 문자열 구성 (예: "Light No. 1: 0, 3, 5")
        FString Output = FString::Printf(TEXT("Light No. %d: "), lightIndex + 1);
        for (int32 i = 0; i < Tiles.Num(); ++i)
        {
            Output += FString::Printf(TEXT("%d"), Tiles[i]);
            if (i < Tiles.Num() - 1)
            {
                Output += TEXT(", ");
            }
        }

        UE_LOG(LogLevel::Error, TEXT("%s"), *Output);
    }
}

