#include "Define.h"
#include "UObject/Casts.h"
#include "UpdateLightBufferPass.h"

#include <algorithm>
#include "D3D11RHI/DXDBufferManager.h"
#include "D3D11RHI/GraphicDevice.h"
#include "D3D11RHI/DXDShaderManager.h"
#include "Components/Light/LightComponent.h"
#include "Components/Light/PointLightComponent.h"
#include "Components/Light/SpotLightComponent.h"
#include "Components/Light/DirectionalLightComponent.h"
#include "Components/Light/AmbientLightComponent.h"
#include "Engine/EditorEngine.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectIterator.h"
#include "TileLightCullingPass.h"

//------------------------------------------------------------------------------
// 생성자/소멸자
//------------------------------------------------------------------------------
FUpdateLightBufferPass::FUpdateLightBufferPass()
    : BufferManager(nullptr)
    , Graphics(nullptr)
    , ShaderManager(nullptr)
{
}

FUpdateLightBufferPass::~FUpdateLightBufferPass()
{
}

void FUpdateLightBufferPass::Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManager)
{
    BufferManager = InBufferManager;
    Graphics = InGraphics;
    ShaderManager = InShaderManager;

    CreatePointLightBuffer();
    CreatePointLightPerTilesBuffer();
}

void FUpdateLightBufferPass::PrepareRenderArr()
{
    for (const auto iter : TObjectRange<ULightComponentBase>())
    {
        if (iter->GetWorld() == GEngine->ActiveWorld)
        {
            if (UPointLightComponent* PointLight = Cast<UPointLightComponent>(iter))
            {
                //PointLights.Add(PointLight); // 당분간 UnUsed : Structured Buffer로 전달
            }
            else if (USpotLightComponent* SpotLight = Cast<USpotLightComponent>(iter))
            {
                SpotLights.Add(SpotLight);
            }
            else if (UDirectionalLightComponent* DirectionalLight = Cast<UDirectionalLightComponent>(iter))
            {
                DirectionalLights.Add(DirectionalLight);
            }
            else if (UAmbientLightComponent* AmbientLight = Cast<UAmbientLightComponent>(iter))
            {
                AmbientLights.Add(AmbientLight);
            }
        }
    }
}

void FUpdateLightBufferPass::Render(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    UpdateLightBuffer();
    Graphics->DeviceContext->PSSetShaderResources(10, 1, &PointLightSRV);
    Graphics->DeviceContext->PSSetShaderResources(20, 1, &PointLightPerTilesSRV);
    Graphics->DeviceContext->PSSetConstantBuffers(8, 1, &TileConstantBuffer);
}

void FUpdateLightBufferPass::ClearRenderArr()
{
    PointLights.Empty();
    SpotLights.Empty();
    DirectionalLights.Empty();
    AmbientLights.Empty();
}


void FUpdateLightBufferPass::UpdateLightBuffer() const
{
    FLightInfoBuffer LightBufferData = {};

    int DirectionalLightsCount=0;
    int PointLightsCount=0;
    int SpotLightsCount=0;
    int AmbientLightsCount=0;
    
    for (auto Light : SpotLights)
    {
        if (SpotLightsCount < MAX_SPOT_LIGHT)
        {
            LightBufferData.SpotLights[SpotLightsCount] = Light->GetSpotLightInfo();
            LightBufferData.SpotLights[SpotLightsCount].Position = Light->GetWorldLocation();
            LightBufferData.SpotLights[SpotLightsCount].Direction = Light->GetDirection();
            SpotLightsCount++;
        }
    }

    for (auto Light : PointLights)
    {
        if (PointLightsCount < MAX_POINT_LIGHT)
        {
            LightBufferData.PointLights[PointLightsCount] = Light->GetPointLightInfo();
            LightBufferData.PointLights[PointLightsCount].Position = Light->GetWorldLocation();
            PointLightsCount++;
        }
    }

    for (auto Light : DirectionalLights)
    {
        if (DirectionalLightsCount < MAX_DIRECTIONAL_LIGHT)
        {
            LightBufferData.Directional[DirectionalLightsCount] = Light->GetDirectionalLightInfo();
            LightBufferData.Directional[DirectionalLightsCount].Direction = Light->GetDirection();
            DirectionalLightsCount++;
        }
    }

    for (auto Light : AmbientLights)
    {
        if (AmbientLightsCount < MAX_DIRECTIONAL_LIGHT)
        {
            LightBufferData.Ambient[AmbientLightsCount] = Light->GetAmbientLightInfo();
            LightBufferData.Ambient[AmbientLightsCount].AmbientColor = Light->GetLightColor();
            AmbientLightsCount++;
        }
    }
    
    LightBufferData.DirectionalLightsCount = DirectionalLightsCount;
    LightBufferData.PointLightsCount = PointLightsCount;
    LightBufferData.SpotLightsCount = SpotLightsCount;
    LightBufferData.AmbientLightsCount = AmbientLightsCount;

    BufferManager->UpdateConstantBuffer(TEXT("FLightInfoBuffer"), LightBufferData);
    
}

