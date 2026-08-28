
#include "TrD3D12RendererRaster.h"
#include "VertexBase.h"
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
    // LoadAssets(GetAssetFullPath(L"shaders.hlsl"));
    LoadAssetsTexture(GetAssetFullPath(SHADER_DIR L"DxRaster/shaders_texture.hlsl"));
}

void TrD3D12RendererRaster::OnUpdate()
{
    using namespace DirectX;

    const XMMATRIX model = XMMatrixIdentity();
    const XMMATRIX view = XMMatrixLookAtLH(
        XMVectorSet(0.0f, 0.0f, -2.5f, 1.0f),
        XMVectorSet(0.0f, 0.0f, 0.25f, 1.0f),
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    const XMMATRIX projection = XMMatrixPerspectiveFovLH(
        XM_PIDIV4,
        mAspectRatio,
        0.1f,
        100.0f);

    SceneConstants constants = {};
    XMStoreFloat4x4(&constants.ModelViewProjection, XMMatrixTranspose(model * view * projection));
    memcpy(mFrameContexts[mFrameIndex].ConstantBufferData, &constants, sizeof(constants));
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
    for(FrameContext& frame : mFrameContexts)
    {
        if(frame.ConstantBuffer != nullptr && frame.ConstantBufferData != nullptr)
        {
            frame.ConstantBuffer->Unmap(0, nullptr);
            frame.ConstantBufferData = nullptr;
        }
    }

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

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    srvHeapDesc.NumDescriptors = 1;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    ThrowIfFailed(mDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvHeap)));

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
    ThrowIfFailed(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE, IID_PPV_ARGS(&mBundleAllocator)));
}

/* note:
 * upload heap: used for the resources that need to be updated frequently by the cpu, is optimized for cpu write
 * (constant buffers, dynamic vertex buffers)
 * default heap: read-only or rarely updated by the cpu, is optimized for gpu read
 * (static vertex buffers, index buffers)
 */
