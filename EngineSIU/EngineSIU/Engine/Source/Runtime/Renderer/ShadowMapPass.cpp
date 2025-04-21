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
}

void FShadowMapPass::PrepareRenderArr()
{
    __super::PrepareRenderArr();
    for (const auto iter : TObjectRange<UPointLightComponent>())
    {
        PointLightComponents.Add(iter);
    }

    for (const auto iter: TObjectRange<UDirectionalLightComponent>())
    {
        DirectionalLightComponents.Add(iter);
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
    for (auto iter : PointLightComponents)
    {
        for(int i = 0; i < 6; ++i) {
            Graphics->DeviceContext->ClearDepthStencilView(iter->PointShadowDSV[i], D3D11_CLEAR_DEPTH, 1.0f, 0);
        }
        D3D11_VIEWPORT vp = {};
        vp.TopLeftX = 0; vp.TopLeftY = 0;
        vp.Width    = (FLOAT)iter->ShadowMapWidth;
        vp.Height   = (FLOAT)iter->ShadowMapHeight;
        vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;
        iter->UpdateViewProjMatrix();
        for (size_t face = 0; face < 6; ++face)
        {
            Graphics->DeviceContext->RSSetViewports(1, &vp);
            Graphics->DeviceContext->OMSetRenderTargets(0, nullptr, iter->PointShadowDSV[face]);
            UpdateLightMatrixConstant(iter->view[face], iter->projection, vp.Width);
            __super::RenderAllStaticMeshes(Viewport);
        }
    }


    // DirectionalLight
    for (auto iter : DirectionalLightComponents)
    {
        FDepthStencilRHI DepthStencilRHI = iter->GetShadowDepthMap();
        Graphics->DeviceContext->ClearDepthStencilView(DepthStencilRHI.DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
        Graphics->DeviceContext->OMSetRenderTargets(0, nullptr, DepthStencilRHI.DSV);
        
        D3D11_VIEWPORT vp = {};
        vp.TopLeftX = 0; vp.TopLeftY = 0;
        vp.Width    = static_cast<float>(iter->ShadowMapSize);
        vp.Height   = static_cast<float>(iter->ShadowMapSize);
        vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;
        Graphics->DeviceContext->RSSetViewports(1, &vp);
        
        UpdateLightMatrixConstant(
            iter->GetLightViewMatrix(Viewport->GetCameraLocation()),
            iter->GetLightProjMatrix(),
            iter->ShadowMapSize
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
