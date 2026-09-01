#pragma once

#include "Backend/TrComputePipeline.h"
#include "Resources/TrDepthNormalView.h"
#include "Resources/TrHierarchicalDepth.h"

struct TrScreenTracePassInputs
{
    const TrHierarchicalDepth* HierarchicalDepth = nullptr;
    TrDepthNormalView DepthNormal;
};

// Kept deliberately generic until the ray layout and hit encoding are chosen.
struct TrScreenTracePassOutputs
{
    TrTexture* TraceResult = nullptr;
};

class TrScreenTracePass
{
public:
    using Inputs = TrScreenTracePassInputs;
    using Outputs = TrScreenTracePassOutputs;

private:
    TrComputePipeline mPipeline;
};
