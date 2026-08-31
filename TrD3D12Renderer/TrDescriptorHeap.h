#pragma once

#include "TrUtil.h"

struct TrDescriptorAllocation
{
    UINT Index = UINT_MAX;
    D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle = {};
    D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle = {};
};

class TrDescriptorHeap
{
public:
    void Initialize(
        ID3D12Device* device,
        D3D12_DESCRIPTOR_HEAP_TYPE type,
        UINT capacity,
        bool shaderVisible,
        const wchar_t* debugName = nullptr);

    TrDescriptorAllocation Allocate();

    ID3D12DescriptorHeap* Get() const { return mHeap.Get(); }
    UINT GetCapacity() const { return mCapacity; }
    UINT GetAllocatedCount() const { return mNextIndex; }
    bool IsShaderVisible() const { return mShaderVisible; }

    D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(UINT index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(UINT index) const;

private:
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mHeap;
    D3D12_DESCRIPTOR_HEAP_TYPE mType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    UINT mDescriptorSize = 0;
    UINT mCapacity = 0;
    UINT mNextIndex = 0;
    bool mShaderVisible = false;
};
