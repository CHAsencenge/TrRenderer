#pragma once

#include "Backend/TrComputePipeline.h"
#include "Passes/TrRenderPass.h"
#include "Resources/TrDepthNormalView.h"
#include "Resources/TrHierarchicalDepth.h"

struct TrHzbPassInputs
{
    TrDepthNormalView DepthNormal;
};

struct TrHzbPassOutputs
{
    TrHierarchicalDepth* HierarchicalDepth = nullptr;
};

struct alignas(16) TrHzbBuildConstants
{
    UINT SourceWidth = 0;
    UINT SourceHeight = 0;
    UINT DestinationWidth = 0;
    UINT DestinationHeight = 0;
};

static_assert(sizeof(TrHzbBuildConstants) == 16);

class TrHzbPass : public TrRenderPass
{
public:
    TrHzbPass() : TrRenderPass("HZB") {}

    using Inputs = TrHzbPassInputs;
    using Outputs = TrHzbPassOutputs;

    void Initialize(ID3D12Device* device, const std::wstring& shaderPath);
    Outputs Build(
        ID3D12GraphicsCommandList* commandList,
        TrDescriptorHeap& resourceHeap,
        const Inputs& inputs,
        TrHierarchicalDepth& hierarchicalDepth);

private:
    TrComputePipeline mPipeline;
};
