#include "StaticMeshRenderPass.h"

#include <array>

#include "EngineLoop.h"
#include "World/World.h"

#include "RendererHelpers.h"
#include "UnrealClient.h"

#include "UObject/UObjectIterator.h"
#include "UObject/Casts.h"

#include "D3D11RHI/DXDBufferManager.h"
#include "D3D11RHI/GraphicDevice.h"
#include "D3D11RHI/DXDShaderManager.h"

#include "Components/StaticMeshComponent.h"
#include "Components/Light/PointLightComponent.h"

#include "BaseGizmos/GizmoBaseComponent.h"
#include "Components/Light/DirectionalLightComponent.h"
#include "Engine/EditorEngine.h"
#include "Math/JungleMath.h"

#include "PropertyEditor/ShowFlags.h"

#include "UnrealEd/EditorViewportClient.h"



FStaticMeshRenderPass::FStaticMeshRenderPass()
    : VertexShader(nullptr)
    , PixelShader(nullptr)
    , InputLayout(nullptr)
    , BufferManager(nullptr)
    , Graphics(nullptr)
    , ShaderManager(nullptr)
{
}

FStaticMeshRenderPass::~FStaticMeshRenderPass()
{
    ReleaseShader();
}

void FStaticMeshRenderPass::CreateShader()
{
    // Begin Debug Shaders
    HRESULT hr = ShaderManager->AddPixelShader(L"StaticMeshPixelShaderDepth", L"Shaders/StaticMeshPixelShaderDepth.hlsl", "mainPS");
    if (FAILED(hr))
    {
        return;
    }
    hr = ShaderManager->AddPixelShader(L"StaticMeshPixelShaderWorldNormal", L"Shaders/StaticMeshPixelShaderWorldNormal.hlsl", "mainPS");
    if (FAILED(hr))
    {
        return;
    }
    hr = ShaderManager->AddPixelShader(L"StaticMeshPixelShaderWorldTangent", L"Shaders/StaticMeshPixelShaderWorldTangent.hlsl", "mainPS");
    if (FAILED(hr))
    {
        return;
    }
    // End Debug Shaders

#pragma region UberShader
    D3D_SHADER_MACRO DefinesGouraud[] =
    {
        { GOURAUD, "1" },
        { nullptr, nullptr }
    };
    hr = ShaderManager->AddPixelShader(L"GOURAUD_StaticMeshPixelShader", L"Shaders/StaticMeshPixelShader.hlsl", "mainPS", DefinesGouraud);
    if (FAILED(hr))
    {
        return;
    }
    
    D3D_SHADER_MACRO DefinesLambert[] =
    {
        { LAMBERT, "1" },
        { nullptr, nullptr }
    };
    hr = ShaderManager->AddPixelShader(L"LAMBERT_StaticMeshPixelShader", L"Shaders/StaticMeshPixelShader.hlsl", "mainPS", DefinesLambert);
    if (FAILED(hr))
    {
        return;
    }
    
    D3D_SHADER_MACRO DefinesBlinnPhong[] =
    {
        { BLINN_PHONG, "1" },
        { nullptr, nullptr }
    };
    hr = ShaderManager->AddPixelShader(L"PHONG_StaticMeshPixelShader", L"Shaders/StaticMeshPixelShader.hlsl", "mainPS", DefinesBlinnPhong);
    if (FAILED(hr))
    {
        return;
    }

    D3D_SHADER_MACRO DefinesSG[] =
    {
        { PBR, "1" },
        { nullptr, nullptr }
    };
    hr = ShaderManager->AddPixelShader(L"PBR_StaticMeshPixelShader", L"Shaders/StaticMeshPixelShader.hlsl", "mainPS", DefinesSG);
    if (FAILED(hr))
    {
        return;
    }
    
#pragma endregion UberShader
    
    VertexShader = ShaderManager->GetVertexShaderByKey(L"StaticMeshVertexShader");
    InputLayout = ShaderManager->GetInputLayoutByKey(L"StaticMeshVertexShader");
    
    PixelShader = ShaderManager->GetPixelShaderByKey(L"PHONG_StaticMeshPixelShader");
}

void FStaticMeshRenderPass::ReleaseShader()
{
    
}

