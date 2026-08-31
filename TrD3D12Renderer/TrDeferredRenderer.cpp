
#include "TrDeferredRenderer.h"
#include "TrCornellBoxScene.h"
#include "TrGlbImporter.h"
#include "TrLog.h"
#include "TrUploadContext.h"
#include "TrWindowApp.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <filesystem>
#include <limits>
#include <stdexcept>


TrDeferredRenderer::TrDeferredRenderer(UINT width, UINT height, std::wstring title) :
    TrRendererBase(width, height, title),
    mFrameIndex(0),
    mViewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
    mScissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height))
{
    DirectX::XMStoreFloat4x4(
        &mPreviousViewProjection,
        DirectX::XMMatrixIdentity());
}

void TrDeferredRenderer::OnInitialize()
{
    TrLog::Info(
        "DX12 renderer initialization started (" +
        std::to_string(mWidth) + "x" + std::to_string(mHeight) + ").");
    LoadPipeline();
    RegisterGpuDebugViews();
    LoadAssets();
    mGpuDebugPanel.Initialize(
        TrWindowApp::GetHwnd(),
        mDevice.Get(),
        SwapFrameCount,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        mResourceHeap,
        mExposure,
        mDepthVisualizationRange);
    mInitialized = true;
    UpdateWindowTitle();
    TrLog::Info("DX12 renderer initialization completed.");
}

