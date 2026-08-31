#pragma once

#include "TrUtil.h"

class TrUploadContext
{
public:
    TrUploadContext() = default;
    ~TrUploadContext();

    TrUploadContext(const TrUploadContext&) = delete;
    TrUploadContext& operator=(const TrUploadContext&) = delete;

    void Initialize(ID3D12Device* device);
    void UploadStaticBuffer(
        const void* sourceData,
        UINT64 byteSize,
        D3D12_RESOURCE_STATES finalState,
        Microsoft::WRL::ComPtr<ID3D12Resource>& destination,
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
    void ExecuteAndWait(ID3D12CommandQueue* commandQueue);

private:
    Microsoft::WRL::ComPtr<ID3D12Device> mDevice;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mCommandAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;
    Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> mIntermediateResources;
    HANDLE mFenceEvent = nullptr;
    UINT64 mFenceValue = 1;
    bool mRecording = false;
};
