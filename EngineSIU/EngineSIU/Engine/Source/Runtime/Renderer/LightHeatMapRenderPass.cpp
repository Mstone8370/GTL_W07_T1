#include "LightHeatMapRenderPass.h"

#include "D3D11RHI/GraphicDevice.h"
#include "D3D11RHI/DXDShaderManager.h"
#include "D3D11RHI/DXDBufferManager.h"
#include "UnrealEd/EditorViewportClient.h"
#include "Define.h"
#include "Engine/Classes/GameFramework/Actor.h"
#include <wchar.h>
#include <UObject/UObjectIterator.h>
#include <Engine/Engine.h>
#include "PropertyEditor/ShowFlags.h"

FLightHeatMapRenderPass::FLightHeatMapRenderPass()
    : Graphics(nullptr)
    , ShaderManager(nullptr)
    , FogVertexShader(nullptr)
    , FogPixelShader(nullptr)
    , InputLayout(nullptr)
{
}

FLightHeatMapRenderPass::~FLightHeatMapRenderPass()
{
}

void FLightHeatMapRenderPass::Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManager)
{
    Graphics = InGraphics;
    BufferManager = InBufferManager;
    ShaderManager = InShaderManager;
    
    CreateShader();
}

void FLightHeatMapRenderPass::CreateShader()
{
    // 입력 레이아웃 정의: POSITION과 TEXCOORD
    D3D11_INPUT_ELEMENT_DESC fogInputLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    // 정점 셰이더 및 입력 레이아웃 생성
    HRESULT hr = ShaderManager->AddVertexShaderAndInputLayout(
        L"FogVertexShader",
        L"Shaders/FogVertexShader.hlsl",
        "mainVS",
        fogInputLayout,
        ARRAYSIZE(fogInputLayout)
    );
    // 픽셀 셰이더 생성
    /*hr = ShaderManager->AddPixelShader(
        L"FogPixelShader",
        L"Shaders/FogPixelShader.hlsl",
        "mainPS"
    );*/

    hr = ShaderManager->AddPixelShader(
        L"FogQuadPixelShader",
        L"Shaders/FogQuadPixelShader.hlsl",
        "mainPS"
    );

    // 생성된 셰이더와 입력 레이아웃 획득
    FogVertexShader = ShaderManager->GetVertexShaderByKey(L"FogVertexShader");
    FogPixelShader = ShaderManager->GetPixelShaderByKey(L"FogPixelShader");
    FogQuadPixelShader = ShaderManager->GetPixelShaderByKey(L"FogQuadPixelShader");
    InputLayout = ShaderManager->GetInputLayoutByKey(L"FogVertexShader");
}

void FLightHeatMapRenderPass::PrepareRender()
{
    for (const auto iter : TObjectRange<UHeightFogComponent>())
    {
        if (iter->GetWorld() == GEngine->ActiveWorld)
        {
            FogComponents.Add(iter);
        }
    }

}

void FLightHeatMapRenderPass::ClearRenderArr()
{
    FogComponents.Empty();

    ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
    Graphics->DeviceContext->PSSetShaderResources(3, 1, nullSRV); // Compute Shader SRV 해제
}

void FLightHeatMapRenderPass::PrepareRenderState(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    float Color[4] = { 0,0,0,0 };
    Graphics->DeviceContext->ClearRenderTargetView(FogRTV, Color);
    
    // TODO: 이거는 뎁스 프리패스에서 마지막에 정리해줘야 함
    Graphics->DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    
    Graphics->DeviceContext->VSSetShader(FogVertexShader, nullptr, 0);
    Graphics->DeviceContext->PSSetShader(FogPixelShader, nullptr, 0);

    // SRV & Sampler 바인딩
    Graphics->DeviceContext->PSSetShaderResources(0, 1, &DebugHeatmapSRV);
    Graphics->DeviceContext->PSSetSamplers(0, 1, &Sampler);
}

void FLightHeatMapRenderPass::Render(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    PrepareRenderState(Viewport);

    FVertexInfo VertexInfo;
    FIndexInfo IndexInfo;

    BufferManager->GetQuadBuffer(VertexInfo, IndexInfo);

    UINT offset = 0;

    for (const auto& Fog : FogComponents)
    {
        Graphics->DeviceContext->IASetVertexBuffers(0, 1, &VertexInfo.VertexBuffer, &VertexInfo.Stride, &offset);
        Graphics->DeviceContext->IASetIndexBuffer(IndexInfo.IndexBuffer, DXGI_FORMAT_R16_UINT, 0);
        Graphics->DeviceContext->IASetInputLayout(InputLayout);

        Graphics->DeviceContext->DrawIndexed(6, 0, 0);
    }

    FinalRender();

    Graphics->DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    Graphics->DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);

}

void FLightHeatMapRenderPass::CreateBlendState()
{
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    HRESULT hr = Graphics->Device->CreateBlendState(&blendDesc, &FogBlendState);
    if (FAILED(hr))
    {
        MessageBox(NULL, L"AlphaBlendState 생성에 실패했습니다!", L"Error", MB_ICONERROR | MB_OK);
    }
}

void FLightHeatMapRenderPass::FinalRender()
{
    FVertexInfo VertexInfo;
    FIndexInfo IndexInfo;

    BufferManager->GetQuadBuffer(VertexInfo, IndexInfo);

    UINT offset = 0;

    Graphics->DeviceContext->IASetVertexBuffers(0, 1, &VertexInfo.VertexBuffer, &VertexInfo.Stride, &offset);
    Graphics->DeviceContext->IASetIndexBuffer(IndexInfo.IndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    Graphics->DeviceContext->IASetInputLayout(InputLayout);

    Graphics->DeviceContext->DrawIndexed(6, 0, 0);
}

void FLightHeatMapRenderPass::CreateRTV()
{
    Graphics->CreateRTV(FogBuffer, FogRTV);
}

void FLightHeatMapRenderPass::SetDebugHeatmapSRV(ID3D11ShaderResourceView* InDebugHeatmapSRV)
{
    DebugHeatmapSRV = InDebugHeatmapSRV;
}
