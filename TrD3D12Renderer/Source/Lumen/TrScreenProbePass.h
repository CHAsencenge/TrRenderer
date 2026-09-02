#pragma once

#include "Backend/TrComputePipeline.h"
#include "Passes/TrRenderPass.h"
#include "Resources/TrDepthNormalView.h"
#include "TrScreenProbeResources.h"

struct TrScreenProbePassInputs
{
    TrDepthNormalView DepthNormal;
};

struct TrScreenProbePassOutputs
{
    TrScreenProbeResources* ScreenProbes = nullptr;
};

struct alignas(16) TrScreenProbeBuildConstants
{
    UINT RenderWidth = 0;
    UINT RenderHeight = 0;
    UINT ProbeCountX = 0;
    UINT ProbeCountY = 0;
    UINT TileSize = 0;
    UINT Padding[3] = {};
};

static_assert(sizeof(TrScreenProbeBuildConstants) == 32);

class TrScreenProbePass : public TrRenderPass
{
public:
    TrScreenProbePass() : TrRenderPass("Screen Probe Build") {}

    using Inputs = TrScreenProbePassInputs;
    using Outputs = TrScreenProbePassOutputs;

    void Initialize(ID3D12Device* device, const std::wstring& shaderPath);
    Outputs Build(
        ID3D12GraphicsCommandList* commandList,
        TrDescriptorHeap& resourceHeap,
        D3D12_GPU_VIRTUAL_ADDRESS viewConstants,
        const Inputs& inputs,
        TrScreenProbeResources& screenProbes);

private:
    TrComputePipeline mPipeline;
};
