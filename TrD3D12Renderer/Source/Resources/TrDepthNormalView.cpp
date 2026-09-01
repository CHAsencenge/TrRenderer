#include "TrDepthNormalView.h"

#include <stdexcept>

namespace
{
    bool IsComputeReadable(D3D12_RESOURCE_STATES state)
    {
        return (state & D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) != 0;
    }
}

TrDepthNormalView::TrDepthNormalView(
    const TrTexture& depth,
    const TrDescriptorAllocation& depthSrv,
    const TrTexture& worldNormal,
    const TrDescriptorAllocation& worldNormalSrv) :
    mDepth(&depth),
    mWorldNormal(&worldNormal),
    mDepthSrv(depthSrv),
    mWorldNormalSrv(worldNormalSrv)
{
}

void TrDepthNormalView::ValidateForCompute() const
{
    if(!IsValid())
    {
        throw std::logic_error("Depth/Normal pass output is incomplete.");
    }

    const D3D12_RESOURCE_DESC& depthDesc = mDepth->GetDescription();
    const D3D12_RESOURCE_DESC& normalDesc = mWorldNormal->GetDescription();
    if(depthDesc.Width != normalDesc.Width ||
       depthDesc.Height != normalDesc.Height ||
       depthDesc.MipLevels != 1 || normalDesc.MipLevels != 1)
    {
        throw std::logic_error(
            "Depth/Normal pass outputs must have matching single-mip dimensions.");
    }
    if(!IsComputeReadable(mDepth->GetState()) ||
       !IsComputeReadable(mWorldNormal->GetState()))
    {
        throw std::logic_error(
            "Depth/Normal pass outputs are not compute-shader readable.");
    }
}

bool TrDepthNormalView::IsValid() const
{
    return mDepth != nullptr && mWorldNormal != nullptr &&
        mDepth->Get() != nullptr && mWorldNormal->Get() != nullptr &&
        mDepthSrv.Index != UINT_MAX && mWorldNormalSrv.Index != UINT_MAX &&
        mDepthSrv.GpuHandle.ptr != 0 && mWorldNormalSrv.GpuHandle.ptr != 0;
}

const TrTexture& TrDepthNormalView::GetDepth() const
{
    if(mDepth == nullptr)
    {
        throw std::logic_error("Depth/Normal view has no depth resource.");
    }
    return *mDepth;
}

const TrTexture& TrDepthNormalView::GetWorldNormal() const
{
    if(mWorldNormal == nullptr)
    {
        throw std::logic_error("Depth/Normal view has no normal resource.");
    }
    return *mWorldNormal;
}

const TrDescriptorAllocation& TrDepthNormalView::GetDepthSrv() const
{
    if(mDepthSrv.Index == UINT_MAX)
    {
        throw std::logic_error("Depth/Normal view has no depth SRV.");
    }
    return mDepthSrv;
}

const TrDescriptorAllocation& TrDepthNormalView::GetWorldNormalSrv() const
{
    if(mWorldNormalSrv.Index == UINT_MAX)
    {
        throw std::logic_error("Depth/Normal view has no normal SRV.");
    }
    return mWorldNormalSrv;
}

UINT TrDepthNormalView::GetWidth() const
{
    return static_cast<UINT>(GetDepth().GetDescription().Width);
}

UINT TrDepthNormalView::GetHeight() const
{
    return GetDepth().GetDescription().Height;
}