void FUpdateLightBufferPass::SetPointLightData(
    const TArray<UPointLightComponent*>& InPointLights, TArray<TArray<uint32>> InPointLightPerTiles)
{
    PointLights = InPointLights; 
    PointLightPerTiles = InPointLightPerTiles;

    uint32 TotalTiles = PointLightPerTiles.Num();
    GPointLightPerTiles.Empty();
    GPointLightPerTiles.SetNum(TotalTiles);

    for (uint32 TileIndex = 0; TileIndex < TotalTiles; ++TileIndex)
    {
        const TArray<uint32>& TileLightList = InPointLightPerTiles[TileIndex];
        PointLightPerTile TileData = {};
        TileData.NumLights = TileLightList.Num();
        TileData.NumLights = FMath::Min<uint32>(TileData.NumLights, MAX_POINTLIGHT_PER_TILE);

        // 각 조명 인덱스를 TileData.Indice 배열에 복사합니다.
        for (uint32 i = 0; i < TileData.NumLights; ++i)
        {
            TileData.Indices[i] = TileLightList[i];
        }
        GPointLightPerTiles[TileIndex] = TileData;
    }

    UpdatePointLightBuffer();
    UpdatePointLightPerTilesBuffer();
}

void FUpdateLightBufferPass::SetTileConstantBuffer(ID3D11Buffer* InTileConstantBuffer)
{
    TileConstantBuffer = InTileConstantBuffer;
}



void FUpdateLightBufferPass::CreatePointLightBuffer()
{
    D3D11_BUFFER_DESC desc = {};
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.ByteWidth = sizeof(FPointLightInfo) * MAX_NUM_POINTLIGHTS; // TOFIX : 하드코딩 : 10000개 light 받을 수 있음
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride = sizeof(FPointLightInfo);

    HRESULT hr = Graphics->Device->CreateBuffer(&desc, nullptr, &PointLightBuffer);
    if (FAILED(hr))
    {
        UE_LOG(LogLevel::Error, TEXT("Failed to create PointLightBuffer"));
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = MAX_NUM_POINTLIGHTS;

    hr = Graphics->Device->CreateShaderResourceView(PointLightBuffer, &srvDesc, &PointLightSRV);
    if (FAILED(hr))
    {
        UE_LOG(LogLevel::Error, TEXT("Failed to create PointLight SRV"));
    }
}

void FUpdateLightBufferPass::CreatePointLightPerTilesBuffer()
{    
    D3D11_BUFFER_DESC desc = {};
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.ByteWidth = sizeof(PointLightPerTile) * MAX_TILE;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride = sizeof(PointLightPerTile);       // 타일당 라이트 : 기존 1024에서 256으로 변경

    /* D3D11_SUBRESOURCE_DATA initData = {};
     initData.pSysMem = GPointLightPerTiles.GetData();*/

    HRESULT hr = Graphics->Device->CreateBuffer(&desc, nullptr, &PointLightPerTilesBuffer);
    if (FAILED(hr))
    {
        UE_LOG(LogLevel::Error, TEXT("Failed to create PointLightPerTilesBuffer"));
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = MAX_TILE;

    hr = Graphics->Device->CreateShaderResourceView(PointLightPerTilesBuffer, &srvDesc, &PointLightPerTilesSRV);
    if (FAILED(hr))
    {
        UE_LOG(LogLevel::Error, TEXT("Failed to create PointLightPerTiles SRV"));
    }
}

void FUpdateLightBufferPass::UpdatePointLightBuffer()
{
    if (PointLights.Num() == 0 || !PointLightBuffer)
        return;

    TArray<FPointLightInfo> TempBuffer;
    TempBuffer.SetNum(MAX_NUM_POINTLIGHTS);
    for (uint32 i = 0; i < PointLights.Num(); ++i)
    {
        TempBuffer[i] = PointLights[i]->GetPointLightInfo();
        TempBuffer[i].Position = PointLights[i]->GetWorldLocation();
    }
    // 이제 TempBuffer에 대해 업데이트
    Graphics->DeviceContext->UpdateSubresource(PointLightBuffer, 0, nullptr,
        TempBuffer.GetData(), 0, 0);
}

void FUpdateLightBufferPass::UpdatePointLightPerTilesBuffer()
{
    if (GPointLightPerTiles.Num() == 0 || !PointLightPerTilesBuffer)
        return;

    TArray<PointLightPerTile> TempBuffer;
    TempBuffer.SetNum(MAX_TILE);
    for (uint32 i = 0; i < GPointLightPerTiles.Num(); ++i)
    {
        TempBuffer[i] = GPointLightPerTiles[i];
    }
    // 이제 TempBuffer에 대해 업데이트
    Graphics->DeviceContext->UpdateSubresource(PointLightPerTilesBuffer, 0, nullptr,
        TempBuffer.GetData(), 0, 0);
}
