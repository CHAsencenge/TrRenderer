
#include "TrD3D12RendererRaster.h"
#include "CornellBoxScene.h"
#include "TrD3D12UploadContext.h"
#include "TrWindowApp.h"


TrD3D12RendererRaster::TrD3D12RendererRaster(UINT width, UINT height, std::wstring title) :
    TrD3D12RendererBase(width, height, title),
    mFrameIndex(0),
    mViewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
    mScissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height))
{
}

void TrD3D12RendererRaster::OnInitialize()
{
    LoadPipeline();
    LoadAssetsCornellBox();
}

void TrD3D12RendererRaster::OnUpdate()
{
    using namespace DirectX;

    const XMMATRIX model = XMMatrixIdentity();
    const XMMATRIX view = XMMatrixLookAtLH(
        XMVectorSet(0.0f, 1.0f, -3.2f, 1.0f),
        XMVectorSet(0.0f, 0.95f, 1.0f, 1.0f),
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    const XMMATRIX projection = XMMatrixPerspectiveFovLH(
        XM_PIDIV4,
        mAspectRatio,
        0.1f,
        100.0f);

    CornellBoxConstants constants = {};
    XMStoreFloat4x4(&constants.ModelViewProjection, XMMatrixTranspose(model * view * projection));
    XMStoreFloat3(
        &constants.LightDirection,
        XMVector3Normalize(XMVectorSet(-0.25f, 1.0f, -0.35f, 0.0f)));
    constants.AmbientStrength = 0.22f;
    mFrameContexts[mFrameIndex].ConstantBuffer.Update(constants);
}

void TrD3D12RendererRaster::OnRender()
{
    PopulateCommandList();
    ID3D12CommandList* ppCommandLists[] = {mCommandList.Get()};
    mCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    ThrowIfFailed(mSwapChain->Present(1, 0));

    MoveToNextFrame();
}

void TrD3D12RendererRaster::OnDestroy()
{
    FlushCommandQueue();
    if(mFenceEvent != nullptr)
    {
        CloseHandle(mFenceEvent);
        mFenceEvent = nullptr;
    }
    ValidateDebugLayer();
}

void TrD3D12RendererRaster::OnKeyDown(UINT8 wParam)
{
}

void TrD3D12RendererRaster::OnKeyUp(UINT8 wParam)
{
}

void TrD3D12RendererRaster::LoadPipeline()
{
    // enable debug layer
    Microsoft::WRL::ComPtr<ID3D12Debug> debugger;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugger))))
    {
        debugger->EnableDebugLayer();
    }

    // create device
    // todo: factory version
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory; 
    ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
    if (mbUseWarpDevice)
    {
        Microsoft::WRL::ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));
        ThrowIfFailed(D3D12CreateDevice(
                warpAdapter.Get(),
                D3D_FEATURE_LEVEL_11_0,
                IID_PPV_ARGS(&mDevice)
            ));
    }
    else
    {
        Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
        GetHardwareAdapter(factory.Get(), IID_PPV_ARGS(&adapter));

        ThrowIfFailed(D3D12CreateDevice(
                adapter.Get(),
                D3D_FEATURE_LEVEL_11_0,
                IID_PPV_ARGS(&mDevice)
            ));
    }

    // describe and create command queue
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};  // aggregate initialization
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(mDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));

    // describe and create swap chain
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = SwapFrameCount;
    swapChainDesc.Width = mWidth;
    swapChainDesc.Height = mHeight;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // UNORM means unsigned normalized
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    // todo: MultiSample
    swapChainDesc.SampleDesc.Count = 1;
    
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(mCommandQueue.Get(), TrWindowApp::GetHwnd(), &swapChainDesc, nullptr, nullptr
        , &swapChain));
    ThrowIfFailed(factory->MakeWindowAssociation(TrWindowApp::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));
    ThrowIfFailed(swapChain.As(&mSwapChain));
    mFrameIndex = mSwapChain->GetCurrentBackBufferIndex();

    mRtvHeap.Initialize(
        mDevice.Get(),
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        SwapFrameCount + TrD3D12DeferredRenderTargets::RtvDescriptorCount,
        false,
        L"Main RTV Heap");
    mDsvHeap.Initialize(
        mDevice.Get(),
        D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
        TrD3D12DeferredRenderTargets::DsvDescriptorCount,
        false,
        L"Main DSV Heap");
    mResourceHeap.Initialize(
        mDevice.Get(),
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        ResourceDescriptorCount,
        true,
        L"Shader Resource Heap");

    // create a rtv for each frame
    for(UINT n = 0; n < SwapFrameCount; n++)
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> backBuffer;
        ThrowIfFailed(mSwapChain->GetBuffer(n, IID_PPV_ARGS(&backBuffer)));
        const std::wstring debugName = L"Back Buffer " + std::to_wstring(n);
        mRenderTargets[n].Attach(
            backBuffer.Get(),
            D3D12_RESOURCE_STATE_PRESENT,
            debugName.c_str());
        mRenderTargetViews[n] = mRtvHeap.Allocate();
        mRenderTargets[n].CreateRenderTargetView(
            mDevice.Get(),
            mRenderTargetViews[n].CpuHandle);
    }

    mDeferredRenderTargets.Initialize(
        mDevice.Get(),
        mWidth,
        mHeight,
        mRtvHeap,
        mDsvHeap,
        mResourceHeap);

    for(FrameContext& frame : mFrameContexts)
    {
        ThrowIfFailed(mDevice->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&frame.CommandAllocator)));
    }

    ThrowIfFailed(mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
    mNextFenceValue = 1;
    mFenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if(mFenceEvent == nullptr)
    {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }
}

