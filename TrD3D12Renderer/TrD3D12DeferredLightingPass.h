#pragma once

#include "TrD3D12DeferredRenderTargets.h"
#include "TrD3D12GraphicsPipeline.h"
#include "TrD3D12RenderConstants.h"

class TrD3D12DeferredLightingPass
{
public:
    void Initialize(ID3D12Device* device, const std::wstring& shaderPath);
    void Render(
        ID3D12GraphicsCommandList* commandList,
        TrD3D12DeferredRenderTargets& renderTargets,
        TrD3D12DescriptorHeap& resourceHeap,
        D3D12_GPU_VIRTUAL_ADDRESS sceneConstants,
        D3D12_GPU_VIRTUAL_ADDRESS viewConstants,
        D3D12_GPU_VIRTUAL_ADDRESS passConstants);

private:
    TrD3D12GraphicsPipeline mPipeline;
};
