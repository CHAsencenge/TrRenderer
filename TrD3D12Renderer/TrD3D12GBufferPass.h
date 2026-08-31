#pragma once

#include "TrD3D12DeferredRenderTargets.h"
#include "TrD3D12GraphicsPipeline.h"
#include "TrD3D12RenderConstants.h"

class TrD3D12GBufferPass
{
public:
    void Initialize(ID3D12Device* device, const std::wstring& shaderPath);

    void Begin(
        ID3D12GraphicsCommandList* commandList,
        TrD3D12DeferredRenderTargets& renderTargets,
        D3D12_GPU_VIRTUAL_ADDRESS viewConstants,
        D3D12_GPU_VIRTUAL_ADDRESS passConstants,
        D3D12_GPU_VIRTUAL_ADDRESS primitiveConstants,
        D3D12_GPU_VIRTUAL_ADDRESS materialConstants,
        const TrD3D12DrawConstants& drawConstants);
    void End(
        ID3D12GraphicsCommandList* commandList,
        TrD3D12DeferredRenderTargets& renderTargets);

    ID3D12PipelineState* GetPipelineState() const
    {
        return mPipeline.GetPipelineState();
    }

private:
    TrD3D12GraphicsPipeline mPipeline;
};
