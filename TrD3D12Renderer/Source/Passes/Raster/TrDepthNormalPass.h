#pragma once

#include "Backend/TrGraphicsPipeline.h"
#include "Renderer/TrRenderConstants.h"
#include "Resources/TrDeferredRenderTargets.h"

class TrDepthNormalPass
{
public:
    void Initialize(ID3D12Device* device, const std::wstring& shaderPath);
    void Begin(
        ID3D12GraphicsCommandList* commandList,
        TrDeferredRenderTargets& renderTargets,
        TrDescriptorHeap& resourceHeap,
        TrDescriptorHeap& samplerHeap,
        D3D12_GPU_VIRTUAL_ADDRESS viewConstants);
    void SetDrawBindings(
        ID3D12GraphicsCommandList* commandList,
        D3D12_GPU_VIRTUAL_ADDRESS primitiveConstants,
        D3D12_GPU_VIRTUAL_ADDRESS materialConstants,
        D3D12_GPU_DESCRIPTOR_HANDLE textureTable,
        D3D12_GPU_DESCRIPTOR_HANDLE samplerTable,
        const TrDrawConstants& drawConstants);
    void End(
        ID3D12GraphicsCommandList* commandList,
        TrDeferredRenderTargets& renderTargets);

    ID3D12PipelineState* GetPipelineState() const
    {
        return mPipeline.GetPipelineState();
    }

private:
    TrGraphicsPipeline mPipeline;
};
