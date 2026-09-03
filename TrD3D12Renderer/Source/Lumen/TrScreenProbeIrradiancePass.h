#pragma once

#include "Backend/TrComputePipeline.h"
#include "Passes/TrRenderPass.h"
#include "TrScreenProbeResources.h"

struct alignas(16) TrScreenProbeIrradianceConstants
{
    UINT ProbeCountX = 0;
    UINT ProbeCountY = 0;
    UINT RayGridDimension = 0;
    UINT RaysPerProbe = 0;
    UINT FrameNumber = 0;
    UINT Padding[3] = {};
};

static_assert(sizeof(TrScreenProbeIrradianceConstants) == 32);

struct TrScreenProbeIrradiancePassOutputs
{
    TrTexture* Irradiance = nullptr;
};

// Projects cosine-weighted per-ray radiance into world-space, Lambert-
// convolved SH L2 coefficients. Temporal accumulation is intentionally
// separate.
class TrScreenProbeIrradiancePass : public TrRenderPass
{
public:
    TrScreenProbeIrradiancePass() : TrRenderPass("Irradiance Integrate") {}

    using Outputs = TrScreenProbeIrradiancePassOutputs;

    void Initialize(ID3D12Device* device, const std::wstring& shaderPath);
    Outputs Integrate(
        ID3D12GraphicsCommandList* commandList,
        TrDescriptorHeap& resourceHeap,
        UINT frameNumber,
        TrScreenProbeResources& screenProbes);

private:
    TrComputePipeline mPipeline;
};
