#include "StaticMeshRenderPass.h"

#include <array>

#include "EngineLoop.h"
#include "World/World.h"

#include "UnrealClient.h"

#include "UObject/UObjectIterator.h"
#include "UObject/Casts.h"

#include "D3D11RHI/DXDBufferManager.h"
#include "D3D11RHI/GraphicDevice.h"
#include "D3D11RHI/DXDShaderManager.h"

#include "Components/StaticMeshComponent.h"
#include "Components/Light/PointLightComponent.h"

#include "Components/Light/DirectionalLightComponent.h"
#include "Engine/EditorEngine.h"

#include "UnrealEd/EditorViewportClient.h"

FStaticMeshRenderPass::FStaticMeshRenderPass() : FStaticMeshRenderPassBase()
{
}

FStaticMeshRenderPass::~FStaticMeshRenderPass()
{
}

void FStaticMeshRenderPass::CreateResource()
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

    CreateSamplers();
}

void FStaticMeshRenderPass::CreateSamplers()
{
    D3D11_SAMPLER_DESC ComparisonSamplerDesc;
    ZeroMemory(&ComparisonSamplerDesc, sizeof(D3D11_SAMPLER_DESC));
    ComparisonSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    ComparisonSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    ComparisonSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    ComparisonSamplerDesc.BorderColor[0] = 1.0f;
    ComparisonSamplerDesc.BorderColor[1] = 1.0f;
    ComparisonSamplerDesc.BorderColor[2] = 1.0f;
    ComparisonSamplerDesc.BorderColor[3] = 1.0f;
    ComparisonSamplerDesc.MinLOD = 0.f;
    ComparisonSamplerDesc.MaxLOD = 0.f;
    ComparisonSamplerDesc.MipLODBias = 0.f;
    ComparisonSamplerDesc.MaxAnisotropy = 0;
    ComparisonSamplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    ComparisonSamplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
    HRESULT hr = FEngineLoop::GraphicDevice.Device->CreateSamplerState(&ComparisonSamplerDesc, &PointShadowComparisonSampler);
    if (FAILED(hr))
    {
        MessageBox(GEngineLoop.AppWnd, L"[FStaticMeshRenderPass::CreateSamplers] Failed to create ComparisonSampler", L"Error", MB_OK);
        return;
    }

    // 조명 타입에 따라 다른 샘플러를 쓸 경우를 고려해서 일단 따로 생성
    hr = FEngineLoop::GraphicDevice.Device->CreateSamplerState(&ComparisonSamplerDesc, &SpotShadowComparisonSampler);
    if (FAILED(hr))
    {
        MessageBox(GEngineLoop.AppWnd, L"[FStaticMeshRenderPass::CreateSamplers] Failed to create ComparisonSampler", L"Error", MB_OK);
        return;
    }
    
    hr = FEngineLoop::GraphicDevice.Device->CreateSamplerState(&ComparisonSamplerDesc, &DirectionalShadowComparisonSampler);
    if (FAILED(hr))
    {
        MessageBox(GEngineLoop.AppWnd, L"[FStaticMeshRenderPass::CreateSamplers] Failed to create ComparisonSampler", L"Error", MB_OK);
        return;
    }
}

void FStaticMeshRenderPass::PrepareRenderPass(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    const EResourceType ResourceType = EResourceType::ERT_Scene;
    FViewportResource* ViewportResource = Viewport->GetViewportResource();
    FRenderTargetRHI* RenderTargetRHI = ViewportResource->GetRenderTarget(ResourceType);
    FDepthStencilRHI* DepthStencilRHI = ViewportResource->GetDepthStencil(ResourceType);
    
    // Setup Viewport and RTV
    Graphics->DeviceContext->RSSetViewports(1, &ViewportResource->GetD3DViewport());
    Graphics->DeviceContext->OMSetRenderTargets(1, &RenderTargetRHI->RTV, DepthStencilRHI->DSV);

    PrepareRenderState(Viewport);

    UpdateShadowConstant(Viewport);

    // Update Show Flag Buffer
    FShowFlagBuffer ShowFlagBuffer;
    ShowFlagBuffer.ShowFlag = Viewport->GetShowFlag();
    BufferManager->UpdateConstantBuffer(TEXT("FShowFlagBuffer"), ShowFlagBuffer);
}