void TrDeferredRenderer::OnUpdate()
{
    using namespace DirectX;

    mRuntimeScene.BeginFrame();
    if(mProceduralAnimationNodeId != TrInvalidRuntimeId)
    {
        const float angle = XMConvertToRadians(4.0f) +
            std::sin(static_cast<float>(mFrameNumber) * 0.0125f) *
            XMConvertToRadians(5.0f);
        const XMMATRIX rigTransform =
            XMMatrixTranslation(0.0f, 0.0f, -1.0f) *
            XMMatrixRotationY(angle) *
            XMMatrixTranslation(0.0f, 0.0f, 1.0f);
        XMFLOAT4X4 localTransform;
        XMStoreFloat4x4(&localTransform, rigTransform);
        mRuntimeScene.SetNodeLocalTransform(
            mProceduralAnimationNodeId,
            localTransform);
        mRuntimeScene.UpdateWorldTransforms();
        if(mFrameNumber == 1)
        {
            std::size_t changedInstanceCount = 0;
            for(const TrRuntimeInstance& instance : mRuntimeScene.GetInstances())
            {
                const float* current = &instance.CurrentWorldTransform._11;
                const float* previous = &instance.PreviousWorldTransform._11;
                bool changed = false;
                for(std::size_t element = 0; element < 16; ++element)
                {
                    changed = changed || std::abs(current[element] - previous[element]) > 1.0e-6f;
                }
                if(changed)
                {
                    if(instance.DirtyFlags != TrRuntimeNodeDirtyFlags::Transform)
                    {
                        throw std::logic_error(
                            "Runtime Scene changed an instance without marking it dirty.");
                    }
                    ++changedInstanceCount;
                }
            }
            if(changedInstanceCount < 2)
            {
                throw std::logic_error(
                    "Runtime Scene hierarchy validation did not propagate the parent transform.");
            }
            TrLog::Info(
                "Runtime Scene temporal hierarchy validation passed: " +
                std::to_string(changedInstanceCount) +
                " descendant instances retain distinct Current/Previous transforms.");
        }
    }

    if(mGpuDebugPanel.BuildFrame(
           mGpuDebug,
           mRuntimeScene,
           mGeometryVisualization,
           mExposure,
           mDepthVisualizationRange))
    {
        UpdateWindowTitle();
    }

    const TrAxisAlignedBounds& sceneBounds = mRuntimeScene.GetWorldBounds();
    const XMFLOAT3 sceneBoundsCenter = sceneBounds.GetCenter();
    const float sceneBoundsRadius = std::max(sceneBounds.GetRadius(), 0.001f);
    const float cameraDistance = mUsingImportedScene
        ? std::max(sceneBoundsRadius * 3.2f, 0.1f)
        : 0.0f;
    const XMVECTOR cameraPosition = mUsingImportedScene
        ? XMVectorSet(
            sceneBoundsCenter.x,
            sceneBoundsCenter.y + sceneBoundsRadius * 0.15f,
            sceneBoundsCenter.z - cameraDistance,
            1.0f)
        : XMVectorSet(0.0f, 1.0f, -3.2f, 1.0f);
    const XMVECTOR cameraTarget = mUsingImportedScene
        ? XMLoadFloat3(&sceneBoundsCenter)
        : XMVectorSet(0.0f, 0.95f, 1.0f, 1.0f);
    const XMMATRIX view = XMMatrixLookAtLH(
        cameraPosition,
        cameraTarget,
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    const float nearPlane = mUsingImportedScene
        ? std::max(sceneBoundsRadius * 0.001f, 0.001f)
        : 0.1f;
    const float farPlane = mUsingImportedScene
        ? std::max(sceneBoundsRadius * 10.0f, 100.0f)
        : 100.0f;
    const XMMATRIX projection = XMMatrixPerspectiveFovLH(
        XM_PIDIV4,
        mAspectRatio,
        nearPlane,
        farPlane);
    const XMMATRIX viewProjection = view * projection;
    const XMMATRIX inverseViewProjection = XMMatrixInverse(nullptr, viewProjection);
    TrSceneConstants sceneConstants = {};
    XMStoreFloat3(
        &sceneConstants.LightDirection,
        XMVector3Normalize(XMVectorSet(-0.25f, 1.0f, -0.35f, 0.0f)));

    TrViewConstants viewConstants = {};
    XMStoreFloat4x4(&viewConstants.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&viewConstants.Projection, XMMatrixTranspose(projection));
    XMStoreFloat4x4(&viewConstants.ViewProjection, XMMatrixTranspose(viewProjection));
    XMStoreFloat4x4(
        &viewConstants.InverseViewProjection,
        XMMatrixTranspose(inverseViewProjection));
    const XMMATRIX previousViewProjection = mFrameNumber == 0
        ? viewProjection
        : XMLoadFloat4x4(&mPreviousViewProjection);
    XMStoreFloat4x4(
        &viewConstants.PreviousViewProjection,
        XMMatrixTranspose(previousViewProjection));
    XMStoreFloat3(&viewConstants.CameraPosition, cameraPosition);
    viewConstants.NearPlane = nearPlane;
    viewConstants.FarPlane = farPlane;
    viewConstants.RenderSize = XMFLOAT2(
        static_cast<float>(mWidth),
        static_cast<float>(mHeight));
    viewConstants.InverseRenderSize = XMFLOAT2(
        1.0f / static_cast<float>(mWidth),
        1.0f / static_cast<float>(mHeight));
    viewConstants.FrameNumber = mFrameNumber;

    TrGBufferPassConstants gBufferPassConstants = {};
    gBufferPassConstants.Visualization = mGeometryVisualization;

    const TrDeferredLightingPassConstants lightingPassConstants = {};
    TrCompositePassConstants compositePassConstants = {};
    compositePassConstants.Exposure = mExposure;
    compositePassConstants.VisualizationMode = static_cast<std::uint32_t>(
        mGpuDebug.GetSelectedView().Visualization);
    compositePassConstants.DepthVisualizationRange = mDepthVisualizationRange;
    compositePassConstants.NearPlane = viewConstants.NearPlane;
    compositePassConstants.FarPlane = viewConstants.FarPlane;

    TrFrameContext& frame = mFrameContexts[mFrameIndex];
    frame.SceneConstantBuffer.Update(sceneConstants);
    frame.ViewConstantBuffer.Update(viewConstants);
    frame.GBufferPassConstantBuffer.Update(gBufferPassConstants);
    const std::vector<TrRuntimeInstance>& instances = mRuntimeScene.GetInstances();
    if(frame.PrimitiveConstantBuffers.size() != instances.size())
    {
        throw std::logic_error("Frame instance constant buffers do not match the Runtime Scene.");
    }
    for(std::size_t instanceIndex = 0; instanceIndex < instances.size(); ++instanceIndex)
    {
        const TrRuntimeInstance& instance = instances[instanceIndex];
        const XMMATRIX world = XMLoadFloat4x4(&instance.CurrentWorldTransform);
        const XMMATRIX previousWorld = XMLoadFloat4x4(&instance.PreviousWorldTransform);
        const XMMATRIX worldInverseTranspose = XMMatrixTranspose(
            XMMatrixInverse(nullptr, world));

        TrPrimitiveConstants primitiveConstants = {};
        XMStoreFloat4x4(
            &primitiveConstants.World,
            XMMatrixTranspose(world));
        XMStoreFloat4x4(
            &primitiveConstants.PreviousWorld,
            XMMatrixTranspose(previousWorld));
        XMStoreFloat4x4(
            &primitiveConstants.WorldInverseTranspose,
            XMMatrixTranspose(worldInverseTranspose));
        primitiveConstants.BoundsCenter = instance.CurrentWorldBounds.GetCenter();
        primitiveConstants.BoundsRadius = instance.CurrentWorldBounds.GetRadius();
        primitiveConstants.InstanceId = instance.InstanceId;
        primitiveConstants.MeshId = instance.MeshId;
        const TrRuntimeNode& node = mRuntimeScene.GetNode(instance.NodeId);
        primitiveConstants.ParentNodeId = node.ParentNodeId;
        primitiveConstants.HierarchyDepth = node.HierarchyDepth;
        frame.PrimitiveConstantBuffers[instanceIndex]->Update(primitiveConstants);
    }
    frame.LightingPassConstantBuffer.Update(lightingPassConstants);
    frame.CompositePassConstantBuffer.Update(compositePassConstants);

    XMStoreFloat4x4(&mPreviousViewProjection, viewProjection);
    ++mFrameNumber;
}

void TrDeferredRenderer::OnRender()
{
    PopulateCommandList();
    ID3D12CommandList* ppCommandLists[] = {mCommandList.Get()};
    mCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    ThrowIfFailed(mSwapChain->Present(1, 0));

    MoveToNextFrame();
}

void TrDeferredRenderer::OnResize(UINT width, UINT height)
{
    if(width == 0 || height == 0 || (width == mWidth && height == mHeight))
    {
        return;
    }

    mWidth = width;
    mHeight = height;
    mAspectRatio = static_cast<float>(width) / static_cast<float>(height);
    mViewport = CD3DX12_VIEWPORT(
        0.0f,
        0.0f,
        static_cast<float>(width),
        static_cast<float>(height));
    mScissorRect = CD3DX12_RECT(
        0,
        0,
        static_cast<LONG>(width),
        static_cast<LONG>(height));
    // WM_SIZE can arrive while CreateWindow is still constructing the window.
    if(!mInitialized)
    {
        return;
    }

    TrLog::Info(
        "Resizing render targets to " + std::to_string(width) + "x" +
        std::to_string(height) + ".");

    FlushCommandQueue();
    for(TrTexture& renderTarget : mRenderTargets)
    {
        renderTarget.Reset();
    }

    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    ThrowIfFailed(mSwapChain->GetDesc(&swapChainDesc));
    ThrowIfFailed(mSwapChain->ResizeBuffers(
        SwapFrameCount,
        width,
        height,
        DXGI_FORMAT_UNKNOWN,
        swapChainDesc.Flags));

    mFrameIndex = mSwapChain->GetCurrentBackBufferIndex();
    CreateBackBufferResources();
    mDeferredRenderTargets.Resize(mDevice.Get(), width, height);
    mLightingHistory.Resize(mDevice.Get(), width, height);

    for(TrFrameContext& frame : mFrameContexts)
    {
        frame.FenceValue = 0;
    }
    mFrameNumber = 0;
    DirectX::XMStoreFloat4x4(
        &mPreviousViewProjection,
        DirectX::XMMatrixIdentity());
}

void TrDeferredRenderer::OnDestroy()
{
    TrLog::Info("DX12 renderer shutdown started.");
    FlushCommandQueue();
    mGpuDebugPanel.Shutdown();
    mInitialized = false;
    if(mFenceEvent != nullptr)
    {
        CloseHandle(mFenceEvent);
        mFenceEvent = nullptr;
    }
    ValidateDebugLayer();
    TrLog::Info("DX12 renderer shutdown completed.");
}

void TrDeferredRenderer::OnKeyDown(UINT8 wParam)
{
}

void TrDeferredRenderer::OnKeyUp(UINT8 wParam)
{
}

void TrDeferredRenderer::LoadPipeline()
{
    // enable debug layer
    Microsoft::WRL::ComPtr<ID3D12Debug> debugger;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugger))))
    {
        debugger->EnableDebugLayer();
        TrLog::Info("D3D12 debug layer enabled.");
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
        TrLog::Info("Using the WARP software adapter.");
    }
    else
    {
        Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
        GetHardwareAdapter(factory.Get(), IID_PPV_ARGS(&adapter));

        DXGI_ADAPTER_DESC1 adapterDescription = {};
        ThrowIfFailed(adapter->GetDesc1(&adapterDescription));
        TrLog::Info(
            std::wstring(L"Using hardware adapter: ") + adapterDescription.Description);

        ThrowIfFailed(D3D12CreateDevice(
                adapter.Get(),
                D3D_FEATURE_LEVEL_11_0,
                IID_PPV_ARGS(&mDevice)
            ));
    }
    ValidateShaderModelSupport();
    TrLog::Info("Shader Model 6.5 support validated.");

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
        SwapFrameCount + TrDeferredRenderTargets::RtvDescriptorCount,
        false,
        L"Main RTV Heap");
    mDsvHeap.Initialize(
        mDevice.Get(),
        D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
        TrDeferredRenderTargets::DsvDescriptorCount,
        false,
        L"Main DSV Heap");
    mResourceHeap.Initialize(
        mDevice.Get(),
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        ResourceDescriptorCount,
        true,
        L"Shader Resource Heap");
    mSamplerHeap.Initialize(
        mDevice.Get(),
        D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
        SamplerDescriptorCount,
        true,
        L"Material Sampler Heap");

    // Reserve stable RTV slots. Resize rewrites these descriptors in place.
    for(UINT n = 0; n < SwapFrameCount; n++)
    {
        mRenderTargetViews[n] = mRtvHeap.Allocate();
    }
    CreateBackBufferResources();

    mDeferredRenderTargets.Initialize(
        mDevice.Get(),
        mWidth,
        mHeight,
        mRtvHeap,
        mDsvHeap,
        mResourceHeap);
    mLightingHistory.Initialize(
        mDevice.Get(),
        mWidth,
        mHeight,
        TrDeferredRenderTargets::HdrLightingFormat,
        mResourceHeap,
        L"Indirect Lighting History");

    for(TrFrameContext& frame : mFrameContexts)
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
    TrLog::Info("DX12 pipeline resources created.");
}

