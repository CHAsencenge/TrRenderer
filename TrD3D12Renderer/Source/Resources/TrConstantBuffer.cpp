#include "TrConstantBuffer.h"

#include <stdexcept>

namespace
{
    UINT AlignConstantBufferSize(UINT size)
    {
        const UINT alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
        return (size + alignment - 1) & ~(alignment - 1);
    }
}

TrConstantBuffer::~TrConstantBuffer()
{
    Reset();
}

void TrConstantBuffer::Initialize(ID3D12Device* device, UINT dataSize)
{
    if(device == nullptr || dataSize == 0)
    {
        throw std::invalid_argument("Constant buffer requires a device and non-zero data size.");
    }

    Reset();
    mDataSize = dataSize;
    mAllocatedSize = AlignConstantBufferSize(dataSize);

    TrBufferDesc bufferDesc;
    bufferDesc.SizeInBytes = mAllocatedSize;
    bufferDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
    bufferDesc.InitialState = D3D12_RESOURCE_STATE_GENERIC_READ;
    mBuffer.Initialize(device, bufferDesc);
}

void TrConstantBuffer::Update(const void* data, UINT dataSize)
{
    if(mBuffer.Get() == nullptr || data == nullptr || dataSize > mDataSize)
    {
        throw std::invalid_argument("Invalid constant buffer update.");
    }

    mBuffer.Update(data, dataSize);
}

void TrConstantBuffer::Reset()
{
    mBuffer.Reset();
    mDataSize = 0;
    mAllocatedSize = 0;
}

D3D12_GPU_VIRTUAL_ADDRESS TrConstantBuffer::GetGpuVirtualAddress() const
{
    return mBuffer.Get() != nullptr ? mBuffer.GetGpuVirtualAddress() : 0;
}
