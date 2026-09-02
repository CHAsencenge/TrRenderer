#pragma once

#include "TrConstantBuffer.h"
#include "TrDescriptorHeap.h"
#include "Renderer/TrRenderConstants.h"
#include "TrScene.h"
#include "TrTexture.h"

#include <cstdint>
#include <array>
#include <memory>
#include <vector>

class TrUploadContext;

struct TrMaterialGpuBinding
{
    D3D12_GPU_VIRTUAL_ADDRESS Constants = 0;
    D3D12_GPU_DESCRIPTOR_HANDLE TextureTable = {};
    D3D12_GPU_DESCRIPTOR_HANDLE SamplerTable = {};
    TrSceneAlphaMode AlphaMode = TrSceneAlphaMode::Opaque;
};

class TrMaterialResources
{
public:
    static constexpr UINT TextureSlotCount = 5;

    void Initialize(
        ID3D12Device* device,
        TrUploadContext& uploadContext,
        TrDescriptorHeap& resourceHeap,
        TrDescriptorHeap& samplerHeap,
        const TrScene* scene);

    const TrMaterialGpuBinding& Get(std::uint32_t materialIndex) const;

private:
    struct TrGpuImage
    {
        std::unique_ptr<TrTexture> Texture;
        TrDescriptorAllocation LinearSrv;
        TrDescriptorAllocation SrgbSrv;
    };

    struct TrSamplerTable
    {
        std::array<std::int32_t, TextureSlotCount> SamplerIndices = {};
        D3D12_GPU_DESCRIPTOR_HANDLE Handle = {};
    };

    TrMaterialGpuBinding CreateMaterialBinding(
        ID3D12Device* device,
        TrDescriptorHeap& resourceHeap,
        TrDescriptorHeap& samplerHeap,
        const TrScene* scene,
        const TrSceneMaterial* material,
        bool gltfDefaults);

    std::vector<std::unique_ptr<TrGpuImage>> mImages;
    std::vector<std::unique_ptr<TrGpuImage>> mFallbackImages;
    std::vector<TrSamplerTable> mSamplerTables;
    TrDescriptorHeap mTextureViewHeap;
    std::vector<std::unique_ptr<TrConstantBuffer>> mConstantBuffers;
    std::vector<TrMaterialGpuBinding> mMaterials;
    TrMaterialGpuBinding mFallbackMaterial;
};
