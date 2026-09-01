#pragma once

#include "Backend/TrComputePipeline.h"
#include "Resources/TrHierarchicalDepth.h"
#include "TrScreenProbeResources.h"

struct TrScreenTracePassInputs
{
    const TrHierarchicalDepth* HierarchicalDepth = nullptr;
    const TrScreenProbeResources* ScreenProbes = nullptr;
};

struct TrScreenTracePassOutputs
{
    TrTexture* TraceResult = nullptr;
};

struct alignas(16) TrScreenTraceConstants
{
    UINT ProbeCountX = 0;
    UINT ProbeCountY = 0;
    UINT TraceAtlasWidth = 0;
    UINT TraceAtlasHeight = 0;
    UINT RayGridDimension = 0;
    UINT HzbMipCount = 0;
    UINT StartMip = 0;
    UINT MaxIterations = 0;
    float MaxTraceDistance = 20.0f;
    float SurfaceBias = 0.02f;
    float SurfaceThickness = 0.08f;
    float BaseStep = 0.04f;
};

static_assert(sizeof(TrScreenTraceConstants) == 48);

class TrScreenTracePass
{
public:
    using Inputs = TrScreenTracePassInputs;
    using Outputs = TrScreenTracePassOutputs;

    void Initialize(ID3D12Device* device, const std::wstring& shaderPath);
    Outputs Trace(
        ID3D12GraphicsCommandList* commandList,
        TrDescriptorHeap& resourceHeap,
        D3D12_GPU_VIRTUAL_ADDRESS viewConstants,
        const Inputs& inputs,
        TrScreenProbeResources& screenProbes);

private:
    TrComputePipeline mPipeline;
};
