#pragma once
#include "Launch/EngineLoop.h"

enum class EShaderSRVSlot : int8
{
    SRV_SceneDepth = 99,
    SRV_Scene = 100,
    SRV_PostProcess = 101,
    SRV_EditorOverlay = 102,
    SRV_Fog = 103,
    SRV_Debug = 104,
    
    SRV_Viewport = 120,

    SRV_MAX = 127,
};

namespace MaterialUtils
{
    inline void UpdateMaterial(FDXDBufferManager* BufferManager, FGraphicsDevice* Graphics, const FObjMaterialInfo& MaterialInfo)
    {
        FMaterialConstants Data;
        
        Data.TextureFlag = MaterialInfo.TextureFlag;
        
        Data.DiffuseColor = MaterialInfo.DiffuseColor;
        
        Data.SpecularColor = MaterialInfo.SpecularColor;
        Data.SpecularExponent = MaterialInfo.SpecularExponent;
        
        Data.EmissiveColor = MaterialInfo.EmissiveColor;
        Data.Transparency = MaterialInfo.Transparency;

        BufferManager->UpdateConstantBuffer(TEXT("FMaterialConstants"), Data);
        
        // Update Textures
        if (MaterialInfo.TextureFlag & static_cast<uint16>(EMaterialTextureFlags::Diffuse))
        {
            std::shared_ptr<FTexture> texture = FEngineLoop::ResourceManager.GetTexture(MaterialInfo.DiffuseColorTexturePath);
            Graphics->DeviceContext->PSSetShaderResources(0, 1, &texture->TextureSRV);
            Graphics->DeviceContext->PSSetSamplers(0, 1, &texture->SamplerState);

            // for Gouraud shading
            Graphics->DeviceContext->VSSetShaderResources(0, 1, &texture->TextureSRV);
            Graphics->DeviceContext->VSSetSamplers(0, 1, &texture->SamplerState);
        }
        else
        {
            ID3D11ShaderResourceView* NullSRV[1] = { nullptr };
            ID3D11SamplerState* NullSampler[1] = { nullptr };
            Graphics->DeviceContext->PSSetShaderResources(0, 1, NullSRV);
            Graphics->DeviceContext->PSSetSamplers(0, 1, NullSampler);

        }
        
        if (MaterialInfo.TextureFlag & static_cast<uint16>(EMaterialTextureFlags::Normal))
        {
            std::shared_ptr<FTexture> texture = FEngineLoop::ResourceManager.GetTexture(MaterialInfo.BumpTexturePath);
            Graphics->DeviceContext->PSSetShaderResources(1, 1, &texture->TextureSRV);
            Graphics->DeviceContext->PSSetSamplers(1, 1, &texture->SamplerState);
        }
        else {
            ID3D11ShaderResourceView* NullSRV[1] = { nullptr };
            ID3D11SamplerState* NullSampler[1] = { nullptr };
            Graphics->DeviceContext->PSSetShaderResources(1, 1, NullSRV);
            Graphics->DeviceContext->PSSetSamplers(1, 1, NullSampler);
        }
    }
}