/* note:
 * upload heap: used for the resources that need to be updated frequently by the cpu, is optimized for cpu write
 * (constant buffers, dynamic vertex buffers)
 * default heap: read-only or rarely updated by the cpu, is optimized for gpu read
 * (static vertex buffers, index buffers)
 */
void TrD3D12RendererRaster::LoadAssetsCornellBox()
{
    mGBufferPass.Initialize(
        mDevice.Get(),
        GetAssetFullPath(SHADER_DIR L"DxRaster/gbuffer.hlsl"));
    mDeferredLightingPass.Initialize(
        mDevice.Get(),
        GetAssetFullPath(SHADER_DIR L"DxRaster/deferred_lighting.hlsl"));
    mCompositePass.Initialize(
        mDevice.Get(),
        GetAssetFullPath(SHADER_DIR L"DxRaster/composite.hlsl"));

    const CornellBoxMeshData meshData = CreateCornellBoxSphereScene();
    TrD3D12UploadContext uploadContext;
    uploadContext.Initialize(mDevice.Get());
    mSceneMesh.Initialize(
        uploadContext,
        meshData.Vertices.data(),
        static_cast<UINT>(meshData.Vertices.size()),
        sizeof(CornellBoxVertex),
        meshData.Indices.data(),
        static_cast<UINT>(meshData.Indices.size()),
        DXGI_FORMAT_R16_UINT);
    uploadContext.ExecuteAndWait(mCommandQueue.Get());

    for(FrameContext& frame : mFrameContexts)
    {
        frame.ConstantBuffer.Initialize(
            mDevice.Get(),
            static_cast<UINT>(sizeof(CornellBoxConstants)));
    }

    ThrowIfFailed(mDevice->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        mFrameContexts[mFrameIndex].CommandAllocator.Get(),
        mGBufferPass.GetPipelineState(),
        IID_PPV_ARGS(&mCommandList)));
    ThrowIfFailed(mCommandList->Close());
}


/* note:
 * RS: root signature
 * IA: input assembler, input data to primitives
 * OM: output merger, after pixel shader
 */
