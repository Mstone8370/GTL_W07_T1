#include "ShadowMapPass.h"

#include "UnrealClient.h"
#include "Components/Light/PointLightComponent.h"
#include "Components/Light/DirectionalLightComponent.h"
#include "Components/Light/SpotLightComponent.h"
#include "D3D11RHI/DXDShaderManager.h"
#include "Engine/Engine.h"
#include "LevelEditor/SLevelEditor.h"
#include "UnrealEd/EditorViewportClient.h"
#include "UObject/Casts.h"
#include "UObject/UObjectIterator.h"

FShadowMapPass::FShadowMapPass() : FStaticMeshRenderPassBase()
{
}

FShadowMapPass::~FShadowMapPass()
{
}

void FShadowMapPass::PrepareRenderArr()
{
    __super::PrepareRenderArr();
    
    for (const auto iter: TObjectRange<UDirectionalLightComponent>())
    {
        if (iter->GetWorld() == GEngine->ActiveWorld)
        {
            DirectionalLightComponents.Add(iter);
        }
    }
    for (const auto iter : TObjectRange<USpotLightComponent>())
    {
        if (iter->GetWorld() == GEngine->ActiveWorld)
        {
            SpotLightComponents.Add(iter);
        }
    }
    for (const auto iter : TObjectRange<UPointLightComponent>())
    {
        if (iter->GetWorld() == GEngine->ActiveWorld)
        {
            PointLightComponents.Add(iter);
        }
    }
}

void FShadowMapPass::ClearRenderArr()
{
    __super::ClearRenderArr();
    
    PointLightComponents.Empty();
    SpotLightComponents.Empty();
    DirectionalLightComponents.Empty();
}

void FShadowMapPass::CreateResource()
{
    HRESULT hr = ShaderManager->AddVertexShader(L"ShadowVertexShader", L"Shaders/ShadowVertexShader.hlsl", "mainVS");
    if (FAILED(hr))
    {
        return;
    }
}

void FShadowMapPass::PrepareRenderPass(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    ID3D11VertexShader* VertexShader = ShaderManager->GetVertexShaderByKey(L"ShadowVertexShader");
    ID3D11InputLayout* InputLayout = ShaderManager->GetInputLayoutByKey(L"StaticMeshVertexShader");
    
    Graphics->DeviceContext->VSSetShader(VertexShader, nullptr, 0);
    Graphics->DeviceContext->IASetInputLayout(InputLayout);
    // 뎁스만 필요하므로, 픽셀 쉐이더는 지정 안함.
    Graphics->DeviceContext->PSSetShader(nullptr, nullptr, 0);
    
    Graphics->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    Graphics->DeviceContext->RSSetState(Graphics->RasterizerShadowMap);

    Graphics->DeviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
}

void FShadowMapPass::CleanUpRenderPass(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    // End
    Graphics->DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
}

void FShadowMapPass::Render_Internal(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    bool bShadowOn = GEngineLoop.GetLevelEditor()->GetActiveViewportClient()->GetShowFlag() & EEngineShowFlags::SF_Shadow;
    if (!bShadowOn)
    {
        return;
    }
    
    RenderPointLight(Viewport);
    RenderSpotLight(Viewport);
    RenderDirectionalLight(Viewport);
}

void FShadowMapPass::UpdateLightMatrixConstant(const FMatrix& LightView, const FMatrix& LightProjection, const float ShadowMapSize)
{
    FLightConstants ObjectData = {};
    ObjectData.LightViewMatrix = LightView;
    ObjectData.LightProjMatrix = LightProjection;
    ObjectData.ShadowMapSize = ShadowMapSize;
    
    BufferManager->BindConstantBuffer(TEXT("FLightConstants"), 0, EShaderStage::Vertex);
    BufferManager->UpdateConstantBuffer(TEXT("FLightConstants"), ObjectData);
}

void FShadowMapPass::SetShadowViewports(float Width, float Height)
{
    ShadowViewport.TopLeftX = 0;
    ShadowViewport.TopLeftY = 0;
    ShadowViewport.Width    = Width;
    ShadowViewport.Height   = Height;
    ShadowViewport.MinDepth = 0.0f;
    ShadowViewport.MaxDepth = 1.0f;
}

void FShadowMapPass::RenderPointLight(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    for (auto PointLight : PointLightComponents)
    {
        // Clear DSV
        for (int i = 0; i < 6; ++i)
        {
            Graphics->DeviceContext->ClearDepthStencilView(PointLight->PointShadowDSV[i], D3D11_CLEAR_DEPTH, 1.0f, 0);
        }

        // Check
        if (!PointLight->IsCastShadows())
        {
            continue;
        }

        // Prepare
        SetShadowViewports(PointLight->GetShadowResolutionScale(), PointLight->GetShadowResolutionScale());
        Graphics->DeviceContext->RSSetViewports(1, &ShadowViewport);

        // Update
        PointLight->UpdateViewProjMatrix();

        // Render
        for (size_t face = 0; face < 6; ++face)
        {
            Graphics->DeviceContext->OMSetRenderTargets(0, nullptr, PointLight->PointShadowDSV[face]);
            UpdateLightMatrixConstant(PointLight->GetLightViewMatrix()[face], PointLight->GetLightProjectionMatrix(), PointLight->GetShadowResolutionScale());

            RenderAllStaticMeshes(Viewport);
        }
    }
}

void FShadowMapPass::RenderSpotLight(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    for (auto SpotLight : SpotLightComponents)
    {
        // Clear DSV
        ID3D11DepthStencilView* DSV = SpotLight->GetShadowDepthMap().DSV;
        Graphics->DeviceContext->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

        // Check
        if (!SpotLight->IsCastShadows())
        {
            continue;
        }

        // Prepare
        SetShadowViewports(SpotLight->GetShadowResolutionScale(), SpotLight->GetShadowResolutionScale());
        Graphics->DeviceContext->RSSetViewports(1, &ShadowViewport);
        
        // Update
        UpdateLightMatrixConstant(SpotLight->GetLightViewMatrix(), SpotLight->GetLightProjectionMatrix(), SpotLight->GetShadowResolutionScale());

        // Render
        Graphics->DeviceContext->OMSetRenderTargets(0, nullptr, DSV);

        RenderAllStaticMeshes(Viewport);
    }
}

void FShadowMapPass::RenderDirectionalLight(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    for (auto DirLight : DirectionalLightComponents)
    {
        // Clear DSV
        FDepthStencilRHI DepthStencilRHI = DirLight->GetShadowDepthMap();
        Graphics->DeviceContext->ClearDepthStencilView(DepthStencilRHI.DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

        // Check
        if (!DirLight->IsCastShadows())
        {
            continue;
        }
        
        // Prepare
        SetShadowViewports(DirLight->GetShadowResolutionScale(), DirLight->GetShadowResolutionScale());
        Graphics->DeviceContext->RSSetViewports(1, &ShadowViewport);

        // Update
        UpdateLightMatrixConstant(
            DirLight->GetLightViewMatrix(Viewport->GetCameraLocation()),
            DirLight->GetLightProjMatrix(),
            DirLight->GetShadowResolutionScale()
        );

        // Render
        Graphics->DeviceContext->OMSetRenderTargets(0, nullptr, DepthStencilRHI.DSV);
        
        RenderAllStaticMeshes(Viewport);
    }
}
