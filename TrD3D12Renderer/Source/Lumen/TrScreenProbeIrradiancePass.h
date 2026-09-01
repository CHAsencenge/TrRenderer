#pragma once

#include "Backend/TrComputePipeline.h"
#include "TrScreenProbeResources.h"

struct alignas(16) TrScreenProbeIrradianceConstants
{
    UINT ProbeCountX = 0;
    UINT ProbeCountY = 0;
    UINT RayGridDimension = 0;
    UINT RaysPerProbe = 0;
};

static_assert(sizeof(TrScreenProbeIrradianceConstants) == 16);

struct TrScreenProbeIrradiancePassOutputs
{
    TrTexture* Irradiance = nullptr;
};

// Integrates cosine-weighted per-ray radiance into one diffuse irradiance
// value per screen probe. Temporal accumulation is intentionally separate.
class TrScreenProbeIrradiancePass
{
public:
    using Outputs = TrScreenProbeIrradiancePassOutputs;

    void Initialize(ID3D12Device* device, const std::wstring& shaderPath);
    Outputs Integrate(
        ID3D12GraphicsCommandList* commandList,
        TrDescriptorHeap& resourceHeap,
        TrScreenProbeResources& screenProbes);

private:
    TrComputePipeline mPipeline;
};
