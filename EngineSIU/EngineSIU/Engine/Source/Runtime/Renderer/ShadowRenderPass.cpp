#include "ShadowRenderPass.h"
#include "D3D11RHI/DXDShaderManager.h"
#include "D3D11RHI/DXDBufferManager.h"
#include "D3D11RHI/GraphicDevice.h"
#include "Engine/EditorEngine.h"
#include "UObject/UObjectIterator.h"
#include "BaseGizmos/GizmoBaseComponent.h"
#include "UnrealEd/EditorViewportClient.h"
#include "UnrealClient.h"

FShadowRenderPass::FShadowRenderPass() {}

FShadowRenderPass::~FShadowRenderPass() {}

void FShadowRenderPass::Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManager)
{
    __super::Initialize(InBufferManager, InGraphics, InShaderManager);
    CreateShader();
}

void FShadowRenderPass::PrepareRenderArr()
{
    __super::PrepareRenderArr();

    for (USpotLightComponent* SpotLight : TObjectRange<USpotLightComponent>())
    {
        if (SpotLight->IsCastShadows())
        {
            SpotLight->CreateShadowMapResources();
            SpotLights.Add(SpotLight);
        }
    }
}

void FShadowRenderPass::Render(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    UINT    origNumVPs = 16;
    D3D11_VIEWPORT origVPs[16];
    Graphics->DeviceContext->RSGetViewports(&origNumVPs, origVPs);

    PrepareRenderState(Viewport);

    for (USpotLightComponent* SpotLight : SpotLights)
    {
        Graphics->DeviceContext->ClearDepthStencilView(SpotLight->GetShadowDepthMap().DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

        FShadowDepthMap const& SM = SpotLight->GetShadowDepthMap();
        D3D11_TEXTURE2D_DESC desc = {};
        SM.Texture2D->GetDesc(&desc);

        D3D11_VIEWPORT shadowVP = {};
        shadowVP.TopLeftX = 0;
        shadowVP.TopLeftY = 0;
        shadowVP.Width = (FLOAT)desc.Width;
        shadowVP.Height = (FLOAT)desc.Height;
        shadowVP.MinDepth = 0.0f;
        shadowVP.MaxDepth = 1.0f;
        Graphics->DeviceContext->RSSetViewports(1, &shadowVP);

        ShadowConstants shadowConstant;
        shadowConstant.LightView = SpotLight->GetViewMatrix();
        shadowConstant.LightProjection = SpotLight->GetProjectionMatrix();
        BufferManager->UpdateConstantBuffer(TEXT("FShadowConstants"), &shadowConstant);

        ID3D11DepthStencilView* DSV = SpotLight->GetShadowDepthMap().DSV;
        Graphics->DeviceContext->OMSetRenderTargets(0, nullptr, DSV);

        __super::RenderAllStaticMeshes(Viewport);
    }
    Graphics->DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    Graphics->DeviceContext->RSSetViewports(origNumVPs, origVPs);
}

void FShadowRenderPass::CreateShader()
{
    HRESULT hr = ShaderManager->AddVertexShader(L"ShadowDepthVertexShader", L"Shaders/ShadowDepthVertexShader.hlsl", "mainVS");
    assert(SUCCEEDED(hr));
}

void FShadowRenderPass::PrepareRenderState(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    VertexShader = ShaderManager->GetVertexShaderByKey(L"ShadowDepthVertexShader");
    InputLayout = ShaderManager->GetInputLayoutByKey(L"StaticMeshVertexShader");

    Graphics->DeviceContext->VSSetShader(VertexShader, nullptr, 0);
    Graphics->DeviceContext->PSSetShader(nullptr, nullptr, 0);
    Graphics->DeviceContext->IASetInputLayout(InputLayout);
    Graphics->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    BufferManager->BindConstantBuffer(TEXT("FShadowConstants"), 0, EShaderStage::Vertex);
}

void FShadowRenderPass::ClearRenderArr()
{
    __super::ClearRenderArr();


    SpotLights.Empty();
}
