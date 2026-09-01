#pragma once

#include "TrBuffer.h"

class TrConstantBuffer
{
public:
    TrConstantBuffer() = default;
    ~TrConstantBuffer();

    TrConstantBuffer(const TrConstantBuffer&) = delete;
    TrConstantBuffer& operator=(const TrConstantBuffer&) = delete;

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
    TrBuffer mBuffer;
    UINT mDataSize = 0;
    UINT mAllocatedSize = 0;
};
