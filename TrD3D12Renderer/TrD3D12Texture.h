#pragma once

#include "TrD3D12ResourceBarrier.h"

class TrD3D12Texture
{
public:
    TrD3D12Texture() = default;
    TrD3D12Texture(const TrD3D12Texture&) = delete;
    TrD3D12Texture& operator=(const TrD3D12Texture&) = delete;

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
        DXGI_FORMAT viewFormat = DXGI_FORMAT_UNKNOWN) const;
    void CreateDepthStencilView(
        ID3D12Device* device,
        D3D12_CPU_DESCRIPTOR_HANDLE handle,
        DXGI_FORMAT viewFormat = DXGI_FORMAT_UNKNOWN) const;
    void CreateShaderResourceView(
        ID3D12Device* device,
        D3D12_CPU_DESCRIPTOR_HANDLE handle,
        DXGI_FORMAT viewFormat = DXGI_FORMAT_UNKNOWN) const;
    void CreateUnorderedAccessView(
        ID3D12Device* device,
        D3D12_CPU_DESCRIPTOR_HANDLE handle,
        DXGI_FORMAT viewFormat = DXGI_FORMAT_UNKNOWN) const;

    bool Transition(
        ID3D12GraphicsCommandList* commandList,
        D3D12_RESOURCE_STATES newState);
    void UavBarrier(ID3D12GraphicsCommandList* commandList) const;

    ID3D12Resource* Get() const { return mResource.Get(); }
    D3D12_RESOURCE_STATES GetState() const { return mState; }
    const D3D12_RESOURCE_DESC& GetDescription() const { return mDescription; }

private:
    DXGI_FORMAT ResolveViewFormat(DXGI_FORMAT viewFormat) const;
    void ValidateResource() const;

    Microsoft::WRL::ComPtr<ID3D12Resource> mResource;
    D3D12_RESOURCE_DESC mDescription = {};
    D3D12_RESOURCE_STATES mState = D3D12_RESOURCE_STATE_COMMON;
};