void TrDeferredRenderer::CreateBackBufferResources()
{
    for(UINT index = 0; index < SwapFrameCount; ++index)
    {
        if(mRenderTargetViews[index].Index == UINT_MAX)
        {
            throw std::logic_error("Back buffer RTV slot has not been allocated.");
        }

        Microsoft::WRL::ComPtr<ID3D12Resource> backBuffer;
        ThrowIfFailed(mSwapChain->GetBuffer(index, IID_PPV_ARGS(&backBuffer)));
        const std::wstring debugName = L"Back Buffer " + std::to_wstring(index);
        mRenderTargets[index].Attach(
            backBuffer.Get(),
            D3D12_RESOURCE_STATE_PRESENT,
            debugName.c_str());
        mRenderTargets[index].CreateRenderTargetView(
            mDevice.Get(),
            mRenderTargetViews[index].CpuHandle);
    }
}

void TrDeferredRenderer::RegisterGpuDebugViews()
{
    mGpuDebug.Reset();
    mGpuDebug.RegisterView(
        L"Final Lighting",
        mDeferredRenderTargets.GetHdrLightingSrv().GpuHandle,
        TrDebugVisualization::HdrColor);
    mGpuDebug.RegisterView(
        L"Base Color",
        mDeferredRenderTargets.GetBaseColorSrv().GpuHandle,
        TrDebugVisualization::LinearColor);
    mGpuDebug.RegisterView(
        L"World Normal",
        mDeferredRenderTargets.GetNormalSrv().GpuHandle,
        TrDebugVisualization::WorldNormal);
    mGpuDebug.RegisterView(
        L"Roughness",
        mDeferredRenderTargets.GetBaseColorSrv().GpuHandle,
        TrDebugVisualization::ScalarAlpha);
    mGpuDebug.RegisterView(
        L"Metallic",
        mDeferredRenderTargets.GetNormalSrv().GpuHandle,
        TrDebugVisualization::ScalarAlpha);
    mGpuDebug.RegisterView(
        L"Emissive",
        mDeferredRenderTargets.GetEmissiveSrv().GpuHandle,
        TrDebugVisualization::HdrColor);
    mGpuDebug.RegisterView(
        L"Ambient Occlusion",
        mDeferredRenderTargets.GetEmissiveSrv().GpuHandle,
        TrDebugVisualization::ScalarAlpha);
    mGpuDebug.RegisterView(
        L"Linear Depth",
        mDeferredRenderTargets.GetDepthSrv().GpuHandle,
        TrDebugVisualization::DeviceDepth);
}

