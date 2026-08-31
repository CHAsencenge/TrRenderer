#pragma once

#include "TrDeferredRenderTargets.h"
#include "TrGraphicsPipeline.h"
#include "TrRenderConstants.h"

class TrGBufferPass
{
public:
    void Initialize(ID3D12Device* device, const std::wstring& shaderPath);

    void Begin(
        ID3D12GraphicsCommandList* commandList,
        TrDeferredRenderTargets& renderTargets,
        D3D12_GPU_VIRTUAL_ADDRESS viewConstants,
        D3D12_GPU_VIRTUAL_ADDRESS passConstants,
        D3D12_GPU_VIRTUAL_ADDRESS primitiveConstants,
        D3D12_GPU_VIRTUAL_ADDRESS materialConstants,
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
