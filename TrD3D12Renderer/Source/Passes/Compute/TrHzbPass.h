#pragma once

#include "Backend/TrComputePipeline.h"
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

// Compute-pass boundary only. Binding layout, reduction direction and dispatch
// strategy are intentionally deferred until the implementation is discussed.
class TrHzbPass
{
public:
    using Inputs = TrHzbPassInputs;
    using Outputs = TrHzbPassOutputs;

private:
    TrComputePipeline mPipeline;
};
