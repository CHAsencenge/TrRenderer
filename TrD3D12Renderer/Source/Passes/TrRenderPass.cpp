#include "TrRenderPass.h"

#include <stdexcept>

TrRenderPass::TrRenderPass(const char* profileName) :
    mProfileName(profileName != nullptr ? profileName : "")
{
    if(mProfileName.empty())
    {
        throw std::invalid_argument("Render pass profile name cannot be empty.");
    }
}

TrProfileScope TrRenderPass::Profile(
    TrPerformanceMonitor& performanceMonitor,
    ID3D12GraphicsCommandList* commandList) const
{
    if(mProfileScopeIndex == UINT_MAX)
    {
        mProfileScopeIndex = performanceMonitor.RegisterScope(
            mProfileName.c_str());
    }
    return performanceMonitor.Profile(mProfileScopeIndex, commandList);
}
