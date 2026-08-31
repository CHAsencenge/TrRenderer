#pragma once

#include "TrUtil.h"

class TrUploadContext;

enum class TrBufferViewType
{
    Structured,
    Raw,
    Typed
};

struct TrBufferDesc
{
    UINT64 SizeInBytes = 0;
    D3D12_HEAP_TYPE HeapType = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_FLAGS Flags = D3D12_RESOURCE_FLAG_NONE;
    D3D12_RESOURCE_STATES InitialState = D3D12_RESOURCE_STATE_COMMON;
    const wchar_t* DebugName = nullptr;
};

struct TrBufferViewDesc
{
    TrBufferViewType Type = TrBufferViewType::Structured;
    UINT64 FirstElement = 0;
    UINT ElementCount = 0;
    UINT ElementStride = 0;
    DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
    UINT64 CounterOffsetInBytes = 0;
};

class TrBuffer
{
public:
    TrBuffer() = default;
    ~TrBuffer();

    TrBuffer(const TrBuffer&) = delete;
    TrBuffer& operator=(const TrBuffer&) = delete;

    void Initialize(ID3D12Device* device, const TrBufferDesc& desc);
    void InitializeStatic(
        TrUploadContext& uploadContext,
        const void* sourceData,
        UINT64 byteSize,
        D3D12_RESOURCE_STATES finalState,
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
        const wchar_t* debugName = nullptr);
    void Reset();

    void Update(const void* data, UINT64 byteSize, UINT64 destinationOffset = 0);
    void* Map(const D3D12_RANGE* readRange = nullptr);
    void Unmap(const D3D12_RANGE* writtenRange = nullptr);

    D3D12_VERTEX_BUFFER_VIEW CreateVertexBufferView(
        UINT strideInBytes,
        UINT64 byteOffset = 0,
        UINT64 byteSize = 0) const;
    D3D12_INDEX_BUFFER_VIEW CreateIndexBufferView(
        DXGI_FORMAT format,
        UINT64 byteOffset = 0,
        UINT64 byteSize = 0) const;
    void CreateConstantBufferView(
        ID3D12Device* device,
        D3D12_CPU_DESCRIPTOR_HANDLE handle,
        UINT64 byteOffset = 0,
        UINT64 byteSize = 0) const;
    void CreateShaderResourceView(
        ID3D12Device* device,
        D3D12_CPU_DESCRIPTOR_HANDLE handle,
        const TrBufferViewDesc& viewDesc) const;
    void CreateUnorderedAccessView(
        ID3D12Device* device,
        D3D12_CPU_DESCRIPTOR_HANDLE handle,
        const TrBufferViewDesc& viewDesc,
        ID3D12Resource* counterResource = nullptr) const;

    bool Transition(
        ID3D12GraphicsCommandList* commandList,
        D3D12_RESOURCE_STATES newState);
    void UavBarrier(ID3D12GraphicsCommandList* commandList) const;

    ID3D12Resource* Get() const { return mResource.Get(); }
    D3D12_GPU_VIRTUAL_ADDRESS GetGpuVirtualAddress(UINT64 byteOffset = 0) const;
    UINT64 GetSizeInBytes() const { return mSizeInBytes; }
    D3D12_HEAP_TYPE GetHeapType() const { return mHeapType; }
    D3D12_RESOURCE_FLAGS GetFlags() const { return mFlags; }
    D3D12_RESOURCE_STATES GetState() const { return mState; }

private:
    void ValidateResource() const;

    Microsoft::WRL::ComPtr<ID3D12Resource> mResource;
    UINT8* mMappedData = nullptr;
    UINT64 mSizeInBytes = 0;
    D3D12_HEAP_TYPE mHeapType = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_FLAGS mFlags = D3D12_RESOURCE_FLAG_NONE;
    D3D12_RESOURCE_STATES mState = D3D12_RESOURCE_STATE_COMMON;
};