void TrDeferredRenderer::UpdateWindowTitle() const
{
    const TrGpuDebugView& debugView = mGpuDebug.GetSelectedView();
    const wchar_t* geometryView = L"Shaded";
    if(mGeometryVisualization == TrGeometryVisualization::Hierarchy)
    {
        geometryView = L"Hierarchy";
    }
    else if(mGeometryVisualization == TrGeometryVisualization::PrimitiveDraw)
    {
        geometryView = L"Primitive Draw";
    }
    const std::wstring title = mTitle + L" | GPU Debug [" +
        std::to_wstring(mGpuDebug.GetSelectedIndex()) + L"] " + debugView.Name +
        L" | Geometry " + geometryView;
    SetWindowTextW(TrWindowApp::GetHwnd(), title.c_str());
}

/* note:
 * upload heap: used for the resources that need to be updated frequently by the cpu, is optimized for cpu write
 * (constant buffers, dynamic vertex buffers)
 * default heap: read-only or rarely updated by the cpu, is optimized for gpu read
 * (static vertex buffers, index buffers)
 */
void TrDeferredRenderer::LoadAssets()
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

    TrUploadContext uploadContext;
    uploadContext.Initialize(mDevice.Get());

    if(GetScenePath().empty())
    {
        mLoadedScene = CreateCornellBoxScene();
        for(std::size_t nodeIndex = 0;
            nodeIndex < mLoadedScene.Nodes.size();
            ++nodeIndex)
        {
            if(mLoadedScene.Nodes[nodeIndex].Name == "Sculpture Rig")
            {
                mProceduralAnimationNodeId = static_cast<TrNodeId>(nodeIndex);
                break;
            }
        }
        if(mProceduralAnimationNodeId == TrInvalidRuntimeId)
        {
            throw std::logic_error("Procedural validation scene is missing its animated rig.");
        }
    }
    else
    {
        const std::filesystem::path scenePath(GetScenePath());
        TrLog::Info(std::wstring(L"Loading scene: ") + scenePath.wstring());
        std::wstring extension = scenePath.extension().wstring();
        std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);
        if(extension == L".glb")
        {
            TrGlbImportResult imported = TrGlbImporter::Import(scenePath);
            for(const std::string& warning : imported.Warnings)
            {
                TrLog::Warn("GLB import: " + warning);
            }
            mLoadedScene = std::move(imported.Scene);
        }
        else if(extension == L".trscene")
        {
            mLoadedScene = TrScene::Load(scenePath);
        }
        else
        {
            throw std::invalid_argument("-scene accepts only .glb or .trscene files.");
        }

        mUsingImportedScene = true;
        mTitle = L"Tr Scene - " + scenePath.filename().wstring();
    }
    mRuntimeScene.Initialize(uploadContext, mLoadedScene);
    const TrAxisAlignedBounds& runtimeBounds = mRuntimeScene.GetWorldBounds();
    TrLog::Info(
        "Loaded Runtime Scene '" + mLoadedScene.Name + "': " +
        std::to_string(mRuntimeScene.GetUploadedMeshCount()) + " GPU meshes, " +
        std::to_string(mLoadedScene.Nodes.size()) + " hierarchy nodes, " +
        std::to_string(mRuntimeScene.GetInstances().size()) + " mesh instances, " +
        std::to_string(mRuntimeScene.GetDrawCount()) + " primitive draws.");
    TrLog::Info(
        "Runtime Scene ID/AABB validation passed. World AABB min (" +
        std::to_string(runtimeBounds.Minimum.x) + ", " +
        std::to_string(runtimeBounds.Minimum.y) + ", " +
        std::to_string(runtimeBounds.Minimum.z) + "), max (" +
        std::to_string(runtimeBounds.Maximum.x) + ", " +
        std::to_string(runtimeBounds.Maximum.y) + ", " +
        std::to_string(runtimeBounds.Maximum.z) + ").");
    mMaterialResources.Initialize(
        mDevice.Get(),
        uploadContext,
        mResourceHeap,
        mSamplerHeap,
        &mLoadedScene);
    uploadContext.ExecuteAndWait(mCommandQueue.Get());

    for(TrFrameContext& frame : mFrameContexts)
    {
        frame.SceneConstantBuffer.Initialize(
            mDevice.Get(),
            static_cast<UINT>(sizeof(TrSceneConstants)));
        frame.ViewConstantBuffer.Initialize(
            mDevice.Get(),
            static_cast<UINT>(sizeof(TrViewConstants)));
        frame.GBufferPassConstantBuffer.Initialize(
            mDevice.Get(),
            static_cast<UINT>(sizeof(TrGBufferPassConstants)));
        frame.PrimitiveConstantBuffers.reserve(
            mRuntimeScene.GetInstances().size());
        for(std::size_t instanceIndex = 0;
            instanceIndex < mRuntimeScene.GetInstances().size();
            ++instanceIndex)
        {
            auto constantBuffer = std::make_unique<TrConstantBuffer>();
            constantBuffer->Initialize(
                mDevice.Get(),
                static_cast<UINT>(sizeof(TrPrimitiveConstants)));
            frame.PrimitiveConstantBuffers.push_back(std::move(constantBuffer));
        }
        frame.LightingPassConstantBuffer.Initialize(
            mDevice.Get(),
            static_cast<UINT>(sizeof(TrDeferredLightingPassConstants)));
        frame.CompositePassConstantBuffer.Initialize(
            mDevice.Get(),
            static_cast<UINT>(sizeof(TrCompositePassConstants)));
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
void TrDeferredRenderer::PopulateCommandList()
{
    // can only be reset when the associated command lists have finished execution on the GPU
    TrFrameContext& frame = mFrameContexts[mFrameIndex];
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
        mResourceHeap,
        mSamplerHeap,
        frame.ViewConstantBuffer.GetGpuVirtualAddress(),
        frame.GBufferPassConstantBuffer.GetGpuVirtualAddress());

    const std::vector<TrRuntimeInstance>& instances = mRuntimeScene.GetInstances();
    for(std::size_t instanceIndex = 0; instanceIndex < instances.size(); ++instanceIndex)
    {
        const TrRuntimeInstance& instance = instances[instanceIndex];
        const TrRuntimeMesh& mesh = mRuntimeScene.GetMesh(instance.MeshId);
        mesh.Geometry->Bind(mCommandList.Get());
        for(std::size_t primitiveIndex = 0;
            primitiveIndex < mesh.Primitives.size();
            ++primitiveIndex)
        {
            const TrRuntimePrimitive& primitive = mesh.Primitives[primitiveIndex];
            const TrMaterialGpuBinding& material =
                mMaterialResources.Get(primitive.MaterialId);
            TrDrawConstants drawConstants = {};
            drawConstants.PrimitiveId = primitive.PrimitiveId;
            drawConstants.MaterialId = primitive.MaterialId;
            drawConstants.LocalPrimitiveIndex = primitive.LocalPrimitiveIndex;
            mGBufferPass.SetDrawBindings(
                mCommandList.Get(),
                frame.PrimitiveConstantBuffers[instanceIndex]->GetGpuVirtualAddress(),
                material.Constants,
                material.TextureTable,
                material.SamplerTable,
                drawConstants);
            mesh.Geometry->DrawRange(
                mCommandList.Get(),
                primitive.IndexCount,
                primitive.FirstIndex);
        }
    }
    mGBufferPass.End(mCommandList.Get(), mDeferredRenderTargets);

    mDeferredLightingPass.Render(
        mCommandList.Get(),
        mDeferredRenderTargets,
        mResourceHeap,
        frame.SceneConstantBuffer.GetGpuVirtualAddress(),
        frame.ViewConstantBuffer.GetGpuVirtualAddress(),
        frame.LightingPassConstantBuffer.GetGpuVirtualAddress());

    mCompositePass.Render(
        mCommandList.Get(),
        mResourceHeap,
        mGpuDebug.GetSelectedView().SourceSrv,
        frame.CompositePassConstantBuffer.GetGpuVirtualAddress(),
        mRenderTargets[mFrameIndex],
        mRenderTargetViews[mFrameIndex].CpuHandle);

    mGpuDebugPanel.Render(mCommandList.Get(), mResourceHeap);
    mRenderTargets[mFrameIndex].Transition(
        mCommandList.Get(),
        D3D12_RESOURCE_STATE_PRESENT);

    ThrowIfFailed(mCommandList->Close());
}