void FStaticMeshRenderPass::CleanUpRenderPass(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
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

    // for Shadow Map
    Graphics->DeviceContext->PSSetShaderResources(12, 1, NullSRV);
    Graphics->DeviceContext->PSSetShaderResources(13, 1, NullSRV);
    Graphics->DeviceContext->PSSetShaderResources(14, 1, NullSRV);
}

void FStaticMeshRenderPass::ChangeViewMode(EViewModeIndex ViewMode)
{
    ID3D11VertexShader* VertexShader = nullptr;
    ID3D11InputLayout* InputLayout = nullptr;
    ID3D11PixelShader* PixelShader = nullptr;
    
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
        TEXT("FShowFlagBuffer"),
    };

    BufferManager->BindConstantBuffers(PSBufferKeys, 0, EShaderStage::Pixel);

    BufferManager->BindConstantBuffer(TEXT("FLightInfoBuffer"), 0, EShaderStage::Vertex);
    BufferManager->BindConstantBuffer(TEXT("FMaterialConstants"), 1, EShaderStage::Vertex);
}

void FStaticMeshRenderPass::UpdateLitUnlitConstant(int32 bIsLit) const
{
    FLitUnlitConstants Data;
    Data.bIsLit = bIsLit;
    BufferManager->UpdateConstantBuffer(TEXT("FLitUnlitConstants"), Data);
}

void FStaticMeshRenderPass::UpdateShadowConstant(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    // DirectionLight
    Graphics->DeviceContext->PSSetSamplers(12, 1, &DirectionalShadowComparisonSampler);
    auto DirLightRange = TObjectRange<UDirectionalLightComponent>();
    if (begin(DirLightRange) != end(DirLightRange))
    {
        UDirectionalLightComponent* DirLightComp = *DirLightRange.Begin;
        ID3D11ShaderResourceView* ShadowMapSRV = DirLightComp->GetShadowDepthMap().SRV;
        Graphics->DeviceContext->PSSetShaderResources(12, 1, &ShadowMapSRV);
    }

    // PointLight
    Graphics->DeviceContext->PSSetSamplers(14, 1, &PointShadowComparisonSampler);
    TArray<ID3D11ShaderResourceView*> ShadowCubeSRV;
    TArray<ID3D11ShaderResourceView*> MomentCubeSRV;
    ID3D11SamplerState* VSMSampler = nullptr;

    TArray<UPointLightComponent*> PointLights;
    for (auto light :  TObjectRange<UPointLightComponent>())
    {
        PointLights.Add(light);
        ShadowCubeSRV.Add(light->PointShadowSRV);
        MomentCubeSRV.Add(light->PointMomentSRV);
        VSMSampler = light->PointShadowVSMSampler;
    }
    Graphics->DeviceContext->PSSetShaderResources(14, ShadowCubeSRV.Num(), ShadowCubeSRV.GetData());
    Graphics->DeviceContext->PSSetSamplers(15, 1, &VSMSampler);
    UpdateSpotLightSRV();
}

