#include "TrUploadContext.h"
#include "TrResourceBarrier.h"

#include <stdexcept>

TrUploadContext::~TrUploadContext()
{
    if(mFenceEvent != nullptr)
    {
        CloseHandle(mFenceEvent);
        mFenceEvent = nullptr;
    }
}

void TrUploadContext::Initialize(ID3D12Device* device)
{
    if(device == nullptr)
    {
        throw std::invalid_argument("Upload context requires a device.");
    }

    mDevice = device;
    ThrowIfFailed(mDevice->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&mCommandAllocator)));
    ThrowIfFailed(mDevice->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        mCommandAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(&mCommandList)));
    ThrowIfFailed(mDevice->CreateFence(
        0,
        D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&mFence)));

    mFenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if(mFenceEvent == nullptr)
    {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }

    mRecording = true;
}

void TrUploadContext::UploadStaticBuffer(
    const void* sourceData,
    UINT64 byteSize,
    D3D12_RESOURCE_STATES finalState,
    Microsoft::WRL::ComPtr<ID3D12Resource>& destination,
    D3D12_RESOURCE_FLAGS flags)
{
    if(!mRecording || sourceData == nullptr || byteSize == 0)
    {
        throw std::invalid_argument("Invalid static buffer upload.");
    }

    const CD3DX12_RESOURCE_DESC destinationDesc =
        CD3DX12_RESOURCE_DESC::Buffer(byteSize, flags);
    CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(mDevice->CreateCommittedResource(
        &defaultHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &destinationDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&destination)));

    Microsoft::WRL::ComPtr<ID3D12Resource> intermediate;
    const CD3DX12_RESOURCE_DESC uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
    CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
    ThrowIfFailed(mDevice->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&intermediate)));

    D3D12_SUBRESOURCE_DATA subresourceData = {};
    subresourceData.pData = sourceData;
    subresourceData.RowPitch = static_cast<LONG_PTR>(byteSize);
    subresourceData.SlicePitch = static_cast<LONG_PTR>(byteSize);
    UpdateSubresources(
        mCommandList.Get(),
        destination.Get(),
        intermediate.Get(),
        0,
        0,
        1,
        &subresourceData);

    TrResourceBarrier::Transition(
        mCommandList.Get(),
        destination.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        finalState);
    mIntermediateResources.push_back(intermediate);
}

void TrUploadContext::ExecuteAndWait(ID3D12CommandQueue* commandQueue)
{
    if(!mRecording || commandQueue == nullptr)
    {
        throw std::invalid_argument("Upload context is not ready for submission.");
    }

    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* commandLists[] = {mCommandList.Get()};
    commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

    const UINT64 submittedFenceValue = mFenceValue++;
    ThrowIfFailed(commandQueue->Signal(mFence.Get(), submittedFenceValue));
    if(mFence->GetCompletedValue() < submittedFenceValue)
    {
        ThrowIfFailed(mFence->SetEventOnCompletion(submittedFenceValue, mFenceEvent));
        const DWORD waitResult = WaitForSingleObject(mFenceEvent, INFINITE);
        if(waitResult != WAIT_OBJECT_0)
        {
            const DWORD error = waitResult == WAIT_FAILED ? GetLastError() : ERROR_GEN_FAILURE;
            ThrowIfFailed(HRESULT_FROM_WIN32(error));
        }
    }

    mIntermediateResources.clear();
    mRecording = false;
}
