#pragma once

#include "TrDescriptorHeap.h"
#include "TrTexture.h"

#include <vector>

// Describes the persistent mip-chain consumed by screen-space tracing.
// The exact mip policy remains a pass implementation detail.
struct TrHierarchicalDepthDesc
{
    UINT Width = 0;
    UINT Height = 0;
    UINT MipCount = 0;
    DXGI_FORMAT Format = DXGI_FORMAT_R32_FLOAT;
};

class TrHierarchicalDepth
{
public:
    const TrHierarchicalDepthDesc& GetDescription() const
    {
        return mDescription;
    }

    TrTexture& GetTexture() { return mTexture; }
    const TrTexture& GetTexture() const { return mTexture; }
    const TrDescriptorAllocation& GetSrv() const { return mSrv; }
    const std::vector<TrDescriptorAllocation>& GetMipUavs() const
    {
        return mMipUavs;
    }

private:
    TrHierarchicalDepthDesc mDescription;
    TrTexture mTexture;
    TrDescriptorAllocation mSrv;
    std::vector<TrDescriptorAllocation> mMipUavs;
};