void TrD3D12RendererRaster::PopulateCommandList()
{
    // can only be reset when the associated command lists have finished execution on the GPU
    FrameContext& frame = mFrameContexts[mFrameIndex];
    ThrowIfFailed(frame.CommandAllocator->Reset());

    // after ExecuteCommandList, before re-recording
    ThrowIfFailed(mCommandList->Reset(
        frame.CommandAllocator.Get(),
        mGBufferPass.GetPipelineState()));

    // need to set viewports and scissor rects each frame
    mCommandList->RSSetViewports(1, &mViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    mGBufferPass.Begin(
        mCommandList.Get(),
        mDeferredRenderTargets,
        frame.ConstantBuffer.GetGpuVirtualAddress());

    mSceneMesh.Bind(mCommandList.Get());
    mSceneMesh.Draw(mCommandList.Get());
    mGBufferPass.End(mCommandList.Get(), mDeferredRenderTargets);

    mDeferredLightingPass.Render(
        mCommandList.Get(),
        mDeferredRenderTargets,
        mResourceHeap,
        frame.ConstantBuffer.GetGpuVirtualAddress());

    mCompositePass.Render(
        mCommandList.Get(),
        mResourceHeap,
        mDeferredRenderTargets.GetHdrLightingSrv().GpuHandle,
        mRenderTargets[mFrameIndex],
        mRenderTargetViews[mFrameIndex].CpuHandle);

    ThrowIfFailed(mCommandList->Close());
}

void TrD3D12RendererRaster::MoveToNextFrame()
{
    FrameContext& submittedFrame = mFrameContexts[mFrameIndex];
    const UINT64 submittedFenceValue = mNextFenceValue++;
    ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), submittedFenceValue));
    submittedFrame.FenceValue = submittedFenceValue;

    mFrameIndex = mSwapChain->GetCurrentBackBufferIndex();
    const FrameContext& nextFrame = mFrameContexts[mFrameIndex];

    if(nextFrame.FenceValue != 0 && mFence->GetCompletedValue() < nextFrame.FenceValue)
    {
        ThrowIfFailed(mFence->SetEventOnCompletion(nextFrame.FenceValue, mFenceEvent));
        WaitForSingleObject(mFenceEvent, INFINITE);
    }
}

void TrD3D12RendererRaster::FlushCommandQueue()
{
    const UINT64 fenceValue = mNextFenceValue++;
    ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), fenceValue));

    if(mFence->GetCompletedValue() < fenceValue)
    {
        ThrowIfFailed(mFence->SetEventOnCompletion(fenceValue, mFenceEvent));
        WaitForSingleObject(mFenceEvent, INFINITE);
    }
}

void TrD3D12RendererRaster::ValidateDebugLayer()
{
#if defined(_DEBUG)
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
    if(FAILED(mDevice.As(&infoQueue)))
    {
        return;
    }

    bool hasErrors = false;
    const UINT64 messageCount = infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();
    for(UINT64 index = 0; index < messageCount; ++index)
    {
        SIZE_T messageSize = 0;
        if(FAILED(infoQueue->GetMessage(index, nullptr, &messageSize)) || messageSize == 0)
        {
            continue;
        }

        std::vector<UINT8> messageStorage(messageSize);
        D3D12_MESSAGE* message = reinterpret_cast<D3D12_MESSAGE*>(messageStorage.data());
        if(FAILED(infoQueue->GetMessage(index, message, &messageSize)))
        {
            continue;
        }

        if(message->Severity == D3D12_MESSAGE_SEVERITY_CORRUPTION ||
           message->Severity == D3D12_MESSAGE_SEVERITY_ERROR)
        {
            OutputDebugStringA(message->pDescription);
            OutputDebugStringA("\n");
            hasErrors = true;
        }
    }

    infoQueue->ClearStoredMessages();
    if(hasErrors)
    {
        ThrowIfFailed(E_FAIL);
    }
#endif
}

void TrD3D12RendererRaster::GetHardwareAdapter(IDXGIFactory4* pFactory, REFIID riid, void** ppAdapter)
{
    *ppAdapter = nullptr;
    for(UINT adapterIndex = 0; ; adapterIndex++)
    {
        IDXGIAdapter1* pAdapter = nullptr;
        if(DXGI_ERROR_NOT_FOUND == pFactory->EnumAdapters1(adapterIndex, &pAdapter))
        {
            // no more adapters to enumerate
            break;
        }

        if(SUCCEEDED(D3D12CreateDevice(pAdapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
        {
            *ppAdapter = pAdapter;
            return;
        }
        pAdapter->Release();
    }
    
}
