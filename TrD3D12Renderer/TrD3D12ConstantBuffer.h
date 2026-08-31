#pragma once

#include "TrD3D12Util.h"

class TrD3D12ConstantBuffer
{
public:
    TrD3D12ConstantBuffer() = default;
    ~TrD3D12ConstantBuffer();

    TrD3D12ConstantBuffer(const TrD3D12ConstantBuffer&) = delete;
    TrD3D12ConstantBuffer& operator=(const TrD3D12ConstantBuffer&) = delete;

    void Initialize(ID3D12Device* device, UINT dataSize);
    void Update(const void* data, UINT dataSize);
    void Reset();

    template<typename T>
    void Update(const T& value)
    {
        Update(&value, static_cast<UINT>(sizeof(T)));
    }

    D3D12_GPU_VIRTUAL_ADDRESS GetGpuVirtualAddress() const;
    UINT GetDataSize() const { return mDataSize; }
    UINT GetAllocatedSize() const { return mAllocatedSize; }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> mResource;
    UINT8* mMappedData = nullptr;
    UINT mDataSize = 0;
    UINT mAllocatedSize = 0;
};