void FStaticMeshRenderPass::UpdateSpotLightSRV()
{
    // 개별로 존재하는 리소스들을 카피해서 연속적인 메모리 공간에 배치한 후에 GPU로 전달
    Graphics->DeviceContext->PSSetSamplers(13, 1, &SpotShadowComparisonSampler);
    
    TArray<ID3D11Texture2D*> SpotDepthTextures;
    for (USpotLightComponent* SpotLight : TObjectRange<USpotLightComponent>())
    {
        SpotDepthTextures.Add(SpotLight->GetShadowDepthMap().Texture2D);
    }

    const uint32 SliceCount = static_cast<uint32>(SpotDepthTextures.Num());
    if (SliceCount == 0)
    {
        if (CachedSpotShadowArrayTex)
        {
            CachedSpotShadowArrayTex->Release();
            CachedSpotShadowArrayTex = nullptr;
        }
        if (CachedSpotShadowArraySRV)
        {
            CachedSpotShadowArraySRV->Release();
            CachedSpotShadowArraySRV = nullptr;
        }
        CachedDepthRTs.Empty();

        ID3D11ShaderResourceView* NullSRV[1] = { nullptr };
        Graphics->DeviceContext->PSSetShaderResources(13, 1, NullSRV);
        
        ID3D11SamplerState* NullSampler[1] = { nullptr };
        Graphics->DeviceContext->PSSetSamplers(13, 1, NullSampler);

        CachedSpotCount = 0;
        return;
    }
    
    bool bCountChanged = (SliceCount != CachedSpotCount);
    bool bOrderChanged = false;
    if (!bCountChanged)
    {
        // 주소 배열 비교
        for (uint32 i = 0; i < SliceCount; ++i)
        {
            if (CachedDepthRTs[i] != SpotDepthTextures[i])
            {
                bOrderChanged = true;
                break;
            }
        }
    }

    if (bCountChanged || bOrderChanged)
    {
        // 1) 기존 리소스 해제
        if (CachedSpotShadowArraySRV)
        {
            CachedSpotShadowArraySRV->Release();
            CachedSpotShadowArraySRV = nullptr;
        }
        if (CachedSpotShadowArrayTex)
        {
            CachedSpotShadowArrayTex->Release();
            CachedSpotShadowArrayTex = nullptr;
        }

        // 2) 텍스처 배열 & SRV 재생성
        D3D11_TEXTURE2D_DESC td = {};
        SpotDepthTextures[0]->GetDesc(&td);
        td.ArraySize        = SliceCount;
        td.MipLevels        = 1;
        td.Format           = DXGI_FORMAT_R32_TYPELESS;
        td.BindFlags        = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        td.Usage            = D3D11_USAGE_DEFAULT;
        td.SampleDesc.Count = 1;
        HRESULT hr = Graphics->Device->CreateTexture2D(&td, nullptr, &CachedSpotShadowArrayTex);
        assert(SUCCEEDED(hr));

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension                  = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        srvDesc.Format                         = DXGI_FORMAT_R32_FLOAT;
        srvDesc.Texture2DArray.MipLevels       = 1;
        srvDesc.Texture2DArray.MostDetailedMip = 0;
        srvDesc.Texture2DArray.FirstArraySlice = 0;
        srvDesc.Texture2DArray.ArraySize       = SliceCount;
        hr = Graphics->Device->CreateShaderResourceView(
            CachedSpotShadowArrayTex, &srvDesc, &CachedSpotShadowArraySRV
        );
        assert(SUCCEEDED(hr));
        
        // 3) 캐시 갱신
        CachedSpotCount = SliceCount;
        CachedDepthRTs  = SpotDepthTextures;
    }
    
    // 4) 매 프레임: 각 슬라이스에 최신 뎁스맵 복사
    for (UINT i = 0; i < SliceCount; ++i)
    {
        UINT dstSub = D3D11CalcSubresource(0, i, 1);
        Graphics->DeviceContext->CopySubresourceRegion(
            CachedSpotShadowArrayTex,  // dst 텍스처 배열
            dstSub,                    // dst 슬라이스
            0, 0, 0,                   // dst 오프셋
            SpotDepthTextures[i],      // src 뎁스맵 텍스처
            0,                         // src 서브리소스 인덱스
            nullptr                    // src 영역 전체 복사
        );
    }

    // 5) 바인딩
    Graphics->DeviceContext->PSSetShaderResources(13, 1, &CachedSpotShadowArraySRV);
}
