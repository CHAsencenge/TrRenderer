#include "TrTexture.h"

#include <algorithm>
#include <stdexcept>

void TrTexture::Initialize2D(
    ID3D12Device* device,
    UINT64 width,
    UINT height,
    DXGI_FORMAT format,
    D3D12_RESOURCE_FLAGS flags,
    D3D12_RESOURCE_STATES initialState,
    const D3D12_CLEAR_VALUE* clearValue,
    const wchar_t* debugName,
    UINT16 mipLevels)
{
    if(device == nullptr || width == 0 || height == 0 ||
       format == DXGI_FORMAT_UNKNOWN || mipLevels == 0)
    {
        throw std::invalid_argument("Texture2D description is incomplete.");
    }

    Reset();
    mDescription = CD3DX12_RESOURCE_DESC::Tex2D(
        format,
        width,
        height,
        1,
        mipLevels,
        1,
        0,
        flags);

    const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &mDescription,
        initialState,
        clearValue,
        IID_PPV_ARGS(&mResource)));

    mMipStates.assign(mDescription.MipLevels, initialState);
    if(debugName != nullptr && debugName[0] != L'\0')
    {
        ThrowIfFailed(mResource->SetName(debugName));
    }
}

void TrTexture::Attach(
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES initialState,
    const wchar_t* debugName)
{
    if(resource == nullptr)
    {
        throw std::invalid_argument("Cannot attach a null texture resource.");
    }

    Reset();
    mResource = resource;
    mDescription = resource->GetDesc();
    mMipStates.assign(mDescription.MipLevels, initialState);
    if(debugName != nullptr && debugName[0] != L'\0')
    {
        ThrowIfFailed(mResource->SetName(debugName));
    }
}

void TrTexture::Reset()
{
    mResource.Reset();
    mDescription = {};
    mMipStates.clear();
}

void TrTexture::CreateRenderTargetView(
    ID3D12Device* device,
    D3D12_CPU_DESCRIPTOR_HANDLE handle,
    DXGI_FORMAT viewFormat,
    UINT mipSlice) const
{
    ValidateResource();
    ValidateMipSlice(mipSlice);
    if(device == nullptr || !(mDescription.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET))
    {
        throw std::invalid_argument("Texture is not a render target.");
    }

    D3D12_RENDER_TARGET_VIEW_DESC viewDesc = {};
    viewDesc.Format = ResolveViewFormat(viewFormat);
    viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    viewDesc.Texture2D.MipSlice = mipSlice;
    viewDesc.Texture2D.PlaneSlice = 0;
    device->CreateRenderTargetView(mResource.Get(), &viewDesc, handle);
}

void TrTexture::CreateDepthStencilView(
    ID3D12Device* device,
    D3D12_CPU_DESCRIPTOR_HANDLE handle,
    DXGI_FORMAT viewFormat,
    UINT mipSlice,
    D3D12_DSV_FLAGS flags) const
{
    ValidateResource();
    ValidateMipSlice(mipSlice);
    if(device == nullptr || !(mDescription.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL))
    {
        throw std::invalid_argument("Texture is not a depth-stencil target.");
    }

    D3D12_DEPTH_STENCIL_VIEW_DESC viewDesc = {};
    viewDesc.Format = ResolveViewFormat(viewFormat);
    viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    viewDesc.Flags = flags;
    viewDesc.Texture2D.MipSlice = mipSlice;
    device->CreateDepthStencilView(mResource.Get(), &viewDesc, handle);
}

