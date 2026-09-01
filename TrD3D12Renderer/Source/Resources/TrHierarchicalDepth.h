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
    static constexpr UINT MaximumMipCount = 15;

    void Initialize(
        ID3D12Device* device,
        UINT width,
        UINT height,
        TrDescriptorHeap& resourceHeap);
    void Resize(ID3D12Device* device, UINT width, UINT height);

    const TrHierarchicalDepthDesc& GetDescription() const
    {
        return mDescription;
    }

    TrTexture& GetTexture() { return mTexture; }
    const TrTexture& GetTexture() const { return mTexture; }
    const TrDescriptorAllocation& GetSrv() const { return mSrv; }
    const TrDescriptorAllocation& GetMipSrv(UINT mip) const;
    const TrDescriptorAllocation& GetMipUav(UINT mip) const;
    UINT GetMipWidth(UINT mip) const;
    UINT GetMipHeight(UINT mip) const;

private:
    static UINT CalculateMipCount(UINT width, UINT height);
    void CreateResource(ID3D12Device* device, UINT width, UINT height);

    TrHierarchicalDepthDesc mDescription;
    TrTexture mTexture;
    TrDescriptorAllocation mSrv;
    std::vector<TrDescriptorAllocation> mMipSrvs;
    std::vector<TrDescriptorAllocation> mMipUavs;
};
