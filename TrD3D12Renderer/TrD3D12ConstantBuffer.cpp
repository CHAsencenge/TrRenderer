#include "TrD3D12ConstantBuffer.h"

#include <cstring>
#include <stdexcept>

namespace
{
    UINT AlignConstantBufferSize(UINT size)
    {
        const UINT alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
        return (size + alignment - 1) & ~(alignment - 1);
    }
}

TrD3D12ConstantBuffer::~TrD3D12ConstantBuffer()
{
    Reset();
}

void TrD3D12ConstantBuffer::Initialize(ID3D12Device* device, UINT dataSize)
{
    if(device == nullptr || dataSize == 0)
    {
        throw std::invalid_argument("Constant buffer requires a device and non-zero data size.");
    }

    Reset();
    mDataSize = dataSize;
    mAllocatedSize = AlignConstantBufferSize(dataSize);

    CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(mAllocatedSize);
    ThrowIfFailed(device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&mResource)));

    const CD3DX12_RANGE noCpuReads(0, 0);
    ThrowIfFailed(mResource->Map(
        0,
        &noCpuReads,
        reinterpret_cast<void**>(&mMappedData)));
    std::memset(mMappedData, 0, mAllocatedSize);
}

void TrD3D12ConstantBuffer::Update(const void* data, UINT dataSize)
{
    if(mMappedData == nullptr || data == nullptr || dataSize > mDataSize)
    {
        throw std::invalid_argument("Invalid constant buffer update.");
    }

    std::memcpy(mMappedData, data, dataSize);
}

void TrD3D12ConstantBuffer::Reset()
{
    if(mResource != nullptr && mMappedData != nullptr)
    {
        mResource->Unmap(0, nullptr);
    }

    mMappedData = nullptr;
    mResource.Reset();
    mDataSize = 0;
    mAllocatedSize = 0;
}

D3D12_GPU_VIRTUAL_ADDRESS TrD3D12ConstantBuffer::GetGpuVirtualAddress() const
{
    return mResource != nullptr ? mResource->GetGPUVirtualAddress() : 0;
}
