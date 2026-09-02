#pragma once

#include "Resources/TrDeferredRenderTargets.h"
#include "Backend/TrGraphicsPipeline.h"
#include "Passes/TrRenderPass.h"
#include "Renderer/TrRenderConstants.h"

class TrGBufferPass : public TrRenderPass
{
public:
    TrGBufferPass() : TrRenderPass("GBuffer") {}

    void Initialize(ID3D12Device* device, const std::wstring& shaderPath);

    void Begin(
        ID3D12GraphicsCommandList* commandList,
        TrDeferredRenderTargets& renderTargets,
        TrDescriptorHeap& resourceHeap,
        TrDescriptorHeap& samplerHeap,
        D3D12_GPU_VIRTUAL_ADDRESS viewConstants,
        D3D12_GPU_VIRTUAL_ADDRESS passConstants,
        bool preserveDepthNormal);
    void BeginPrepassedDraws(
        ID3D12GraphicsCommandList* commandList,
        D3D12_GPU_VIRTUAL_ADDRESS viewConstants,
        D3D12_GPU_VIRTUAL_ADDRESS passConstants);
    void SetDrawBindings(
        ID3D12GraphicsCommandList* commandList,
        D3D12_GPU_VIRTUAL_ADDRESS primitiveConstants,
        D3D12_GPU_VIRTUAL_ADDRESS materialConstants,
        D3D12_GPU_DESCRIPTOR_HANDLE textureTable,
        D3D12_GPU_DESCRIPTOR_HANDLE samplerTable,
        const TrDrawConstants& drawConstants);
    TrDepthNormalView End(
        ID3D12GraphicsCommandList* commandList,
        TrDeferredRenderTargets& renderTargets);

    ID3D12PipelineState* GetPipelineState() const
    {
        return mDepthWritePipeline.GetPipelineState();
    }

private:
    TrGraphicsPipeline mDepthWritePipeline;
    TrGraphicsPipeline mDepthEqualPipeline;
};