void TrTexture::CreateShaderResourceView(
    ID3D12Device* device,
    D3D12_CPU_DESCRIPTOR_HANDLE handle,
    DXGI_FORMAT viewFormat,
    UINT mostDetailedMip,
    UINT mipCount,
    UINT planeSlice) const
{
    ValidateResource();
    const UINT resolvedMipCount = ResolveMipCount(mostDetailedMip, mipCount);
    if(device == nullptr || (mDescription.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE))
    {
        throw std::invalid_argument("Texture cannot be exposed as an SRV.");
    }
    const UINT planeCount = D3D12GetFormatPlaneCount(device, mDescription.Format);
    if(planeSlice >= planeCount)
    {
        throw std::out_of_range("Texture SRV plane slice exceeds the resource.");
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
    viewDesc.Format = ResolveViewFormat(viewFormat);
    viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    viewDesc.Texture2D.MostDetailedMip = mostDetailedMip;
    viewDesc.Texture2D.MipLevels = resolvedMipCount;
    viewDesc.Texture2D.PlaneSlice = planeSlice;
    viewDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    device->CreateShaderResourceView(mResource.Get(), &viewDesc, handle);
}

void TrTexture::CreateUnorderedAccessView(
    ID3D12Device* device,
    D3D12_CPU_DESCRIPTOR_HANDLE handle,
    DXGI_FORMAT viewFormat,
    UINT mipSlice) const
{
    ValidateResource();
    ValidateMipSlice(mipSlice);
    if(device == nullptr || !(mDescription.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
    {
        throw std::invalid_argument("Texture is not an unordered-access resource.");
    }

    D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc = {};
    viewDesc.Format = ResolveViewFormat(viewFormat);
    viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    viewDesc.Texture2D.MipSlice = mipSlice;
    viewDesc.Texture2D.PlaneSlice = 0;
    device->CreateUnorderedAccessView(mResource.Get(), nullptr, &viewDesc, handle);
}

bool TrTexture::Transition(
    ID3D12GraphicsCommandList* commandList,
    D3D12_RESOURCE_STATES newState,
    UINT mipSlice)
{
    ValidateResource();
    if(mipSlice != D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
    {
        ValidateMipSlice(mipSlice);
        const bool recorded = TrResourceBarrier::Transition(
            commandList,
            mResource.Get(),
            mMipStates[mipSlice],
            newState,
            mipSlice);
        mMipStates[mipSlice] = newState;
        return recorded;
    }

    const D3D12_RESOURCE_STATES firstState = mMipStates.front();
    bool hasUniformState = true;
    for(const D3D12_RESOURCE_STATES state : mMipStates)
    {
        if(state != firstState)
        {
            hasUniformState = false;
            break;
        }
    }

    bool recorded = false;
    if(hasUniformState)
    {
        recorded = TrResourceBarrier::Transition(
            commandList,
            mResource.Get(),
            firstState,
            newState);
    }
    else
    {
        for(UINT mip = 0; mip < mMipStates.size(); ++mip)
        {
            recorded |= TrResourceBarrier::Transition(
                commandList,
                mResource.Get(),
                mMipStates[mip],
                newState,
                mip);
        }
    }

    std::fill(mMipStates.begin(), mMipStates.end(), newState);
    return recorded;
}

void TrTexture::UavBarrier(ID3D12GraphicsCommandList* commandList) const
{
    ValidateResource();
    TrResourceBarrier::Uav(commandList, mResource.Get());
}

DXGI_FORMAT TrTexture::ResolveViewFormat(DXGI_FORMAT viewFormat) const
{
    return viewFormat == DXGI_FORMAT_UNKNOWN ? mDescription.Format : viewFormat;
}

UINT TrTexture::ResolveMipCount(UINT mostDetailedMip, UINT mipCount) const
{
    ValidateMipSlice(mostDetailedMip);
    const UINT availableMipCount = mDescription.MipLevels - mostDetailedMip;
    const UINT resolvedMipCount = mipCount == 0 ? availableMipCount : mipCount;
    if(resolvedMipCount == 0 || resolvedMipCount > availableMipCount)
    {
        throw std::out_of_range("Texture SRV mip range exceeds the resource.");
    }
    return resolvedMipCount;
}

void TrTexture::ValidateMipSlice(UINT mipSlice) const
{
    ValidateResource();
    if(mipSlice >= mDescription.MipLevels)
    {
        throw std::out_of_range("Texture mip slice exceeds the resource.");
    }
}

D3D12_RESOURCE_STATES TrTexture::GetState() const
{
    ValidateResource();
    const D3D12_RESOURCE_STATES firstState = mMipStates.front();
    for(const D3D12_RESOURCE_STATES state : mMipStates)
    {
        if(state != firstState)
        {
            throw std::logic_error("Texture mip levels are in different resource states.");
        }
    }
    return firstState;
}

D3D12_RESOURCE_STATES TrTexture::GetState(UINT mipSlice) const
{
    ValidateMipSlice(mipSlice);
    return mMipStates[mipSlice];
}

void TrTexture::ValidateResource() const
{
    if(mResource == nullptr ||
       mDescription.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
       mDescription.DepthOrArraySize != 1 ||
       mMipStates.size() != mDescription.MipLevels)
    {
        throw std::logic_error("Non-array Texture2D has not been initialized.");
    }
}
