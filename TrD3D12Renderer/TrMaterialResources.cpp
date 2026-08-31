#include "TrMaterialResources.h"

#include "TrImageDecoder.h"
#include "TrLog.h"
#include "TrUploadContext.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{
    enum TextureSlot : std::size_t
    {
        BaseColorSlot,
        MetallicRoughnessSlot,
        NormalSlot,
        OcclusionSlot,
        EmissiveSlot,
        TextureSlotCount
    };

    using TextureBindingArray =
        std::array<const TrSceneTextureBinding*, TextureSlotCount>;

    TextureBindingArray GetTextureBindings(const TrSceneMaterial* material)
    {
        if(material == nullptr)
        {
            return {nullptr, nullptr, nullptr, nullptr, nullptr};
        }
        return
        {
            &material->BaseColorTexture,
            &material->MetallicRoughnessTexture,
            &material->NormalTexture,
            &material->OcclusionTexture,
            &material->EmissiveTexture
        };
    }

    void CopyTransform(
        TrMaterialConstants::TextureTransform& destination,
        const TrSceneTextureBinding& source)
    {
        destination.Offset = {source.Offset[0], source.Offset[1]};
        destination.Scale = {source.Scale[0], source.Scale[1]};
        destination.Rotation = source.Rotation;
        destination.TexCoord = source.TexCoord >= 0
            ? static_cast<std::uint32_t>(source.TexCoord)
            : 0;
        destination.Strength = source.Strength;
    }

    TrMaterialConstants CreateConstants(
        const TrSceneMaterial* material,
        bool gltfDefaults)
    {
        TrMaterialConstants constants;
        if(material == nullptr)
        {
            if(gltfDefaults)
            {
                constants.Roughness = 1.0f;
                constants.Metallic = 1.0f;
            }
            return constants;
        }

        constants.BaseColorFactor =
        {
            material->BaseColorFactor[0],
            material->BaseColorFactor[1],
            material->BaseColorFactor[2],
            material->BaseColorFactor[3]
        };
        constants.EmissiveFactor =
        {
            material->EmissiveFactor[0],
            material->EmissiveFactor[1],
            material->EmissiveFactor[2]
        };
        constants.EmissiveStrength = material->EmissiveStrength;
        constants.Roughness = material->RoughnessFactor;
        constants.Metallic = material->MetallicFactor;
        constants.AlphaCutoff = material->AlphaCutoff;
        constants.Flags =
            (material->Unlit ? 1u : 0u) |
            (material->DoubleSided ? 2u : 0u) |
            (material->AlphaMode == TrSceneAlphaMode::Mask ? 4u : 0u) |
            (material->AlphaMode == TrSceneAlphaMode::Blend ? 8u : 0u);
        CopyTransform(constants.BaseColorTexture, material->BaseColorTexture);
        CopyTransform(
            constants.MetallicRoughnessTexture,
            material->MetallicRoughnessTexture);
        CopyTransform(constants.NormalTexture, material->NormalTexture);
        CopyTransform(constants.OcclusionTexture, material->OcclusionTexture);
        CopyTransform(constants.EmissiveTexture, material->EmissiveTexture);
        return constants;
    }

    D3D12_TEXTURE_ADDRESS_MODE ConvertAddressMode(std::uint32_t wrapMode)
    {
        switch(wrapMode)
        {
            case 33071: return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            case 33648: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
            default: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        }
    }

    D3D12_FILTER_TYPE ConvertMinFilter(std::uint32_t filter)
    {
        return filter == 9728 || filter == 9984 || filter == 9986
            ? D3D12_FILTER_TYPE_POINT
            : D3D12_FILTER_TYPE_LINEAR;
    }

    D3D12_FILTER_TYPE ConvertMagFilter(std::uint32_t filter)
    {
        return filter == 9728
            ? D3D12_FILTER_TYPE_POINT
            : D3D12_FILTER_TYPE_LINEAR;
    }

    D3D12_FILTER_TYPE ConvertMipFilter(std::uint32_t filter)
    {
        return filter == 9984 || filter == 9985
            ? D3D12_FILTER_TYPE_POINT
            : D3D12_FILTER_TYPE_LINEAR;
    }

    D3D12_SAMPLER_DESC CreateSamplerDesc(const TrSceneSampler* sampler)
    {
        D3D12_SAMPLER_DESC description = {};
        const std::uint32_t minFilter = sampler != nullptr ? sampler->MinFilter : 0;
        const std::uint32_t magFilter = sampler != nullptr ? sampler->MagFilter : 0;
        description.Filter = D3D12_ENCODE_BASIC_FILTER(
            ConvertMinFilter(minFilter),
            ConvertMagFilter(magFilter),
            ConvertMipFilter(minFilter),
            D3D12_FILTER_REDUCTION_TYPE_STANDARD);
        description.AddressU = ConvertAddressMode(sampler != nullptr ? sampler->WrapU : 10497);
        description.AddressV = ConvertAddressMode(sampler != nullptr ? sampler->WrapV : 10497);
        description.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        description.MaxAnisotropy = 1;
        description.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        description.MinLOD = 0.0f;
        description.MaxLOD = D3D12_FLOAT32_MAX;
        return description;
    }

    const TrSceneTexture* ResolveTexture(
        const TrScene* scene,
        const TrSceneTextureBinding* binding)
    {
        if(scene == nullptr || binding == nullptr || binding->TextureIndex < 0)
        {
            return nullptr;
        }
        return &scene->Textures[static_cast<std::size_t>(binding->TextureIndex)];
    }

    const TrSceneSampler* ResolveSampler(
        const TrScene* scene,
        const TrSceneTexture* texture)
    {
        if(scene == nullptr || texture == nullptr || texture->SamplerIndex < 0)
        {
            return nullptr;
        }
        return &scene->Samplers[static_cast<std::size_t>(texture->SamplerIndex)];
    }
}

