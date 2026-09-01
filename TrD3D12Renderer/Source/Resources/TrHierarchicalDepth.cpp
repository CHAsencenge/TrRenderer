#include "TrHierarchicalDepth.h"

#include <algorithm>
#include <stdexcept>

void TrHierarchicalDepth::Initialize(
    ID3D12Device* device,
    UINT width,
    UINT height,
    TrDescriptorHeap& resourceHeap)
{
    if(device == nullptr || width == 0 || height == 0 ||
       resourceHeap.Get() == nullptr || !resourceHeap.IsShaderVisible() ||
       resourceHeap.GetType() != D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
    {
        throw std::invalid_argument("HZB initialization inputs are invalid.");
    }
    if(mSrv.Index != UINT_MAX)
    {
        throw std::logic_error("HZB has already been initialized.");
    }

    mSrv = resourceHeap.Allocate();
    mMipSrvs.reserve(MaximumMipCount);
    mMipUavs.reserve(MaximumMipCount);
    for(UINT mip = 0; mip < MaximumMipCount; ++mip)
    {
        mMipSrvs.push_back(resourceHeap.Allocate());
        mMipUavs.push_back(resourceHeap.Allocate());
    }
    CreateResource(device, width, height);
}

void TrHierarchicalDepth::Resize(
    ID3D12Device* device,
    UINT width,
    UINT height)
{
    if(mSrv.Index == UINT_MAX || mMipSrvs.size() != MaximumMipCount ||
       mMipUavs.size() != MaximumMipCount)
    {
        throw std::logic_error("HZB has not been initialized.");
    }
    CreateResource(device, width, height);
}

const TrDescriptorAllocation& TrHierarchicalDepth::GetMipSrv(UINT mip) const
{
    if(mip >= mDescription.MipCount)
    {
        throw std::out_of_range("HZB SRV mip is outside the active chain.");
    }
    return mMipSrvs[mip];
}

const TrDescriptorAllocation& TrHierarchicalDepth::GetMipUav(UINT mip) const
{
    if(mip >= mDescription.MipCount)
    {
        throw std::out_of_range("HZB UAV mip is outside the active chain.");
    }
    return mMipUavs[mip];
}

UINT TrHierarchicalDepth::GetMipWidth(UINT mip) const
{
    if(mip >= mDescription.MipCount)
    {
        throw std::out_of_range("HZB width mip is outside the active chain.");
    }
    return std::max(1u, mDescription.Width >> mip);
}

UINT TrHierarchicalDepth::GetMipHeight(UINT mip) const
{
    if(mip >= mDescription.MipCount)
    {
        throw std::out_of_range("HZB height mip is outside the active chain.");
    }
    return std::max(1u, mDescription.Height >> mip);
}

UINT TrHierarchicalDepth::CalculateMipCount(UINT width, UINT height)
{
    UINT mipCount = 1;
    while(width > 1 || height > 1)
    {
        width = std::max(1u, width >> 1);
        height = std::max(1u, height >> 1);
        ++mipCount;
    }
    return mipCount;
}

void TrHierarchicalDepth::CreateResource(
    ID3D12Device* device,
    UINT width,
    UINT height)
{
    if(device == nullptr || width == 0 || height == 0 ||
       width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
       height > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION)
    {
        throw std::invalid_argument("HZB dimensions are invalid.");
    }

    const UINT mipCount = CalculateMipCount(width, height);
    if(mipCount > MaximumMipCount)
    {
        throw std::out_of_range("HZB mip count exceeds the reserved views.");
    }

    mDescription.Width = width;
    mDescription.Height = height;
    mDescription.MipCount = mipCount;
    mDescription.Format = DXGI_FORMAT_R32_FLOAT;
    mTexture.Initialize2D(
        device,
        width,
        height,
        mDescription.Format,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
        nullptr,
        L"Hierarchical Z Buffer",
        static_cast<UINT16>(mipCount));
    mTexture.CreateShaderResourceView(
        device,
        mSrv.CpuHandle,
        mDescription.Format,
        0,
        mipCount);
    for(UINT mip = 0; mip < mipCount; ++mip)
    {
        mTexture.CreateShaderResourceView(
            device,
            mMipSrvs[mip].CpuHandle,
            mDescription.Format,
            mip,
            1);
        mTexture.CreateUnorderedAccessView(
            device,
            mMipUavs[mip].CpuHandle,
            mDescription.Format,
            mip);
    }
}
