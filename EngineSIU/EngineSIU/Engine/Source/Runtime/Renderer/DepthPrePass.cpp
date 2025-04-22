#include "DepthPrePass.h"

#include "UnrealClient.h"
#include "D3D11RHI/GraphicDevice.h"
#include "D3D11RHI/DXDShaderManager.h"
#include "UnrealEd/EditorViewportClient.h"

FDepthPrePass::FDepthPrePass() : FStaticMeshRenderPassBase()
{
}

FDepthPrePass::~FDepthPrePass()
{
}

void FDepthPrePass::PrepareRenderPass(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    ID3D11VertexShader* VertexShader = ShaderManager->GetVertexShaderByKey(L"StaticMeshVertexShader");
    ID3D11InputLayout* InputLayout = ShaderManager->GetInputLayoutByKey(L"StaticMeshVertexShader");
    
    Graphics->DeviceContext->VSSetShader(VertexShader, nullptr, 0);
    Graphics->DeviceContext->IASetInputLayout(InputLayout);
    // 뎁스만 필요하므로, 픽셀 쉐이더는 지정 안함.
    Graphics->DeviceContext->PSSetShader(nullptr, nullptr, 0);

    Graphics->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    Graphics->DeviceContext->RSSetState(Graphics->RasterizerSolidBack);

    Graphics->DeviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

    FViewportResource* ViewportResource = Viewport->GetViewportResource();
    FDepthStencilRHI* DepthStencilRHI = ViewportResource->GetDepthStencil(EResourceType::ERT_Debug);
    Graphics->DeviceContext->OMSetRenderTargets(0, nullptr, DepthStencilRHI->DSV); // ← 깊이 전용
}

void FDepthPrePass::CleanUpRenderPass(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    // 렌더 타겟 해제
    Graphics->DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
}

void FDepthPrePass::Render_Internal(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    __super::Render_Internal(Viewport);
}
