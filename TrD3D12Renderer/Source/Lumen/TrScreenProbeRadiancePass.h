#pragma once

#include "Backend/TrComputePipeline.h"
#include "Resources/TrDeferredRenderTargets.h"
#include "TrScreenProbeResources.h"

struct TrScreenProbeRadiancePassOutputs
{
    TrTexture* Radiance = nullptr;
};

// Resolves a screen-space hit into one incident-radiance sample per probe ray.
// The output is overwritten every frame and is not a temporal history.
class TrScreenProbeRadiancePass
{
public:
    using Outputs = TrScreenProbeRadiancePassOutputs;

    void Initialize(ID3D12Device* device, const std::wstring& shaderPath);
    Outputs Resolve(
        ID3D12GraphicsCommandList* commandList,
        TrDescriptorHeap& resourceHeap,
        D3D12_GPU_VIRTUAL_ADDRESS sceneConstants,
        D3D12_GPU_VIRTUAL_ADDRESS lightingPassConstants,
        const TrDeferredRenderTargets& renderTargets,
        TrScreenProbeResources& screenProbes);

private:
    TrComputePipeline mPipeline;
};
