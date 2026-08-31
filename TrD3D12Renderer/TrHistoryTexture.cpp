#include "TrHistoryTexture.h"

#include <stdexcept>

void TrHistoryTexture::Initialize(
    ID3D12Device* device,
    UINT width,
    UINT height,
    DXGI_FORMAT format,
    TrDescriptorHeap& resourceHeap,
    const wchar_t* debugName)
{
    if(mInitialized)
    {
        throw std::logic_error("History texture has already been initialized; use Resize.");
    }
    if(device == nullptr || width == 0 || height == 0 ||
       format == DXGI_FORMAT_UNKNOWN)
    {
        throw std::invalid_argument("History texture description is incomplete.");
    }
    if(resourceHeap.GetType() != D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ||
       !resourceHeap.IsShaderVisible())
    {
        throw std::invalid_argument(
            "History texture requires a shader-visible CBV/SRV/UAV heap.");
    }

    for(TrDescriptorAllocation& srv : mSrvs)
    {
        srv = resourceHeap.Allocate();
    }
    for(TrDescriptorAllocation& uav : mUavs)
    {
        uav = resourceHeap.Allocate();
    }

    mFormat = format;
    mDebugName = debugName != nullptr ? debugName : L"History Texture";
    mInitialized = true;
    CreateResources(device, width, height);
}

void TrHistoryTexture::Resize(ID3D12Device* device, UINT width, UINT height)
{
    ValidateInitialized();
    if(device == nullptr || width == 0 || height == 0)
    {
        throw std::invalid_argument("History texture resize dimensions are invalid.");
    }
    CreateResources(device, width, height);
}

void TrHistoryTexture::AdvanceFrame()
{
    ValidateInitialized();
    mCurrentIndex = GetPreviousIndex();
    mValid = true;
}

void TrHistoryTexture::Invalidate()
{
    mCurrentIndex = 0;
    mValid = false;
}

TrTexture& TrHistoryTexture::GetCurrent()
{
    ValidateInitialized();
    return mTextures[mCurrentIndex];
}

const TrTexture& TrHistoryTexture::GetCurrent() const
{
    ValidateInitialized();
    return mTextures[mCurrentIndex];
}

TrTexture& TrHistoryTexture::GetPrevious()
{
    ValidateInitialized();
    return mTextures[GetPreviousIndex()];
}

const TrTexture& TrHistoryTexture::GetPrevious() const
{
    ValidateInitialized();
    return mTextures[GetPreviousIndex()];
}

const TrDescriptorAllocation& TrHistoryTexture::GetCurrentSrv() const
{
    ValidateInitialized();
    return mSrvs[mCurrentIndex];
}

const TrDescriptorAllocation& TrHistoryTexture::GetPreviousSrv() const
{
    ValidateInitialized();
    return mSrvs[GetPreviousIndex()];
}

const TrDescriptorAllocation& TrHistoryTexture::GetCurrentUav() const
{
    ValidateInitialized();
    return mUavs[mCurrentIndex];
}

const TrDescriptorAllocation& TrHistoryTexture::GetPreviousUav() const
{
    ValidateInitialized();
    return mUavs[GetPreviousIndex()];
}

void TrHistoryTexture::CreateResources(
    ID3D12Device* device,
    UINT width,
    UINT height)
{
    for(UINT index = 0; index < _countof(mTextures); ++index)
    {
        const std::wstring resourceName =
            mDebugName + L" " + std::to_wstring(index);
        mTextures[index].Initialize2D(
            device,
            width,
            height,
            mFormat,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            nullptr,
            resourceName.c_str());
        mTextures[index].CreateShaderResourceView(
            device,
            mSrvs[index].CpuHandle);
        mTextures[index].CreateUnorderedAccessView(
            device,
            mUavs[index].CpuHandle);
    }
    Invalidate();
}

void TrHistoryTexture::ValidateInitialized() const
{
    if(!mInitialized)
    {
        throw std::logic_error("History texture has not been initialized.");
    }
}