void FStaticMeshRenderPass::ChangeViewMode(EViewModeIndex ViewMode)
{
    switch (ViewMode)
    {
    case EViewModeIndex::VMI_Lit_Gouraud:
        PixelShader = ShaderManager->GetPixelShaderByKey(L"GOURAUD_StaticMeshPixelShader");
        break;
    case EViewModeIndex::VMI_LIT_PBR:
        PixelShader = ShaderManager->GetPixelShaderByKey(L"PBR_StaticMeshPixelShader");
        break;
    case EViewModeIndex::VMI_Lit_BlinnPhong:
        PixelShader = ShaderManager->GetPixelShaderByKey(L"PHONG_StaticMeshPixelShader");
        break;
    case EViewModeIndex::VMI_SceneDepth:
        PixelShader = ShaderManager->GetPixelShaderByKey(L"StaticMeshPixelShaderDepth");
        break;
    case EViewModeIndex::VMI_WorldNormal:
        PixelShader = ShaderManager->GetPixelShaderByKey(L"StaticMeshPixelShaderWorldNormal");
        break;
    case EViewModeIndex::VMI_WorldTangent:
        PixelShader = ShaderManager->GetPixelShaderByKey(L"StaticMeshPixelShaderWorldTangent");
        break;
    default:
        PixelShader = ShaderManager->GetPixelShaderByKey(L"LAMBERT_StaticMeshPixelShader");
        break;
    }

    // Constant buffer
    if (ViewMode < EViewModeIndex::VMI_Unlit || ViewMode == EViewModeIndex::VMI_LightHeatMap)
    {
        UpdateLitUnlitConstant(1);
    }
    else
    {
        UpdateLitUnlitConstant(0);
    }

    // Vertex Shader and Input Layout
    if (ViewMode == EViewModeIndex::VMI_Lit_Gouraud)
    {
        VertexShader = ShaderManager->GetVertexShaderByKey(L"GOURAUD_StaticMeshVertexShader");
        InputLayout = ShaderManager->GetInputLayoutByKey(L"GOURAUD_StaticMeshVertexShader");
    }
    else
    {
        VertexShader = ShaderManager->GetVertexShaderByKey(L"StaticMeshVertexShader");
        InputLayout = ShaderManager->GetInputLayoutByKey(L"StaticMeshVertexShader");
    }
    
    // Rasterizer
    Graphics->ChangeRasterizer(ViewMode);

    // Setup
    Graphics->DeviceContext->VSSetShader(VertexShader, nullptr, 0);
    Graphics->DeviceContext->IASetInputLayout(InputLayout);
    Graphics->DeviceContext->PSSetShader(PixelShader, nullptr, 0);
}

void FStaticMeshRenderPass::UpdatePointLightConstantBuffer(const TArray<UPointLightComponent*>& PointLights)
{
    FPointLightMatrix ObjectData = {};
    uint32 count = FMath::Min((int32)PointLights.Num(), MAX_POINT_LIGHT);
    for (uint32 i = 0; i < count; ++i)
    {
        auto* L = PointLights[i];
        for (int face = 0; face < 6; ++face)
        {
            ObjectData.LightViewMat[i*6 + face] = L->GetLightViewMatrix()[face];
        }
        ObjectData.LightProjectMat[i] = L->GetLightProjectionMatrix();
    }
    BufferManager->UpdateConstantBuffer(TEXT("FPointLightMatrix"), ObjectData);
}

void FStaticMeshRenderPass::UpdateSpotLightConstantBuffer(const FMatrix& View, const FMatrix& Projection)
{
    FSpotLightConstants ObjectData = {};
    ObjectData.LightView = View;
    ObjectData.LightProjection = Projection;
    BufferManager->UpdateConstantBuffer(TEXT("FSpotLightConstants"), ObjectData);
}

void FStaticMeshRenderPass::Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManager)
{
    BufferManager = InBufferManager;
    Graphics = InGraphics;
    ShaderManager = InShaderManager;

    CreateShader();
}

void FStaticMeshRenderPass::PrepareRenderArr()
{
    for (const auto iter : TObjectRange<UStaticMeshComponent>())
    {
        if (!Cast<UGizmoBaseComponent>(iter) && iter->GetWorld() == GEngine->ActiveWorld)
        {
            StaticMeshComponents.Add(iter);
        }
    }
    
}

void FStaticMeshRenderPass::PrepareRenderState(const std::shared_ptr<FEditorViewportClient>& Viewport) 
{
    const EViewModeIndex ViewMode = Viewport->GetViewMode();

    ChangeViewMode(ViewMode);

    Graphics->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    TArray<FString> PSBufferKeys = {
        TEXT("FLightInfoBuffer"),
        TEXT("FMaterialConstants"),
        TEXT("FLitUnlitConstants"),
        TEXT("FSubMeshConstants"),
        TEXT("FTextureConstants"),
        TEXT("FLightConstants"),
        TEXT("FPointLightMatrix"),
        TEXT("FSpotLightConstants"),
    };

    BufferManager->BindConstantBuffers(PSBufferKeys, 0, EShaderStage::Pixel);

    BufferManager->BindConstantBuffer(TEXT("FLightInfoBuffer"), 0, EShaderStage::Vertex);
    BufferManager->BindConstantBuffer(TEXT("FMaterialConstants"), 1, EShaderStage::Vertex);
}

