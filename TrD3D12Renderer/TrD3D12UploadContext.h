#pragma once

#include "TrD3D12Util.h"

class TrD3D12UploadContext
{
public:
    TrD3D12UploadContext() = default;
    ~TrD3D12UploadContext();

    TrD3D12UploadContext(const TrD3D12UploadContext&) = delete;
    TrD3D12UploadContext& operator=(const TrD3D12UploadContext&) = delete;

    void Initialize(ID3D12Device* device);
    void UploadStaticBuffer(
        const void* sourceData,
        UINT64 byteSize,
        D3D12_RESOURCE_STATES finalState,
        Microsoft::WRL::ComPtr<ID3D12Resource>& destination);
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
