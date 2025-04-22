#include "ShadowMapPass.h"

#include "UnrealClient.h"
#include "Components/Light/PointLightComponent.h"
#include "Components/Light/DirectionalLightComponent.h"
#include "D3D11RHI/DXDShaderManager.h"
#include "UnrealEd/EditorViewportClient.h"
#include "UObject/Casts.h"
#include "UObject/UObjectIterator.h"

FShadowMapPass::FShadowMapPass()
{
}

FShadowMapPass::~FShadowMapPass()
{
}

void FShadowMapPass::Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManage)
{
    __super::Initialize(InBufferManager, InGraphics, InShaderManage);
    
    CraeteShadowShader();
    SetShadowViewports();
}

void FShadowMapPass::PrepareRenderArr()
{
    __super::PrepareRenderArr();
    for (const auto iter: TObjectRange<UDirectionalLightComponent>())
    {
        DirectionalLightComponents.Add(iter);
    }
    for (const auto iter : TObjectRange<USpotLightComponent>())
    {
        SpotLightComponents.Add(iter);
    }
    for (const auto iter : TObjectRange<UPointLightComponent>())
    {
        PointLightComponents.Add(iter);
    }

}

void FShadowMapPass::Render(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    ID3D11Buffer* LightBuffer = BufferManager->GetConstantBuffer(TEXT("FLightConstants"));
    Graphics->DeviceContext->VSSetConstantBuffers(1, 1, &LightBuffer);
    UINT numViewports = 1;
    D3D11_VIEWPORT origVP[16];  
    Graphics->DeviceContext->RSGetViewports(&numViewports, origVP);
    
    PrepareRenderState(Viewport);


    // PointLight
    for (auto PointLight : PointLightComponents)
    {
        ShadowViewport.Width = ShadowViewport.Height = PointLight->ShadowResolutionScale;
        Graphics->DeviceContext->RSSetViewports(1, &ShadowViewport);
        for(int i = 0; i < 6; ++i) {
            Graphics->DeviceContext->ClearDepthStencilView(PointLight->PointShadowDSV[i], D3D11_CLEAR_DEPTH, 1.0f, 0);
        }
        PointLight->UpdateViewProjMatrix();
        for (size_t face = 0; face < 6; ++face)
        {

            Graphics->DeviceContext->OMSetRenderTargets(0, nullptr, PointLight->PointShadowDSV[face]);
            UpdateLightMatrixConstant(PointLight->GetLightViewMatrix()[face], PointLight->GetLightProjectionMatrix(), PointLight->ShadowResolutionScale);
            __super::RenderAllStaticMeshes(Viewport);
        }
    }

    // SpotLight
    for (auto SpotLight : SpotLightComponents)
    {

        Graphics->DeviceContext->ClearDepthStencilView(SpotLight->GetShadowDepthMap().DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

        FShadowDepthMap const& SM = SpotLight->GetShadowDepthMap();

        D3D11_TEXTURE2D_DESC desc = {};
        SM.Texture2D->GetDesc(&desc);
        
        ShadowViewport.Width = ShadowViewport.Height = SpotLight->ShadowResolutionScale;
        Graphics->DeviceContext->RSSetViewports(1, &ShadowViewport);
        UpdateLightMatrixConstant(SpotLight->GetLightViewMatrix(), SpotLight->GetLightProjectionMatrix(), SpotLight->ShadowResolutionScale);
        
        ID3D11DepthStencilView* DSV = SpotLight->GetShadowDepthMap().DSV;
        Graphics->DeviceContext->OMSetRenderTargets(0, nullptr, DSV);

        __super::RenderAllStaticMeshes(Viewport);
    }
    
    // DirectionalLight
    for (auto DirLight : DirectionalLightComponents)
    {
        // Set Viewport 
        ShadowViewport.Width = ShadowViewport.Height = DirLight->ShadowResolutionScale;
        Graphics->DeviceContext->RSSetViewports(1, &ShadowViewport);
        
        FDepthStencilRHI DepthStencilRHI = DirLight->GetShadowDepthMap();
        Graphics->DeviceContext->ClearDepthStencilView(DepthStencilRHI.DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
        Graphics->DeviceContext->OMSetRenderTargets(0, nullptr, DepthStencilRHI.DSV);
        
        
        UpdateLightMatrixConstant(
            DirLight->GetLightViewMatrix(Viewport->GetCameraLocation()),
            DirLight->GetLightProjMatrix(),
            DirLight->ShadowResolutionScale
        );
        
        __super::RenderAllStaticMeshes(Viewport);
    }
    
    Graphics->DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    Graphics->DeviceContext->RSSetViewports(numViewports, origVP);
}

void FShadowMapPass::ClearRenderArr()
{
    __super::ClearRenderArr();
    PointLightComponents.Empty();
    SpotLightComponents.Empty();
    DirectionalLightComponents.Empty();
}

void FShadowMapPass::PrepareRenderState(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    VertexShader = ShaderManager->GetVertexShaderByKey(L"ShadowVertexShader");
    InputLayout = ShaderManager->GetInputLayoutByKey(L"StaticMeshVertexShader");
    
    Graphics->DeviceContext->VSSetShader(VertexShader, nullptr, 0);
    Graphics->DeviceContext->IASetInputLayout(InputLayout);

    // 뎁스만 필요하므로, 픽셀 쉐이더는 지정 안함.
    Graphics->DeviceContext->PSSetShader(nullptr, nullptr, 0);
    Graphics->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    Graphics->DeviceContext->RSSetState(Graphics->RasterizerShadowMap);

    Graphics->DeviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
}

void FShadowMapPass::UpdateLightMatrixConstant(const FMatrix& LightView, const FMatrix& LightProjection, const float ShadowMapSize)
{
    FLightConstants ObjectData = {};
    ObjectData.LightViewMatrix = LightView;
    ObjectData.LightProjMatrix = LightProjection;
    ObjectData.ShadowMapSize = ShadowMapSize;
    
    BufferManager->BindConstantBuffer(TEXT("FLightConstants"), 5, EShaderStage::Vertex);
    BufferManager->UpdateConstantBuffer(TEXT("FLightConstants"), ObjectData);
}

void FShadowMapPass::CraeteShadowShader()
{
    // Begine ShadowMap Shader 
    HRESULT hr = ShaderManager->AddVertexShader(L"ShadowVertexShader", L"Shaders/ShadowVertexShader.hlsl", "mainVS");
    if (FAILED(hr))
    {
        return;
    }
    // End Debug Shaders

}

void FShadowMapPass::SetShadowViewports()
{
    ShadowViewport.TopLeftX = 0; ShadowViewport.TopLeftY = 0;
    ShadowViewport.Width    = 1024;
    ShadowViewport.Height   = 1024;
    ShadowViewport.MinDepth = 0.0f; ShadowViewport.MaxDepth = 1.0f;
}