void FStaticMeshRenderPass::UpdateObjectConstant(const FMatrix& WorldMatrix, const FVector4& UUIDColor, bool bIsSelected) const
{
    FObjectConstantBuffer ObjectData = {};
    ObjectData.WorldMatrix = WorldMatrix;
    ObjectData.InverseTransposedWorld = FMatrix::Transpose(FMatrix::Inverse(WorldMatrix));
    ObjectData.UUIDColor = UUIDColor;
    ObjectData.bIsSelected = bIsSelected;
    
    BufferManager->UpdateConstantBuffer(TEXT("FObjectConstantBuffer"), ObjectData);
}

void FStaticMeshRenderPass::UpdateLitUnlitConstant(int32 isLit) const
{
    FLitUnlitConstants Data;
    Data.bIsLit = isLit;
    BufferManager->UpdateConstantBuffer(TEXT("FLitUnlitConstants"), Data);
}

void FStaticMeshRenderPass::RenderPrimitive(FStaticMeshRenderData* RenderData, TArray<FStaticMaterial*> Materials, TArray<UMaterial*> OverrideMaterials, int SelectedSubMeshIndex) const
{
    UINT Stride = sizeof(FStaticMeshVertex);
    UINT Offset = 0;

    FVertexInfo VertexInfo;
    BufferManager->CreateVertexBuffer(RenderData->ObjectName, RenderData->Vertices, VertexInfo);
    
    Graphics->DeviceContext->IASetVertexBuffers(0, 1, &VertexInfo.VertexBuffer, &Stride, &Offset);

    FIndexInfo IndexInfo;
    BufferManager->CreateIndexBuffer(RenderData->ObjectName, RenderData->Indices, IndexInfo);
    if (IndexInfo.IndexBuffer)
    {
        Graphics->DeviceContext->IASetIndexBuffer(IndexInfo.IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
    }

    if (RenderData->MaterialSubsets.Num() == 0)
    {
        Graphics->DeviceContext->DrawIndexed(RenderData->Indices.Num(), 0, 0);
        return;
    }

    for (int SubMeshIndex = 0; SubMeshIndex < RenderData->MaterialSubsets.Num(); SubMeshIndex++)
    {
        uint32 MaterialIndex = RenderData->MaterialSubsets[SubMeshIndex].MaterialIndex;

        FSubMeshConstants SubMeshData = (SubMeshIndex == SelectedSubMeshIndex) ? FSubMeshConstants(true) : FSubMeshConstants(false);

        BufferManager->UpdateConstantBuffer(TEXT("FSubMeshConstants"), SubMeshData);

        if (OverrideMaterials[MaterialIndex] != nullptr)
        {
            MaterialUtils::UpdateMaterial(BufferManager, Graphics, OverrideMaterials[MaterialIndex]->GetMaterialInfo());
        }
        else
        {
            MaterialUtils::UpdateMaterial(BufferManager, Graphics, Materials[MaterialIndex]->Material->GetMaterialInfo());
        }

        uint32 StartIndex = RenderData->MaterialSubsets[SubMeshIndex].IndexStart;
        uint32 IndexCount = RenderData->MaterialSubsets[SubMeshIndex].IndexCount;
        Graphics->DeviceContext->DrawIndexed(IndexCount, StartIndex, 0);
    }
}

void FStaticMeshRenderPass::RenderPrimitive(ID3D11Buffer* pBuffer, UINT numVertices) const
{
    UINT Stride = sizeof(FStaticMeshVertex);
    UINT Offset = 0;
    Graphics->DeviceContext->IASetVertexBuffers(0, 1, &pBuffer, &Stride, &Offset);
    Graphics->DeviceContext->Draw(numVertices, 0);
}

void FStaticMeshRenderPass::RenderPrimitive(ID3D11Buffer* pVertexBuffer, UINT numVertices, ID3D11Buffer* pIndexBuffer, UINT numIndices) const
{
    UINT Stride = sizeof(FStaticMeshVertex);
    UINT Offset = 0;
    Graphics->DeviceContext->IASetVertexBuffers(0, 1, &pVertexBuffer, &Stride, &Offset);
    Graphics->DeviceContext->IASetIndexBuffer(pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
    Graphics->DeviceContext->DrawIndexed(numIndices, 0, 0);
}

void FStaticMeshRenderPass::RenderAllStaticMeshes(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    for (UStaticMeshComponent* Comp : StaticMeshComponents)
    {
        if (!Comp || !Comp->GetStaticMesh())
        {
            continue;
        }

        FStaticMeshRenderData* RenderData = Comp->GetStaticMesh()->GetRenderData();
        if (RenderData == nullptr)
        {
            continue;
        }
        
#pragma region ShadowMap
        for (USpotLightComponent* SpotLight : TObjectRange<USpotLightComponent>())
        {
            auto srv = SpotLight->GetShadowDepthMap().SRV;
            Graphics->DeviceContext->PSSetShaderResources(13, 1, &srv);
            
            UpdateSpotLightConstantBuffer(SpotLight->GetLightViewMatrix(), SpotLight->GetLightProjectionMatrix());
        }
        
        TArray<ID3D11ShaderResourceView*> ShadowCubeSRV;
        ID3D11SamplerState* PCFSampler = nullptr;
        TArray<UPointLightComponent*> PointLights;
        for (auto light :  TObjectRange<UPointLightComponent>())
        {
            PointLights.Add(light);
            ShadowCubeSRV.Add(light->PointShadowSRV);
            PCFSampler = light->PointShadowComparisonSampler;
        }
        UpdatePointLightConstantBuffer(PointLights);
        Graphics->DeviceContext->PSSetShaderResources(
            14,
            ShadowCubeSRV.Num(),         
            ShadowCubeSRV.GetData()
        );     
        Graphics->DeviceContext->PSSetSamplers(
           13,         
           1,
           &PCFSampler
        );
#pragma endregion
        
        UEditorEngine* Engine = Cast<UEditorEngine>(GEngine);

        FMatrix WorldMatrix = Comp->GetWorldMatrix();
        FVector4 UUIDColor = Comp->EncodeUUID() / 255.0f;
        const bool bIsSelected = (Engine && Engine->GetSelectedActor() == Comp->GetOwner());

        UpdateObjectConstant(WorldMatrix, UUIDColor, bIsSelected);

        RenderPrimitive(RenderData, Comp->GetStaticMesh()->GetMaterials(), Comp->GetOverrideMaterials(), Comp->GetselectedSubMeshIndex());

        if (Viewport->GetShowFlag() & static_cast<uint64>(EEngineShowFlags::SF_AABB))
        {
            FEngineLoop::PrimitiveDrawBatch.AddAABBToBatch(Comp->GetBoundingBox(), Comp->GetWorldLocation(), WorldMatrix);
        }
    }
}

void FStaticMeshRenderPass::Render(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    const EResourceType ResourceType = EResourceType::ERT_Scene;
    FViewportResource* ViewportResource = Viewport->GetViewportResource();
    FRenderTargetRHI* RenderTargetRHI = ViewportResource->GetRenderTarget(ResourceType);
    FDepthStencilRHI* DepthStencilRHI = ViewportResource->GetDepthStencil(ResourceType);
    
    // Setup Viewport and RTV
    Graphics->DeviceContext->RSSetViewports(1, &ViewportResource->GetD3DViewport());
    Graphics->DeviceContext->OMSetRenderTargets(1, &RenderTargetRHI->RTV, DepthStencilRHI->DSV);

    auto tempDirLightRange = TObjectRange<UDirectionalLightComponent>();
    
    if (begin(tempDirLightRange) != end(tempDirLightRange))
    {
        UDirectionalLightComponent* tempDirLight = *tempDirLightRange.Begin;
        FLightConstants LightData = {};
        LightData.LightViewMatrix = tempDirLight->GetLightViewMatrix(Viewport->GetCameraLocation());
        LightData.LightProjMatrix = tempDirLight->GetLightProjMatrix();
        LightData.ShadowMapSize = tempDirLight->ShadowResolutionScale;
        BufferManager->BindConstantBuffer(TEXT("FLightConstants"), 5, EShaderStage::Pixel);
        BufferManager->UpdateConstantBuffer(TEXT("FLightConstants"), LightData);
    
        ID3D11ShaderResourceView* ShadowMapSRV = tempDirLight->GetShadowDepthMap().SRV;
        Graphics->DeviceContext->PSSetShaderResources(12, 1, &ShadowMapSRV);
    }
    PrepareRenderState(Viewport);
    RenderAllStaticMeshes(Viewport);

    // 렌더 타겟 해제
    Graphics->DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

    // 쉐이더 리소스 해제
    constexpr UINT NumViews = static_cast<UINT>(EMaterialTextureSlots::MTS_MAX);
    
    ID3D11ShaderResourceView* NullSRVs[NumViews] = { nullptr };
    ID3D11SamplerState* NullSamplers[NumViews] = { nullptr};
    
    Graphics->DeviceContext->PSSetShaderResources(0, NumViews, NullSRVs);
    Graphics->DeviceContext->PSSetSamplers(0, NumViews, NullSamplers);

    // for Gouraud shading
    ID3D11ShaderResourceView* NullSRV[1] = { nullptr };
    ID3D11SamplerState* NullSampler[1] = { nullptr};
    Graphics->DeviceContext->VSSetShaderResources(0, 1, NullSRV);
    Graphics->DeviceContext->VSSetSamplers(0, 1, NullSampler);
}

void FStaticMeshRenderPass::ClearRenderArr()
{
    StaticMeshComponents.Empty();
}

