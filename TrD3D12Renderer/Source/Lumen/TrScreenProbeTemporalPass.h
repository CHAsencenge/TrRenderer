#pragma once

#include "Backend/TrComputePipeline.h"
#include "Passes/TrRenderPass.h"
#include "TrScreenProbeResources.h"

struct alignas(16) TrScreenProbeTemporalConstants
{
    UINT ProbeCountX = 0;
    UINT ProbeCountY = 0;
    UINT HistoryValid = 0;
    UINT FrameNumber = 0;
    float StaticHistoryWeight = 0.9f;
    float NormalSimilarityThreshold = 0.8f;
    float RelativePositionThreshold = 0.03f;
    float MinimumPositionThreshold = 0.05f;
};

static_assert(sizeof(TrScreenProbeTemporalConstants) == 32);

struct TrScreenProbeTemporalPassOutputs
{
    TrTexture* Irradiance = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE IrradianceSrv = {};
};

// Reprojects the previous probe irradiance into the current screen-probe grid.
// Position and normal histories identify whether a reprojected probe still
// represents the same surface before its lighting is accumulated.
class TrScreenProbeTemporalPass : public TrRenderPass
{
public:
    TrScreenProbeTemporalPass() : TrRenderPass("Probe Temporal") {}

    using Outputs = TrScreenProbeTemporalPassOutputs;

    void Initialize(ID3D12Device* device, const std::wstring& shaderPath);
    Outputs Resolve(
        ID3D12GraphicsCommandList* commandList,
        TrDescriptorHeap& resourceHeap,
        D3D12_GPU_VIRTUAL_ADDRESS viewConstants,
        UINT frameNumber,
        TrScreenProbeResources& screenProbes);

private:
    TrComputePipeline mPipeline;
};
