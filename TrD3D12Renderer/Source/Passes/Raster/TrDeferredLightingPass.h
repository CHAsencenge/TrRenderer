#pragma once

#include "Resources/TrDeferredRenderTargets.h"
#include "Backend/TrGraphicsPipeline.h"
#include "Lumen/TrScreenProbeResources.h"
#include "Renderer/TrRenderConstants.h"

class TrDeferredLightingPass
{
public:
    void Initialize(ID3D12Device* device, const std::wstring& shaderPath);
    void Render(
        ID3D12GraphicsCommandList* commandList,
        TrDeferredRenderTargets& renderTargets,
        TrDescriptorHeap& resourceHeap,
        D3D12_GPU_VIRTUAL_ADDRESS sceneConstants,
        D3D12_GPU_VIRTUAL_ADDRESS viewConstants,
        D3D12_GPU_VIRTUAL_ADDRESS passConstants,
        TrScreenProbeResources& screenProbes);

private:
    TrGraphicsPipeline mPipeline;
};
