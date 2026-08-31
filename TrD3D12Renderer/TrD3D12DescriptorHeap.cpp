#include "TrD3D12DescriptorHeap.h"

#include <stdexcept>

void TrD3D12DescriptorHeap::Initialize(
    ID3D12Device* device,
    D3D12_DESCRIPTOR_HEAP_TYPE type,
    UINT capacity,
    bool shaderVisible,
    const wchar_t* debugName)
{
    if(device == nullptr || capacity == 0)
    {
        throw std::invalid_argument("Descriptor heap requires a device and non-zero capacity.");
    }

    if(shaderVisible && type != D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV &&
       type != D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
    {
        throw std::invalid_argument("RTV and DSV descriptor heaps cannot be shader visible.");
    }

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = type;
    heapDesc.NumDescriptors = capacity;
    heapDesc.Flags = shaderVisible
        ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
        : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mHeap)));

    if(debugName != nullptr && debugName[0] != L'\0')
    {
        ThrowIfFailed(mHeap->SetName(debugName));
    }

    mType = type;
    mDescriptorSize = device->GetDescriptorHandleIncrementSize(type);
    mCapacity = capacity;
    mNextIndex = 0;
    mShaderVisible = shaderVisible;
}

TrD3D12DescriptorAllocation TrD3D12DescriptorHeap::Allocate()
{
    if(mHeap == nullptr || mNextIndex >= mCapacity)
    {
        throw std::runtime_error("Descriptor heap capacity exhausted.");
    }

    TrD3D12DescriptorAllocation allocation;
    allocation.Index = mNextIndex;
    allocation.CpuHandle = GetCpuHandle(mNextIndex);
    if(mShaderVisible)
    {
        allocation.GpuHandle = GetGpuHandle(mNextIndex);
    }

    ++mNextIndex;
    return allocation;
}

D3D12_CPU_DESCRIPTOR_HANDLE TrD3D12DescriptorHeap::GetCpuHandle(UINT index) const
{
    if(mHeap == nullptr || index >= mCapacity)
    {
        throw std::out_of_range("Descriptor index is outside the heap.");
    }

    return CD3DX12_CPU_DESCRIPTOR_HANDLE(
        mHeap->GetCPUDescriptorHandleForHeapStart(),
        static_cast<INT>(index),
        mDescriptorSize);
}

D3D12_GPU_DESCRIPTOR_HANDLE TrD3D12DescriptorHeap::GetGpuHandle(UINT index) const
{
    if(!mShaderVisible)
    {
        throw std::logic_error("CPU-only descriptor heap has no GPU handles.");
    }
    if(mHeap == nullptr || index >= mCapacity)
    {
        throw std::out_of_range("Descriptor index is outside the heap.");
    }

    return CD3DX12_GPU_DESCRIPTOR_HANDLE(
        mHeap->GetGPUDescriptorHandleForHeapStart(),
        static_cast<INT>(index),
        mDescriptorSize);
}