void TrDeferredRenderer::MoveToNextFrame()
{
    TrFrameContext& submittedFrame = mFrameContexts[mFrameIndex];
    const UINT64 submittedFenceValue = mNextFenceValue++;
    ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), submittedFenceValue));
    submittedFrame.FenceValue = submittedFenceValue;

    mFrameIndex = mSwapChain->GetCurrentBackBufferIndex();
    const TrFrameContext& nextFrame = mFrameContexts[mFrameIndex];

    if(nextFrame.FenceValue != 0 && mFence->GetCompletedValue() < nextFrame.FenceValue)
    {
        ThrowIfFailed(mFence->SetEventOnCompletion(nextFrame.FenceValue, mFenceEvent));
        WaitForSingleObject(mFenceEvent, INFINITE);
    }
}

void TrDeferredRenderer::FlushCommandQueue()
{
    const UINT64 fenceValue = mNextFenceValue++;
    ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), fenceValue));

    if(mFence->GetCompletedValue() < fenceValue)
    {
        ThrowIfFailed(mFence->SetEventOnCompletion(fenceValue, mFenceEvent));
        WaitForSingleObject(mFenceEvent, INFINITE);
    }
}

void TrDeferredRenderer::ValidateShaderModelSupport() const
{
    D3D12_FEATURE_DATA_SHADER_MODEL shaderModel = {D3D_SHADER_MODEL_6_5};
    const HRESULT result = mDevice->CheckFeatureSupport(
        D3D12_FEATURE_SHADER_MODEL,
        &shaderModel,
        sizeof(shaderModel));
    if(FAILED(result) || shaderModel.HighestShaderModel < D3D_SHADER_MODEL_6_5)
    {
        throw std::runtime_error(
            "Shader Model 6.5 is required for the DX12 renderer.");
    }
}

void TrDeferredRenderer::ValidateDebugLayer()
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
            TrLog::Error(message->pDescription);
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

void TrDeferredRenderer::GetHardwareAdapter(IDXGIFactory4* pFactory, REFIID riid, void** ppAdapter)
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