void TrMaterialResources::Initialize(
    ID3D12Device* device,
    TrUploadContext& uploadContext,
    TrDescriptorHeap& resourceHeap,
    TrDescriptorHeap& samplerHeap,
    const TrScene* scene)
{
    if(device == nullptr || resourceHeap.Get() == nullptr || samplerHeap.Get() == nullptr ||
       !resourceHeap.IsShaderVisible() || !samplerHeap.IsShaderVisible() ||
       resourceHeap.GetType() != D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ||
       samplerHeap.GetType() != D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
    {
        throw std::invalid_argument("Material resource initialization is incomplete.");
    }

    mImages.clear();
    mFallbackImages.clear();
    mConstantBuffers.clear();
    mMaterials.clear();

    const std::size_t sourceViewCount =
        (scene != nullptr ? scene->Images.size() : 0) + 3;
    if(sourceViewCount > std::numeric_limits<UINT>::max() / 2)
    {
        throw std::overflow_error("Scene contains too many material images.");
    }
    mTextureViewHeap.Initialize(
        device,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        static_cast<UINT>(sourceViewCount * 2),
        false,
        L"Material Texture View Staging Heap");

    auto createGpuImage = [this, device, &uploadContext](
        const std::uint8_t* pixels,
        UINT width,
        UINT height,
        const wchar_t* debugName)
    {
        auto image = std::make_unique<TrGpuImage>();
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        uploadContext.UploadTexture2D(
            pixels,
            static_cast<UINT64>(width) * 4,
            static_cast<UINT64>(width) * height * 4,
            width,
            height,
            DXGI_FORMAT_R8G8B8A8_TYPELESS,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            resource);
        image->Texture = std::make_unique<TrTexture>();
        image->Texture->Attach(
            resource.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            debugName);
        image->LinearSrv = mTextureViewHeap.Allocate();
        image->SrgbSrv = mTextureViewHeap.Allocate();
        image->Texture->CreateShaderResourceView(
            device,
            image->LinearSrv.CpuHandle,
            DXGI_FORMAT_R8G8B8A8_UNORM);
        image->Texture->CreateShaderResourceView(
            device,
            image->SrgbSrv.CpuHandle,
            DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
        return image;
    };

    const std::array<std::uint8_t, 4> white = {255, 255, 255, 255};
    const std::array<std::uint8_t, 4> neutralNormal = {128, 128, 255, 255};
    const std::array<std::uint8_t, 4> black = {0, 0, 0, 255};
    mFallbackImages.push_back(createGpuImage(white.data(), 1, 1, L"Material White Fallback"));
    mFallbackImages.push_back(createGpuImage(
        neutralNormal.data(), 1, 1, L"Material Normal Fallback"));
    mFallbackImages.push_back(createGpuImage(black.data(), 1, 1, L"Material Black Fallback"));

    if(scene != nullptr)
    {
        mImages.resize(scene->Images.size());
        std::vector<bool> referencedImages(scene->Images.size(), false);
        for(const TrSceneMaterial& material : scene->Materials)
        {
            for(const TrSceneTextureBinding* binding : GetTextureBindings(&material))
            {
                const TrSceneTexture* texture = ResolveTexture(scene, binding);
                if(texture != nullptr && texture->ImageIndex >= 0)
                {
                    referencedImages[static_cast<std::size_t>(texture->ImageIndex)] = true;
                }
            }
        }

        for(std::size_t imageIndex = 0; imageIndex < scene->Images.size(); ++imageIndex)
        {
            if(!referencedImages[imageIndex])
            {
                continue;
            }
            const TrSceneImage& source = scene->Images[imageIndex];
            if(source.Data.empty())
            {
                TrLog::Warn("Material image '" + source.Name + "' has no embedded payload; using fallback.");
                continue;
            }
            try
            {
                const TrDecodedImageRgba8 decoded = TrImageDecoder::DecodeRgba8(source.Data);
                const std::wstring debugName =
                    L"Scene Material Image " + std::to_wstring(imageIndex);
                mImages[imageIndex] = createGpuImage(
                    decoded.Pixels.data(),
                    decoded.Width,
                    decoded.Height,
                    debugName.c_str());
                TrLog::Info(
                    "Created material texture '" + source.Name + "' (" +
                    std::to_string(decoded.Width) + "x" +
                    std::to_string(decoded.Height) + ").");
            }
            catch(const std::exception& exception)
            {
                TrLog::Warn(
                    "Failed to decode material image '" + source.Name +
                    "'; using fallback. " + exception.what());
            }
        }
    }

    mFallbackMaterial = CreateMaterialBinding(
        device,
        resourceHeap,
        samplerHeap,
        scene,
        nullptr,
        scene != nullptr);
    if(scene != nullptr)
    {
        mMaterials.reserve(scene->Materials.size());
        for(const TrSceneMaterial& material : scene->Materials)
        {
            mMaterials.push_back(CreateMaterialBinding(
                device,
                resourceHeap,
                samplerHeap,
                scene,
                &material,
                true));
        }
    }
}

TrMaterialGpuBinding TrMaterialResources::CreateMaterialBinding(
    ID3D12Device* device,
    TrDescriptorHeap& resourceHeap,
    TrDescriptorHeap& samplerHeap,
    const TrScene* scene,
    const TrSceneMaterial* material,
    bool gltfDefaults)
{
    auto constantBuffer = std::make_unique<TrConstantBuffer>();
    constantBuffer->Initialize(device, static_cast<UINT>(sizeof(TrMaterialConstants)));
    constantBuffer->Update(CreateConstants(material, gltfDefaults));

    const TextureBindingArray textureBindings = GetTextureBindings(material);
    const std::array<const TrGpuImage*, TextureSlotCount> fallbackImages =
    {
        mFallbackImages[0].get(),
        mFallbackImages[0].get(),
        mFallbackImages[1].get(),
        mFallbackImages[0].get(),
        mFallbackImages[2].get()
    };
    const std::array<bool, TextureSlotCount> useSrgb =
    {
        true,
        false,
        false,
        false,
        true
    };

    TrMaterialGpuBinding result;
    result.Constants = constantBuffer->GetGpuVirtualAddress();
    UINT firstTextureIndex = UINT_MAX;
    UINT firstSamplerIndex = UINT_MAX;
    for(std::size_t slot = 0; slot < TextureSlotCount; ++slot)
    {
        const TrSceneTexture* texture = ResolveTexture(scene, textureBindings[slot]);
        const TrGpuImage* image = fallbackImages[slot];
        if(texture != nullptr && texture->ImageIndex >= 0)
        {
            const auto& loadedImage = mImages[static_cast<std::size_t>(texture->ImageIndex)];
            if(loadedImage != nullptr)
            {
                image = loadedImage.get();
            }
        }

        const TrDescriptorAllocation textureDestination = resourceHeap.Allocate();
        const TrDescriptorAllocation samplerDestination = samplerHeap.Allocate();
        if(slot == 0)
        {
            firstTextureIndex = textureDestination.Index;
            firstSamplerIndex = samplerDestination.Index;
            result.TextureTable = textureDestination.GpuHandle;
            result.SamplerTable = samplerDestination.GpuHandle;
        }
        else if(textureDestination.Index != firstTextureIndex + slot ||
                samplerDestination.Index != firstSamplerIndex + slot)
        {
            throw std::logic_error("Material texture and sampler tables must be contiguous.");
        }

        const D3D12_CPU_DESCRIPTOR_HANDLE sourceHandle = useSrgb[slot]
            ? image->SrgbSrv.CpuHandle
            : image->LinearSrv.CpuHandle;
        device->CopyDescriptorsSimple(
            1,
            textureDestination.CpuHandle,
            sourceHandle,
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        const D3D12_SAMPLER_DESC samplerDescription = CreateSamplerDesc(
            ResolveSampler(scene, texture));
        device->CreateSampler(&samplerDescription, samplerDestination.CpuHandle);
    }

    mConstantBuffers.push_back(std::move(constantBuffer));
    return result;
}

const TrMaterialGpuBinding& TrMaterialResources::Get(
    std::uint32_t materialIndex) const
{
    if(materialIndex == TrInvalidSceneIndex || materialIndex >= mMaterials.size())
    {
        return mFallbackMaterial;
    }
    return mMaterials[materialIndex];
}
