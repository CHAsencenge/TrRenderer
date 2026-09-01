#pragma once

#include "Backend/TrResourceBarrier.h"

#include <vector>

class TrTexture
{
public:
    TrTexture() = default;
    TrTexture(const TrTexture&) = delete;
    TrTexture& operator=(const TrTexture&) = delete;

    void Initialize2D(
        ID3D12Device* device,
        UINT64 width,
        UINT height,
        DXGI_FORMAT format,
        D3D12_RESOURCE_FLAGS flags,
        D3D12_RESOURCE_STATES initialState,
        const D3D12_CLEAR_VALUE* clearValue = nullptr,
        const wchar_t* debugName = nullptr,
        UINT16 mipLevels = 1);

    void Attach(
        ID3D12Resource* resource,
        D3D12_RESOURCE_STATES initialState,
        const wchar_t* debugName = nullptr);
    void Reset();

    void CreateRenderTargetView(
        ID3D12Device* device,
        D3D12_CPU_DESCRIPTOR_HANDLE handle,
        DXGI_FORMAT viewFormat = DXGI_FORMAT_UNKNOWN,
        UINT mipSlice = 0) const;
    void CreateDepthStencilView(
        ID3D12Device* device,
        D3D12_CPU_DESCRIPTOR_HANDLE handle,
        DXGI_FORMAT viewFormat = DXGI_FORMAT_UNKNOWN,
        UINT mipSlice = 0,
        D3D12_DSV_FLAGS flags = D3D12_DSV_FLAG_NONE) const;

    // A mipCount of zero exposes every mip from mostDetailedMip onward.
    void CreateShaderResourceView(
        ID3D12Device* device,
        D3D12_CPU_DESCRIPTOR_HANDLE handle,
        DXGI_FORMAT viewFormat = DXGI_FORMAT_UNKNOWN,
        UINT mostDetailedMip = 0,
        UINT mipCount = 0,
        UINT planeSlice = 0) const;
    void CreateUnorderedAccessView(
        ID3D12Device* device,
        D3D12_CPU_DESCRIPTOR_HANDLE handle,
        DXGI_FORMAT viewFormat = DXGI_FORMAT_UNKNOWN,
        UINT mipSlice = 0) const;

    // ALL_SUBRESOURCES transitions every mip, including mixed-state mip chains.
    bool Transition(
        ID3D12GraphicsCommandList* commandList,
        D3D12_RESOURCE_STATES newState,
        UINT mipSlice = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
    void UavBarrier(ID3D12GraphicsCommandList* commandList) const;

    ID3D12Resource* Get() const { return mResource.Get(); }
    D3D12_RESOURCE_STATES GetState() const;
    D3D12_RESOURCE_STATES GetState(UINT mipSlice) const;
    UINT GetMipCount() const { return mDescription.MipLevels; }
    const D3D12_RESOURCE_DESC& GetDescription() const { return mDescription; }

private:
    DXGI_FORMAT ResolveViewFormat(DXGI_FORMAT viewFormat) const;
    UINT ResolveMipCount(UINT mostDetailedMip, UINT mipCount) const;
    void ValidateMipSlice(UINT mipSlice) const;
    void ValidateResource() const;

    Microsoft::WRL::ComPtr<ID3D12Resource> mResource;
    D3D12_RESOURCE_DESC mDescription = {};
    std::vector<D3D12_RESOURCE_STATES> mMipStates;
};