void TrD3D12RendererRaster::LoadAssets(const std::wstring filename)
{
    // create root signature (resources used for xxx)
    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;

    /* note: D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
     * the root signature allows the input assembler input layout to be bound during the pipeline state object (PSO) creation
     */
    rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    /// output buffer of the serialized desc data
    Microsoft::WRL::ComPtr<ID3DBlob> signature;
    Microsoft::WRL::ComPtr<ID3DBlob> error;
    /// convert the root signature description into a serialized form
    /// a binary representation that can be used for creating a root signature object
    ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
    /// create the root signature from the serialized data
    ThrowIfFailed(mDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));

    // create the pipeline state, which includes compiling and loading shaders
    Microsoft::WRL::ComPtr<ID3DBlob> vertexShader;
    Microsoft::WRL::ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
    // d3dcompiler.h
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif
    // todo: flexible shader read
    ThrowIfFailed(D3DCompileFromFile(filename.c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
    ThrowIfFailed(D3DCompileFromFile(filename.c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));
    
    // define the vertex input layout
    D3D12_INPUT_ELEMENT_DESC inputElementDesc[] =
    {
        /* note:
         * SemanticIndex differentiates between multiple elements with the same semantic name
         * InpputSlot identifies the vertex buffer binding slot from which the data sourced
         */
        {"POSITION", 0 ,DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };
    
    // describe and create graphics pipeline state object
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = {inputElementDesc, _countof(inputElementDesc)};
    psoDesc.pRootSignature = mRootSignature.Get();
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPipelineState)));
    
    // create the command list
    ThrowIfFailed(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mFrameContexts[mFrameIndex].CommandAllocator.Get(), mPipelineState.Get(), IID_PPV_ARGS(&mCommandList)))
    ThrowIfFailed(mCommandList->Close());
    
    // create vertex buffer
    // todo: find a more flexible way to populate vertex
    VertexBase triangleVertices[] = 
        {
        {{ 0.0f, 0.25f * mAspectRatio, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{0.25f, -0.25f * mAspectRatio, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
        {{-0.25f, -0.25f * mAspectRatio, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
        };
    
    UINT vertexBufferSize = sizeof(triangleVertices);

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
    ThrowIfFailed(mDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mVertexBuffer)));

    UINT8* pVertexDataBegin;
    CD3DX12_RANGE readRange (0, 0);
    ThrowIfFailed(mVertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
    memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
    mVertexBuffer->Unmap(0, nullptr);

    mVertexBufferView.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
    mVertexBufferView.SizeInBytes = vertexBufferSize;
    mVertexBufferView.StrideInBytes = sizeof(VertexBase);

    // Create and record bundle (pre-set)
    ThrowIfFailed(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE, mBundleAllocator.Get(), mPipelineState.Get(), IID_PPV_ARGS(&mBundle)));
    mBundle->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    mBundle->IASetVertexBuffers(0, 1, &mVertexBufferView);
    mBundle->DrawInstanced(3, 1, 0, 0);
    ThrowIfFailed(mBundle->Close());

    
    ThrowIfFailed(mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
    mNextFenceValue = 1;
    mFenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if(mFenceEvent == nullptr)
    {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }

    FlushCommandQueue();
}

void TrD3D12RendererRaster::LoadAssetsTexture(const std::wstring filename)
{
    // create root signature (resources used for xxx)
    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if(FAILED(mDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
    {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;        
    }
    
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    
    CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
    
    CD3DX12_ROOT_PARAMETER1 rootParameters[2];
    rootParameters[0].InitAsConstantBufferView(
        0,
        0,
        D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE,
        D3D12_SHADER_VISIBILITY_VERTEX);
    rootParameters[1].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 0;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MinLOD = 0.0f; // 0 is the most detailed mipmap level, any level higher than that is less detailed
    sampler.MaxLOD = D3D12_FLOAT32_MAX;  // no upper limit
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // populate root signature desc
    rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    /// output buffer of the serialized desc data
    Microsoft::WRL::ComPtr<ID3DBlob> signature;
    Microsoft::WRL::ComPtr<ID3DBlob> error;
    /// convert the root signature description into a serialized form
    /// a binary representation that can be used for creating a root signature object
    ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
    /// create the root signature from the serialized data
    ThrowIfFailed(mDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));

    // create the pipeline state, which includes compiling and loading shaders
    Microsoft::WRL::ComPtr<ID3DBlob> vertexShader;
    Microsoft::WRL::ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
    // d3dcompiler.h
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif
    // todo: flexible shader read
    ThrowIfFailed(D3DCompileFromFile(filename.c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
    ThrowIfFailed(D3DCompileFromFile(filename.c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));
    
    // define the vertex input layout
    D3D12_INPUT_ELEMENT_DESC inputElementDesc[] =
    {
        {"POSITION", 0 ,DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };
    
    // describe and create graphics pipeline state object
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = {inputElementDesc, _countof(inputElementDesc)};
    psoDesc.pRootSignature = mRootSignature.Get();
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count = 1;
    ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPipelineState)));
    
    // create the command list
    ThrowIfFailed(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mFrameContexts[mFrameIndex].CommandAllocator.Get(), mPipelineState.Get(), IID_PPV_ARGS(&mCommandList)))
    // ThrowIfFailed(mCommandList->Close());
    
    // The small front quad is submitted before the larger back quad. The
    // depth buffer must reject the back quad in their overlapping region.
    const TextureVertexBase vertices[] =
    {
        { {-0.45f,  0.45f, 0.0f}, {0.0f, 0.0f} },
        { { 0.45f,  0.45f, 0.0f}, {1.0f, 0.0f} },
        { { 0.45f, -0.45f, 0.0f}, {1.0f, 1.0f} },
        { {-0.45f, -0.45f, 0.0f}, {0.0f, 1.0f} },
        { {-0.80f,  0.80f, 0.5f}, {0.0f, 0.0f} },
        { { 0.80f,  0.80f, 0.5f}, {4.0f, 0.0f} },
        { { 0.80f, -0.80f, 0.5f}, {4.0f, 4.0f} },
        { {-0.80f, -0.80f, 0.5f}, {0.0f, 4.0f} }
    };
    const UINT16 indices[] =
    {
        0, 1, 2, 0, 2, 3,
        4, 5, 6, 4, 6, 7
    };

    const UINT vertexBufferSize = sizeof(vertices);
    const UINT indexBufferSize = sizeof(indices);
    mIndexCount = _countof(indices);

    CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_HEAP_PROPERTIES bufferUploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
    CD3DX12_RESOURCE_DESC indexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);

    ThrowIfFailed(mDevice->CreateCommittedResource(
        &defaultHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &vertexBufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&mVertexBuffer)));
    ThrowIfFailed(mDevice->CreateCommittedResource(
        &defaultHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &indexBufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&mIndexBuffer)));

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexUploadHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexUploadHeap;
    ThrowIfFailed(mDevice->CreateCommittedResource(
        &bufferUploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &vertexBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&vertexUploadHeap)));
    ThrowIfFailed(mDevice->CreateCommittedResource(
        &bufferUploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &indexBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&indexUploadHeap)));

    D3D12_SUBRESOURCE_DATA vertexData = {};
    vertexData.pData = vertices;
    vertexData.RowPitch = vertexBufferSize;
    vertexData.SlicePitch = vertexBufferSize;
    UpdateSubresources(mCommandList.Get(), mVertexBuffer.Get(), vertexUploadHeap.Get(), 0, 0, 1, &vertexData);

    D3D12_SUBRESOURCE_DATA indexData = {};
    indexData.pData = indices;
    indexData.RowPitch = indexBufferSize;
    indexData.SlicePitch = indexBufferSize;
    UpdateSubresources(mCommandList.Get(), mIndexBuffer.Get(), indexUploadHeap.Get(), 0, 0, 1, &indexData);

    CD3DX12_RESOURCE_BARRIER bufferBarriers[] =
    {
        CD3DX12_RESOURCE_BARRIER::Transition(
            mVertexBuffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER),
        CD3DX12_RESOURCE_BARRIER::Transition(
            mIndexBuffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_INDEX_BUFFER)
    };
    mCommandList->ResourceBarrier(_countof(bufferBarriers), bufferBarriers);

    mVertexBufferView.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
    mVertexBufferView.SizeInBytes = vertexBufferSize;
    mVertexBufferView.StrideInBytes = sizeof(TextureVertexBase);

    mIndexBufferView.BufferLocation = mIndexBuffer->GetGPUVirtualAddress();
    mIndexBufferView.SizeInBytes = indexBufferSize;
    mIndexBufferView.Format = DXGI_FORMAT_R16_UINT;

    D3D12_RESOURCE_DESC textureDesc = {}; 
    textureDesc.MipLevels = 1;  // number of levels
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.Width = TextureWidth;
    textureDesc.Height = TextureHeight;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    textureDesc.DepthOrArraySize = 1;  // specifies the array size if it is an array of 1D or 2D resources, or the depth of the resource if it is 3D
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    // texture
    CD3DX12_HEAP_PROPERTIES texHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(mDevice->CreateCommittedResource(
        &texHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&mTexture)));

    Microsoft::WRL::ComPtr<ID3D12Resource> textureUploadHeap;
    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(mTexture.Get(), 0, 1);

    // texture upload heap
    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
    ThrowIfFailed(mDevice->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &uploadBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&textureUploadHeap)));

    // copy texture data to the intermediate upload heap, then from the upload heap to the Texture2D
    std::vector<UINT8> data = GenerateTextureData();
    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = &data[0];
    textureData.RowPitch = TextureWidth * TexturePixelSize;
    textureData.SlicePitch = TextureWidth * TextureHeight * TexturePixelSize; // meaning?

    UpdateSubresources(mCommandList.Get(), mTexture.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);
    // after copied to GPU, texture should be shader resource
    CD3DX12_RESOURCE_BARRIER dest2ShaderResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(mTexture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    mCommandList->ResourceBarrier(1, &dest2ShaderResourceBarrier);
    // so describe and create a SRV
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;  // Specifies how memory gets routed by a shader resource view
    srvDesc.Texture2D.MipLevels = 1;  // union
    mDevice->CreateShaderResourceView(mTexture.Get(), &srvDesc, mSrvHeap->GetCPUDescriptorHandleForHeapStart());

    // Each swap-chain frame owns its upload-buffer slice so the CPU never
    // overwrites constants that are still being consumed by the GPU.
    CD3DX12_HEAP_PROPERTIES constantBufferHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(ConstantBufferSize);
    const CD3DX12_RANGE noCpuReads(0, 0);
    SceneConstants initialConstants = {};
    DirectX::XMStoreFloat4x4(
        &initialConstants.ModelViewProjection,
        DirectX::XMMatrixIdentity());

    for(FrameContext& frame : mFrameContexts)
    {
        ThrowIfFailed(mDevice->CreateCommittedResource(
            &constantBufferHeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &constantBufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&frame.ConstantBuffer)));
        ThrowIfFailed(frame.ConstantBuffer->Map(
            0,
            &noCpuReads,
            reinterpret_cast<void**>(&frame.ConstantBufferData)));
        memcpy(frame.ConstantBufferData, &initialConstants, sizeof(initialConstants));
    }

    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* ppCommandLists[] = {mCommandList.Get()};
    mCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    
    
    ThrowIfFailed(mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
    mNextFenceValue = 1;
    mFenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if(mFenceEvent == nullptr)
    {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }

    FlushCommandQueue();
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
    ThrowIfFailed(mCommandList->Reset(frame.CommandAllocator.Get(), mPipelineState.Get()));

    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    /// srv descriptor heaps
    ID3D12DescriptorHeap* ppHeaps[] = {mSrvHeap.Get()};
    mCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
    mCommandList->SetGraphicsRootConstantBufferView(
        0,
        frame.ConstantBuffer->GetGPUVirtualAddress());
    mCommandList->SetGraphicsRootDescriptorTable(
        1,
        mSrvHeap->GetGPUDescriptorHandleForHeapStart());

    // need to set viewports and scissor rects each frame
    mCommandList->RSSetViewports(1, &mViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    CD3DX12_RESOURCE_BARRIER present2RtBarrier = CD3DX12_RESOURCE_BARRIER::Transition(mRenderTargets[mFrameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    mCommandList->ResourceBarrier(1, &present2RtBarrier);
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), mFrameIndex, mRtvDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(mDsvHeap->GetCPUDescriptorHandleForHeapStart());
    mCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    // record commands
    const float clearColor[] = {0.0f, 0.2f, 0.4f, 1.0f};
    mCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    mCommandList->ClearDepthStencilView(
        dsvHandle,
        D3D12_CLEAR_FLAG_DEPTH,
        1.0f,
        0,
        0,
        nullptr);

    mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    mCommandList->IASetVertexBuffers(0, 1, &mVertexBufferView);
    mCommandList->IASetIndexBuffer(&mIndexBufferView);
    mCommandList->DrawIndexedInstanced(mIndexCount, 1, 0, 0, 0);
    
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

std::vector<UINT8> TrD3D12RendererRaster::GenerateTextureData()
{
    const UINT textureSize = TextureWidth * TextureHeight * TexturePixelSize;
    std::vector<UINT8> data(textureSize);
    UINT8* pData = &data[0];

    for(UINT n = 0; n < textureSize; n += TexturePixelSize)
    {
        // coordinate
        UINT x = n % (TextureWidth * TexturePixelSize);  // [0, TextureWidth * TexturePixelSize]
        UINT y = n / (TextureWidth * TexturePixelSize);  // [0, TextureHeight]

        // cell coordinate
        UINT cx = x / ((TextureWidth * TexturePixelSize) >> 3);
        UINT cy = y / (TextureHeight >> 3);

        // RGBA
        if (cx % 2 == cy % 2)
        {
            pData[n] = 0x00;
            pData[n + 1] = 0x00;
            pData[n + 2] = 0x00;
            pData[n + 3] = 0xff;
        }
        else
        {
            pData[n] = 0xff;
            pData[n + 1] = 0xff;
            pData[n + 2] = 0xff;
            pData[n + 3] = 0xff;
        }
    }
    return data;
}
