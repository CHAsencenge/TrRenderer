
#include "TrD3D12RendererRaster.h"
#include "CornellBoxScene.h"
#include "TrD3D12UploadContext.h"
#include "TrWindowApp.h"


TrD3D12RendererRaster::TrD3D12RendererRaster(UINT width, UINT height, std::wstring title) :
    TrD3D12RendererBase(width, height, title),
    mFrameIndex(0),
    mViewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
    mScissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
    mRtvDescriptorSize(0)
{
}

void TrD3D12RendererRaster::OnInitialize()
{
    LoadPipeline();
    LoadAssetsCornellBox(GetAssetFullPath(SHADER_DIR L"DxRaster/cornell_box.hlsl"));
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

    // create descriptor heaps
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NumDescriptors = SwapFrameCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    ThrowIfFailed(mDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&mRtvHeap)));
    mRtvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    ThrowIfFailed(mDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&mDsvHeap)));
    

    // create frame resources
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart()); // get cpu heap start handle, need d3dx12.h

    // create a rtv for each frame
    for(UINT n = 0; n < SwapFrameCount; n++)
    {
        ThrowIfFailed(mSwapChain->GetBuffer(n, IID_PPV_ARGS(&mRenderTargets[n])));
        mDevice->CreateRenderTargetView(mRenderTargets[n].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, mRtvDescriptorSize); // Offset is declared in d3dx12.h
    }

    D3D12_CLEAR_VALUE depthClearValue = {};
    depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    depthClearValue.DepthStencil.Depth = 1.0f;
    depthClearValue.DepthStencil.Stencil = 0;

    CD3DX12_HEAP_PROPERTIES depthHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC depthResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_D32_FLOAT,
        mWidth,
        mHeight,
        1,
        1,
        1,
        0,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
    ThrowIfFailed(mDevice->CreateCommittedResource(
        &depthHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &depthResourceDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthClearValue,
        IID_PPV_ARGS(&mDepthStencil)));

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    mDevice->CreateDepthStencilView(
        mDepthStencil.Get(),
        &dsvDesc,
        mDsvHeap->GetCPUDescriptorHandleForHeapStart());

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
void TrD3D12RendererRaster::LoadAssetsCornellBox(const std::wstring& filename)
{
    CD3DX12_ROOT_PARAMETER rootParameter;
    rootParameter.InitAsConstantBufferView(
        0,
        0,
        D3D12_SHADER_VISIBILITY_ALL);

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(
        1,
        &rootParameter,
        0,
        nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    const D3D12_INPUT_ELEMENT_DESC inputElements[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };

    TrD3D12GraphicsPipelineDesc pipelineDesc;
    pipelineDesc.ShaderPath = filename;
    pipelineDesc.RootSignatureDesc = &rootSignatureDesc;
    pipelineDesc.InputElements = inputElements;
    pipelineDesc.InputElementCount = _countof(inputElements);
    pipelineDesc.CullMode = D3D12_CULL_MODE_NONE;
    mGraphicsPipeline.Initialize(mDevice.Get(), pipelineDesc);

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
        mGraphicsPipeline.GetPipelineState(),
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
        mGraphicsPipeline.GetPipelineState()));

    mCommandList->SetGraphicsRootSignature(mGraphicsPipeline.GetRootSignature());

    mCommandList->SetGraphicsRootConstantBufferView(
        0,
        frame.ConstantBuffer.GetGpuVirtualAddress());

    // need to set viewports and scissor rects each frame
    mCommandList->RSSetViewports(1, &mViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    CD3DX12_RESOURCE_BARRIER present2RtBarrier = CD3DX12_RESOURCE_BARRIER::Transition(mRenderTargets[mFrameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    mCommandList->ResourceBarrier(1, &present2RtBarrier);
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), mFrameIndex, mRtvDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(mDsvHeap->GetCPUDescriptorHandleForHeapStart());
    mCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    // record commands
    const float clearColor[] = {0.025f, 0.025f, 0.025f, 1.0f};
    mCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    mCommandList->ClearDepthStencilView(
        dsvHandle,
        D3D12_CLEAR_FLAG_DEPTH,
        1.0f,
        0,
        0,
        nullptr);

    mSceneMesh.Bind(mCommandList.Get());
    mSceneMesh.Draw(mCommandList.Get());
    
    CD3DX12_RESOURCE_BARRIER rt2PresentBarrier = CD3DX12_RESOURCE_BARRIER::Transition(mRenderTargets[mFrameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    mCommandList->ResourceBarrier(1, &rt2PresentBarrier);

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
