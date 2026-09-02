
#include "TrDeferredRenderer.h"
#include "TrRenderConfig.h"
#include "Scene/TrCornellBoxScene.h"
#include "TrGlbImporter.h"
#include "TrLog.h"
#include "Backend/TrUploadContext.h"
#include "App/TrWindowApp.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <filesystem>
#include <limits>
#include <stdexcept>

namespace
{
    std::wstring NormalizeScenePath(const std::filesystem::path& path)
    {
        std::error_code error;
        std::filesystem::path normalized = std::filesystem::absolute(path, error);
        if(error)
        {
            normalized = path;
            error.clear();
        }
        const std::filesystem::path canonical =
            std::filesystem::weakly_canonical(normalized, error);
        if(!error)
        {
            normalized = canonical;
        }
        return normalized.lexically_normal().wstring();
    }

    bool ScenePathsEqual(const std::wstring& lhs, const std::wstring& rhs)
    {
        return _wcsicmp(lhs.c_str(), rhs.c_str()) == 0;
    }

    bool ShouldRenderInDepthNormalPrepass(TrSceneAlphaMode alphaMode)
    {
        if(!TrRenderConfig::IsDepthNormalPrepassEnabled ||
           alphaMode == TrSceneAlphaMode::Blend)
        {
            return false;
        }
        return alphaMode == TrSceneAlphaMode::Opaque ||
            (alphaMode == TrSceneAlphaMode::Mask &&
             TrRenderConfig::IncludeMaskedInDepthNormalPrepass);
    }

    float Halton(UINT index, UINT base)
    {
        float result = 0.0f;
        float fraction = 1.0f;
        while(index > 0)
        {
            fraction /= static_cast<float>(base);
            result += fraction * static_cast<float>(index % base);
            index /= base;
        }
        return result;
    }

    DirectX::XMFLOAT2 CalculateTemporalJitterNdc(
        UINT frameNumber,
        UINT width,
        UINT height)
    {
        const UINT sequenceIndex = frameNumber % 8u + 1u;
        const float jitterPixelsX = Halton(sequenceIndex, 2u) - 0.5f;
        const float jitterPixelsY = Halton(sequenceIndex, 3u) - 0.5f;
        return DirectX::XMFLOAT2(
            2.0f * jitterPixelsX / static_cast<float>(width),
            -2.0f * jitterPixelsY / static_cast<float>(height));
    }
}


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
    TrLog::Info(
        TrRenderConfig::UseReversedZ
            ? "Depth convention: Reversed-Z (clear 0, compare greater-equal)."
            : "Depth convention: Forward-Z (clear 1, compare less-equal).");
    TrLog::Info(
        std::string("Depth/Normal prepass mode: ") +
        TrRenderConfig::GetPrepassModeName() + ".");
    LoadPipeline();
    RegisterGpuDebugViews();
    LoadAssets();
    InitializeCamera();
    BuildSceneSelectionList();
    mPerformanceMonitor.Reset();
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

    mPerformanceMonitor.Tick();

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

    std::optional<std::wstring> sceneChangeRequest;
    const TrGeometryVisualization previousGeometryVisualization =
        mGeometryVisualization;
    const bool previousIndirectLighting = mPipelineFeatures.IsEnabled(
        TrPipelineFeature::IndirectLighting);
    if(mGpuDebugPanel.BuildFrame(
           mGpuDebug,
           mRuntimeScene,
           mPerformanceMonitor.GetSnapshot(),
           mPipelineFeatures,
           mGeometryVisualization,
           mExposure,
           mDepthVisualizationRange,
           mSceneSelectionEntries,
           mCurrentSceneSelectionIndex,
           sceneChangeRequest))
    {
        UpdateWindowTitle();
    }
    const bool indirectLightingEnabled = mPipelineFeatures.IsEnabled(
        TrPipelineFeature::IndirectLighting);
    if(previousGeometryVisualization != mGeometryVisualization ||
       previousIndirectLighting != indirectLightingEnabled)
    {
        mScreenProbeResources.InvalidateHistory();
        mTaaColorHistory.Invalidate();
        mTaaDepthHistory.Invalidate();
    }
    if(previousIndirectLighting != indirectLightingEnabled)
    {
        TrLog::Info(
            std::string("Screen Probe indirect lighting ") +
            (indirectLightingEnabled ? "enabled." : "disabled."));
    }
    if(sceneChangeRequest.has_value())
    {
        mRequestedScenePath = std::move(sceneChangeRequest);
        const std::wstring displayPath = mRequestedScenePath->empty()
            ? L"Procedural Cornell Box"
            : *mRequestedScenePath;
        TrLog::Info(L"Scene switch requested: " + displayPath);
        PostMessageW(TrWindowApp::GetHwnd(), WM_CLOSE, 0, 0);
        return;
    }

    const auto updateTime = std::chrono::steady_clock::now();
    const float deltaSeconds = std::clamp(
        std::chrono::duration<float>(updateTime - mLastCameraUpdateTime).count(),
        0.0f,
        0.1f);
    mLastCameraUpdateTime = updateTime;
    UpdateCamera(deltaSeconds);

    constexpr float verticalFieldOfView = XM_PIDIV4;
    const float cosPitch = std::cos(mCameraPitch);
    const XMVECTOR cameraForward = XMVectorSet(
        std::sin(mCameraYaw) * cosPitch,
        std::sin(mCameraPitch),
        std::cos(mCameraYaw) * cosPitch,
        0.0f);
    const XMVECTOR cameraPosition = XMLoadFloat3(&mCameraPosition);
    const XMMATRIX view = XMMatrixLookToLH(
        cameraPosition,
        cameraForward,
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    const XMMATRIX unjitteredProjection = TrRenderConfig::CreatePerspectiveFovLH(
        verticalFieldOfView,
        mAspectRatio,
        mCameraNearPlane,
        mCameraFarPlane);
    const XMFLOAT2 currentJitter = CalculateTemporalJitterNdc(
        mFrameNumber,
        mWidth,
        mHeight);
    mPreviousTemporalJitter = mFrameNumber == 0
        ? currentJitter
        : mTemporalJitter;
    mTemporalJitter = currentJitter;
    const XMMATRIX projection = unjitteredProjection * XMMatrixTranslation(
        currentJitter.x,
        currentJitter.y,
        0.0f);
    const XMMATRIX viewProjection = view * projection;
    const XMMATRIX unjitteredViewProjection = view * unjitteredProjection;
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
        ? unjitteredViewProjection
        : XMLoadFloat4x4(&mPreviousViewProjection);
    XMStoreFloat4x4(
        &viewConstants.PreviousViewProjection,
        XMMatrixTranspose(previousViewProjection));
    XMStoreFloat3(&viewConstants.CameraPosition, cameraPosition);
    viewConstants.NearPlane = mCameraNearPlane;
    viewConstants.FarPlane = mCameraFarPlane;
    viewConstants.RenderSize = XMFLOAT2(
        static_cast<float>(mWidth),
        static_cast<float>(mHeight));
    viewConstants.InverseRenderSize = XMFLOAT2(
        1.0f / static_cast<float>(mWidth),
        1.0f / static_cast<float>(mHeight));
    viewConstants.TemporalJitter = mTemporalJitter;
    viewConstants.PreviousTemporalJitter = mPreviousTemporalJitter;
    viewConstants.FrameNumber = mFrameNumber;

    TrGBufferPassConstants gBufferPassConstants = {};
    gBufferPassConstants.Visualization = mGeometryVisualization;

    TrDeferredLightingPassConstants lightingPassConstants = {};
    lightingPassConstants.FeatureMask = mPipelineFeatures.GetEnabledMask();
    const TrForwardTransparentPassConstants transparentPassConstants = {};
    TrCompositePassConstants compositePassConstants = {};
    compositePassConstants.Exposure = mExposure;
    compositePassConstants.VisualizationMode = static_cast<std::uint32_t>(
        mGpuDebug.GetSelectedView().Visualization);
    compositePassConstants.DepthVisualizationRange = mDepthVisualizationRange;
    compositePassConstants.NearPlane = viewConstants.NearPlane;
    compositePassConstants.FarPlane = viewConstants.FarPlane;
    compositePassConstants.OutputSize = viewConstants.RenderSize;

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
    frame.ForwardTransparentPassConstantBuffer.Update(transparentPassConstants);
    frame.CompositePassConstantBuffer.Update(compositePassConstants);

    XMStoreFloat4x4(&mPreviousViewProjection, unjitteredViewProjection);
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
    mHierarchicalDepth.Resize(mDevice.Get(), width, height);
    mScreenProbeResources.Resize(mDevice.Get(), width, height);
    mTaaColorHistory.Resize(mDevice.Get(), width, height);
    mTaaDepthHistory.Resize(mDevice.Get(), width, height);
    RegisterGpuDebugViews();

    for(TrFrameContext& frame : mFrameContexts)
    {
        frame.FenceValue = 0;
    }
    mFrameNumber = 0;
    DirectX::XMStoreFloat4x4(
        &mPreviousViewProjection,
        DirectX::XMMatrixIdentity());
    mTemporalJitter = {0.0f, 0.0f};
    mPreviousTemporalJitter = {0.0f, 0.0f};
    mPerformanceMonitor.Reset();
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
    switch(wParam)
    {
    case 'W':
        mMoveForward = true;
        break;
    case 'S':
        mMoveBackward = true;
        break;
    case 'A':
        mMoveLeft = true;
        break;
    case 'D':
        mMoveRight = true;
        break;
    }
}

void TrDeferredRenderer::OnKeyUp(UINT8 wParam)
{
    switch(wParam)
    {
    case 'W':
        mMoveForward = false;
        break;
    case 'S':
        mMoveBackward = false;
        break;
    case 'A':
        mMoveLeft = false;
        break;
    case 'D':
        mMoveRight = false;
        break;
    }
}

void TrDeferredRenderer::OnMouseMove(INT x, INT y)
{
    if(!mMouseLookActive)
    {
        return;
    }

    constexpr float mouseSensitivity = 0.003f;
    const INT deltaX = x - mLastMouseX;
    const INT deltaY = y - mLastMouseY;
    mLastMouseX = x;
    mLastMouseY = y;

    mCameraYaw += static_cast<float>(deltaX) * mouseSensitivity;
    mCameraPitch -= static_cast<float>(deltaY) * mouseSensitivity;
    constexpr float maximumPitch = DirectX::XM_PIDIV2 - 0.01f;
    mCameraPitch = std::clamp(mCameraPitch, -maximumPitch, maximumPitch);
}

void TrDeferredRenderer::OnRightMouseButtonDown(INT x, INT y)
{
    mMouseLookActive = true;
    mLastMouseX = x;
    mLastMouseY = y;
}

void TrDeferredRenderer::OnRightMouseButtonUp()
{
    mMouseLookActive = false;
}

void TrDeferredRenderer::OnInputFocusLost()
{
    mMoveForward = false;
    mMoveBackward = false;
    mMoveLeft = false;
    mMoveRight = false;
    mMouseLookActive = false;
}

void TrDeferredRenderer::InitializeCamera()
{
    using namespace DirectX;

    const TrAxisAlignedBounds& sceneBounds = mUsingImportedScene
        ? mCameraBounds
        : mRuntimeScene.GetWorldBounds();
    const TrAxisAlignedBounds& fullSceneBounds = mRuntimeScene.GetWorldBounds();
    const XMFLOAT3 sceneBoundsCenter = sceneBounds.GetCenter();
    const float sceneBoundsRadius = std::max(sceneBounds.GetRadius(), 0.001f);
    const float fullSceneBoundsRadius = std::max(
        fullSceneBounds.GetRadius(),
        0.001f);
    const float halfWidth =
        (sceneBounds.Maximum.x - sceneBounds.Minimum.x) * 0.5f;
    const float halfHeight =
        (sceneBounds.Maximum.y - sceneBounds.Minimum.y) * 0.5f;
    const float halfDepth =
        (sceneBounds.Maximum.z - sceneBounds.Minimum.z) * 0.5f;
    constexpr float verticalFieldOfView = XM_PIDIV4;
    const float tangentHalfFov = std::tan(verticalFieldOfView * 0.5f);
    const float distanceToFitHeight = halfHeight / tangentHalfFov;
    const float distanceToFitWidth = halfWidth /
        std::max(mAspectRatio * tangentHalfFov, 0.001f);
    const float cameraDistance = mUsingImportedScene
        ? std::max(
            (std::max(distanceToFitHeight, distanceToFitWidth) + halfDepth) *
                1.15f,
            0.1f)
        : 0.0f;

    const XMVECTOR cameraPosition = mUsingImportedScene
        ? XMVectorSet(
            sceneBoundsCenter.x,
            sceneBoundsCenter.y + halfHeight * 0.15f,
            sceneBoundsCenter.z - cameraDistance,
            1.0f)
        : XMVectorSet(0.0f, 1.0f, -3.2f, 1.0f);
    const XMVECTOR cameraTarget = mUsingImportedScene
        ? XMLoadFloat3(&sceneBoundsCenter)
        : XMVectorSet(0.0f, 0.95f, 1.0f, 1.0f);
    const XMVECTOR cameraForward = XMVector3Normalize(
        XMVectorSubtract(cameraTarget, cameraPosition));
    XMFLOAT3 initialForward;
    XMStoreFloat3(&initialForward, cameraForward);
    XMStoreFloat3(&mCameraPosition, cameraPosition);
    mCameraYaw = std::atan2(initialForward.x, initialForward.z);
    mCameraPitch = std::asin(std::clamp(initialForward.y, -1.0f, 1.0f));
    mCameraMoveSpeed = mUsingImportedScene
        ? std::max(sceneBoundsRadius * 0.5f, 0.1f)
        : 2.0f;
    mCameraNearPlane = mUsingImportedScene
        ? std::max(sceneBoundsRadius * 0.001f, 0.001f)
        : 0.1f;
    mCameraFarPlane = mUsingImportedScene
        ? std::max(
            std::max(sceneBoundsRadius * 10.0f, fullSceneBoundsRadius * 2.0f),
            100.0f)
        : 100.0f;
    mLastCameraUpdateTime = std::chrono::steady_clock::now();
}

void TrDeferredRenderer::UpdateCamera(float deltaSeconds)
{
    using namespace DirectX;

    const float forwardInput =
        static_cast<float>(mMoveForward) - static_cast<float>(mMoveBackward);
    const float rightInput =
        static_cast<float>(mMoveRight) - static_cast<float>(mMoveLeft);
    if(forwardInput == 0.0f && rightInput == 0.0f)
    {
        return;
    }

    const float cosPitch = std::cos(mCameraPitch);
    const XMVECTOR forward = XMVectorSet(
        std::sin(mCameraYaw) * cosPitch,
        std::sin(mCameraPitch),
        std::cos(mCameraYaw) * cosPitch,
        0.0f);
    const XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    const XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, forward));
    XMVECTOR movement = XMVectorAdd(
        XMVectorScale(forward, forwardInput),
        XMVectorScale(right, rightInput));
    movement = XMVector3Normalize(movement);

    XMVECTOR position = XMLoadFloat3(&mCameraPosition);
    position = XMVectorMultiplyAdd(
        movement,
        XMVectorReplicate(mCameraMoveSpeed * deltaSeconds),
        position);
    XMStoreFloat3(&mCameraPosition, position);
}

void TrDeferredRenderer::BuildSceneSelectionList()
{
    mSceneSelectionEntries.clear();
    mSceneSelectionEntries.push_back({
        L"Cornell Box [Procedural]",
        L"Built-in procedural validation scene",
        L""});

    const std::filesystem::path projectRoot(SLN_ROOT_DIR);
    const std::filesystem::path searchRoots[] = {
        projectRoot / "Assets",
        projectRoot / "artifacts"};

    auto addScene = [&](const std::filesystem::path& path)
    {
        std::wstring extension = path.extension().wstring();
        std::transform(
            extension.begin(), extension.end(), extension.begin(), ::towlower);
        if(extension != L".glb" && extension != L".trscene")
        {
            return;
        }

        const std::wstring normalizedPath = NormalizeScenePath(path);
        const auto duplicate = std::find_if(
            mSceneSelectionEntries.begin(),
            mSceneSelectionEntries.end(),
            [&](const TrSceneSelectionEntry& entry)
            {
                return !entry.ScenePath.empty() &&
                    ScenePathsEqual(entry.ScenePath, normalizedPath);
            });
        if(duplicate != mSceneSelectionEntries.end())
        {
            return;
        }

        std::error_code relativeError;
        std::filesystem::path detail =
            std::filesystem::relative(path, projectRoot, relativeError);
        if(relativeError)
        {
            detail = path;
        }
        const std::wstring type = extension == L".glb" ? L"GLB" : L"TRSCENE";
        mSceneSelectionEntries.push_back({
            path.stem().wstring() + L" [" + type + L"]",
            detail.generic_wstring(),
            normalizedPath});
    };

    for(const std::filesystem::path& searchRoot : searchRoots)
    {
        std::error_code existsError;
        if(!std::filesystem::is_directory(searchRoot, existsError))
        {
            continue;
        }

        try
        {
            for(const std::filesystem::directory_entry& entry :
                std::filesystem::recursive_directory_iterator(
                    searchRoot,
                    std::filesystem::directory_options::skip_permission_denied))
            {
                if(entry.is_regular_file())
                {
                    addScene(entry.path());
                }
            }
        }
        catch(const std::filesystem::filesystem_error& error)
        {
            TrLog::Warn(
                "Scene list skipped part of '" +
                searchRoot.string() + "': " + error.what());
        }
    }

    const std::wstring currentPath = GetScenePath().empty()
        ? std::wstring()
        : NormalizeScenePath(GetScenePath());
    if(!currentPath.empty())
    {
        const auto current = std::find_if(
            mSceneSelectionEntries.begin(),
            mSceneSelectionEntries.end(),
            [&](const TrSceneSelectionEntry& entry)
            {
                return !entry.ScenePath.empty() &&
                    ScenePathsEqual(entry.ScenePath, currentPath);
            });
        if(current == mSceneSelectionEntries.end())
        {
            addScene(currentPath);
        }
    }

    std::sort(
        mSceneSelectionEntries.begin() + 1,
        mSceneSelectionEntries.end(),
        [](const TrSceneSelectionEntry& lhs, const TrSceneSelectionEntry& rhs)
        {
            const int nameOrder = _wcsicmp(
                lhs.DisplayName.c_str(), rhs.DisplayName.c_str());
            if(nameOrder != 0)
            {
                return nameOrder < 0;
            }
            return _wcsicmp(lhs.Detail.c_str(), rhs.Detail.c_str()) < 0;
        });

    mCurrentSceneSelectionIndex = 0;
    if(!currentPath.empty())
    {
        for(std::size_t index = 1; index < mSceneSelectionEntries.size(); ++index)
        {
            if(ScenePathsEqual(
                   mSceneSelectionEntries[index].ScenePath,
                   currentPath))
            {
                mCurrentSceneSelectionIndex = index;
                break;
            }
        }
    }

    TrLog::Info(
        "Scene selection list contains " +
        std::to_string(mSceneSelectionEntries.size()) + " entries.");
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
        mResourceHeap,
        TrRenderConfig::DepthClearValue);
    mTaaColorHistory.Initialize(
        mDevice.Get(),
        mWidth,
        mHeight,
        TrDeferredRenderTargets::HdrLightingFormat,
        mResourceHeap,
        L"TAA Color History");
    mTaaDepthHistory.Initialize(
        mDevice.Get(),
        mWidth,
        mHeight,
        DXGI_FORMAT_R32_FLOAT,
        mResourceHeap,
        L"TAA Depth History");
    mHierarchicalDepth.Initialize(
        mDevice.Get(),
        mWidth,
        mHeight,
        mResourceHeap);
    TrLog::Info(
        "HZB initialized: " + std::to_string(mWidth) + "x" +
        std::to_string(mHeight) + ", " +
        std::to_string(mHierarchicalDepth.GetDescription().MipCount) +
        " mips, closest-depth reduction.");
    mScreenProbeResources.Initialize(
        mDevice.Get(),
        mWidth,
        mHeight,
        mResourceHeap);
    const TrScreenProbeLayout& screenProbeLayout =
        mScreenProbeResources.GetLayout();
    TrLog::Info(
        "Lumen Screen Probes initialized: " +
        std::to_string(screenProbeLayout.ProbeCountX) + "x" +
        std::to_string(screenProbeLayout.ProbeCountY) + " probes, " +
        std::to_string(TrScreenProbeLayout::RaysPerProbe) +
        " rays per probe.");

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
    mPerformanceMonitor.Initialize(
        mDevice.Get(),
        mCommandQueue.Get(),
        SwapFrameCount);
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
        mTaaColorHistory.GetCurrentSrv().GpuHandle,
        TrDebugVisualization::HdrColor);
    mGpuDebug.RegisterView(
        L"Raw Lighting",
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
    mGpuDebug.RegisterView(
        L"Velocity",
        mDeferredRenderTargets.GetVelocitySrv().GpuHandle,
        TrDebugVisualization::MotionVectors);
    for(UINT mip = 0;
        mip < mHierarchicalDepth.GetDescription().MipCount;
        ++mip)
    {
        const std::wstring name = L"HZB Mip " + std::to_wstring(mip) +
            L" (" + std::to_wstring(mHierarchicalDepth.GetMipWidth(mip)) +
            L"x" + std::to_wstring(mHierarchicalDepth.GetMipHeight(mip)) +
            L")";
        mGpuDebug.RegisterView(
            name.c_str(),
            mHierarchicalDepth.GetMipSrv(mip).GpuHandle,
            TrDebugVisualization::DeviceDepth);
    }
    mGpuDebug.RegisterView(
        L"Lumen Screen Probe Normals",
        mScreenProbeResources.GetNormalDepthSrv().GpuHandle,
        TrDebugVisualization::WorldNormal);
    mGpuDebug.RegisterView(
        L"Lumen Screen Trace",
        mScreenProbeResources.GetTraceDebugSrv().GpuHandle,
        TrDebugVisualization::ScreenTrace);
    mGpuDebug.RegisterView(
        L"Lumen Probe Radiance",
        mScreenProbeResources.GetRadianceSrv().GpuHandle,
        TrDebugVisualization::HdrColor);
    mGpuDebug.RegisterView(
        L"Lumen Probe Irradiance",
        mScreenProbeResources.GetIrradianceSrv().GpuHandle,
        TrDebugVisualization::HdrColor);
    mProbeTemporalDebugViewIndex = mGpuDebug.GetViewCount();
    mGpuDebug.RegisterView(
        L"Lumen Probe Irradiance Temporal",
        mScreenProbeResources.GetIrradianceHistory()
            .GetCurrentSrv().GpuHandle,
        TrDebugVisualization::HdrColor);
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
    if(TrRenderConfig::IsDepthNormalPrepassEnabled)
    {
        mDepthNormalPass.Initialize(
            mDevice.Get(),
            GetAssetFullPath(SHADER_DIR L"Raster/depth_normal.hlsl"));
    }
    mGBufferPass.Initialize(
        mDevice.Get(),
        GetAssetFullPath(SHADER_DIR L"Raster/gbuffer.hlsl"));
    mHzbPass.Initialize(
        mDevice.Get(),
        GetAssetFullPath(SHADER_DIR L"Compute/hzb.hlsl"));
    mScreenProbePass.Initialize(
        mDevice.Get(),
        GetAssetFullPath(SHADER_DIR L"Lumen/screen_probe.hlsl"));
    mScreenTracePass.Initialize(
        mDevice.Get(),
        GetAssetFullPath(SHADER_DIR L"Lumen/screen_trace.hlsl"));
    mScreenProbeRadiancePass.Initialize(
        mDevice.Get(),
        GetAssetFullPath(SHADER_DIR L"Lumen/screen_probe_radiance.hlsl"));
    mScreenProbeIrradiancePass.Initialize(
        mDevice.Get(),
        GetAssetFullPath(SHADER_DIR L"Lumen/screen_probe_irradiance.hlsl"));
    mScreenProbeTemporalPass.Initialize(
        mDevice.Get(),
        GetAssetFullPath(SHADER_DIR L"Lumen/screen_probe_temporal.hlsl"));
    mTaaPass.Initialize(
        mDevice.Get(),
        GetAssetFullPath(SHADER_DIR L"Compute/taa.hlsl"));
    mDeferredLightingPass.Initialize(
        mDevice.Get(),
        GetAssetFullPath(SHADER_DIR L"Raster/deferred_lighting.hlsl"));
    mForwardTransparentPass.Initialize(
        mDevice.Get(),
        GetAssetFullPath(SHADER_DIR L"Raster/forward_transparent.hlsl"));
    mCompositePass.Initialize(
        mDevice.Get(),
        GetAssetFullPath(SHADER_DIR L"Raster/composite.hlsl"));

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
    mCameraBounds = runtimeBounds;
    if(mUsingImportedScene)
    {
        TrAxisAlignedBounds litGeometryBounds;
        std::size_t excludedUnlitInstances = 0;
        for(const TrRuntimeInstance& instance : mRuntimeScene.GetInstances())
        {
            const TrSceneMesh& mesh = mLoadedScene.Meshes[instance.MeshId];
            bool hasLitOrDefaultMaterial = false;
            for(const TrScenePrimitive& primitive : mesh.Primitives)
            {
                if(primitive.MaterialIndex == TrInvalidSceneIndex ||
                   !mLoadedScene.Materials[primitive.MaterialIndex].Unlit)
                {
                    hasLitOrDefaultMaterial = true;
                    break;
                }
            }

            if(hasLitOrDefaultMaterial)
            {
                litGeometryBounds.Expand(instance.CurrentWorldBounds);
            }
            else
            {
                ++excludedUnlitInstances;
            }
        }

        if(litGeometryBounds.IsValid() && litGeometryBounds.GetRadius() > 0.001f)
        {
            mCameraBounds = litGeometryBounds;
            const DirectX::XMFLOAT3 framingMinimum = mCameraBounds.Minimum;
            const DirectX::XMFLOAT3 framingMaximum = mCameraBounds.Maximum;
            TrLog::Info(
                "Imported-scene camera framing excluded " +
                std::to_string(excludedUnlitInstances) +
                " unlit-only background instances. Framing AABB min (" +
                std::to_string(framingMinimum.x) + ", " +
                std::to_string(framingMinimum.y) + ", " +
                std::to_string(framingMinimum.z) + "), max (" +
                std::to_string(framingMaximum.x) + ", " +
                std::to_string(framingMaximum.y) + ", " +
                std::to_string(framingMaximum.z) + ").");
        }
    }
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
    std::size_t opaqueDrawCount = 0;
    std::size_t alphaMaskDrawCount = 0;
    std::size_t alphaBlendDrawCount = 0;
    std::size_t prepassDrawCount = 0;
    for(const TrRuntimeInstance& instance : mRuntimeScene.GetInstances())
    {
        const TrRuntimeMesh& mesh = mRuntimeScene.GetMesh(instance.MeshId);
        for(const TrRuntimePrimitive& primitive : mesh.Primitives)
        {
            const TrSceneAlphaMode alphaMode =
                mMaterialResources.Get(primitive.MaterialId).AlphaMode;
            if(ShouldRenderInDepthNormalPrepass(alphaMode))
            {
                ++prepassDrawCount;
            }
            switch(alphaMode)
            {
            case TrSceneAlphaMode::Mask:
                ++alphaMaskDrawCount;
                break;
            case TrSceneAlphaMode::Blend:
                ++alphaBlendDrawCount;
                break;
            default:
                ++opaqueDrawCount;
                break;
            }
        }
    }
    TrLog::Info(
        "Material draw classification: " +
        std::to_string(opaqueDrawCount) + " opaque, " +
        std::to_string(alphaMaskDrawCount) + " alpha mask, " +
        std::to_string(alphaBlendDrawCount) + " alpha blend.");
    TrLog::Info(
        "Depth/Normal prepass selected " +
        std::to_string(prepassDrawCount) + " primitive draws.");
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
        frame.ForwardTransparentPassConstantBuffer.Initialize(
            mDevice.Get(),
            static_cast<UINT>(sizeof(TrForwardTransparentPassConstants)));
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
    mPerformanceMonitor.BeginFrame(mFrameIndex, mCommandList.Get());

    // need to set viewports and scissor rects each frame
    mCommandList->RSSetViewports(1, &mViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    struct SceneDraw
    {
        std::size_t InstanceIndex = 0;
        std::size_t PrimitiveIndex = 0;
        float CameraDistanceSquared = 0.0f;
    };
    std::vector<SceneDraw> prepassedDraws;
    std::vector<SceneDraw> regularGBufferDraws;
    std::vector<SceneDraw> transparentDraws;
    prepassedDraws.reserve(mRuntimeScene.GetDrawCount());
    regularGBufferDraws.reserve(mRuntimeScene.GetDrawCount());
    transparentDraws.reserve(mRuntimeScene.GetDrawCount());

    const std::vector<TrRuntimeInstance>& instances = mRuntimeScene.GetInstances();
    for(std::size_t instanceIndex = 0; instanceIndex < instances.size(); ++instanceIndex)
    {
        const TrRuntimeInstance& instance = instances[instanceIndex];
        const TrRuntimeMesh& mesh = mRuntimeScene.GetMesh(instance.MeshId);
        for(std::size_t primitiveIndex = 0;
            primitiveIndex < mesh.Primitives.size();
            ++primitiveIndex)
        {
            const TrRuntimePrimitive& primitive = mesh.Primitives[primitiveIndex];
            const TrMaterialGpuBinding& material =
                mMaterialResources.Get(primitive.MaterialId);
            if(material.AlphaMode == TrSceneAlphaMode::Blend)
            {
                const DirectX::XMFLOAT3 localCenter =
                    primitive.LocalBounds.GetCenter();
                const DirectX::XMVECTOR worldCenter =
                    DirectX::XMVector3TransformCoord(
                        DirectX::XMLoadFloat3(&localCenter),
                        DirectX::XMLoadFloat4x4(
                            &instance.CurrentWorldTransform));
                const DirectX::XMVECTOR cameraPosition =
                    DirectX::XMLoadFloat3(&mCameraPosition);
                transparentDraws.push_back(
                {
                    instanceIndex,
                    primitiveIndex,
                    DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(
                        DirectX::XMVectorSubtract(
                            worldCenter,
                            cameraPosition)))
                });
                continue;
            }

            const SceneDraw draw = {instanceIndex, primitiveIndex, 0.0f};
            if(ShouldRenderInDepthNormalPrepass(material.AlphaMode))
            {
                prepassedDraws.push_back(draw);
            }
            else
            {
                regularGBufferDraws.push_back(draw);
            }
        }
    }

    if(!prepassedDraws.empty())
    {
        mDepthNormalPass.ExecuteProfiled(
            mPerformanceMonitor,
            mCommandList.Get(),
            [&]()
            {
                mDepthNormalPass.Begin(
                    mCommandList.Get(),
                    mDeferredRenderTargets,
                    mResourceHeap,
                    mSamplerHeap,
                    frame.ViewConstantBuffer.GetGpuVirtualAddress());
                for(const SceneDraw& draw : prepassedDraws)
                {
                    const TrRuntimeInstance& instance =
                        instances[draw.InstanceIndex];
                    const TrRuntimeMesh& mesh =
                        mRuntimeScene.GetMesh(instance.MeshId);
                    const TrRuntimePrimitive& primitive =
                        mesh.Primitives[draw.PrimitiveIndex];
                    const TrMaterialGpuBinding& material =
                        mMaterialResources.Get(primitive.MaterialId);
                    mesh.Geometry->Bind(mCommandList.Get());

                    TrDrawConstants drawConstants = {};
                    drawConstants.PrimitiveId = primitive.PrimitiveId;
                    drawConstants.MaterialId = primitive.MaterialId;
                    drawConstants.LocalPrimitiveIndex =
                        primitive.LocalPrimitiveIndex;
                    mDepthNormalPass.SetDrawBindings(
                        mCommandList.Get(),
                        frame.PrimitiveConstantBuffers[draw.InstanceIndex]
                            ->GetGpuVirtualAddress(),
                        material.Constants,
                        material.TextureTable,
                        material.SamplerTable,
                        drawConstants);
                    mesh.Geometry->DrawRange(
                        mCommandList.Get(),
                        primitive.IndexCount,
                        primitive.FirstIndex);
                }
                mDepthNormalPass.End(
                    mCommandList.Get(),
                    mDeferredRenderTargets);
            });
    }

    mGBufferPass.ExecuteProfiled(
        mPerformanceMonitor,
        mCommandList.Get(),
        [&]()
        {
            mGBufferPass.Begin(
                mCommandList.Get(),
                mDeferredRenderTargets,
                mResourceHeap,
                mSamplerHeap,
                frame.ViewConstantBuffer.GetGpuVirtualAddress(),
                frame.GBufferPassConstantBuffer.GetGpuVirtualAddress(),
                !prepassedDraws.empty());

            const auto drawGBufferItems =
                [this, &frame, &instances](const std::vector<SceneDraw>& draws)
            {
                for(const SceneDraw& draw : draws)
                {
                    const TrRuntimeInstance& instance =
                        instances[draw.InstanceIndex];
                    const TrRuntimeMesh& mesh =
                        mRuntimeScene.GetMesh(instance.MeshId);
                    const TrRuntimePrimitive& primitive =
                        mesh.Primitives[draw.PrimitiveIndex];
                    const TrMaterialGpuBinding& material =
                        mMaterialResources.Get(primitive.MaterialId);
                    mesh.Geometry->Bind(mCommandList.Get());

                    TrDrawConstants drawConstants = {};
                    drawConstants.PrimitiveId = primitive.PrimitiveId;
                    drawConstants.MaterialId = primitive.MaterialId;
                    drawConstants.LocalPrimitiveIndex =
                        primitive.LocalPrimitiveIndex;
                    mGBufferPass.SetDrawBindings(
                        mCommandList.Get(),
                        frame.PrimitiveConstantBuffers[draw.InstanceIndex]
                            ->GetGpuVirtualAddress(),
                        material.Constants,
                        material.TextureTable,
                        material.SamplerTable,
                        drawConstants);
                    mesh.Geometry->DrawRange(
                        mCommandList.Get(),
                        primitive.IndexCount,
                        primitive.FirstIndex);
                }
            };

            drawGBufferItems(regularGBufferDraws);
            if(!prepassedDraws.empty())
            {
                mGBufferPass.BeginPrepassedDraws(
                    mCommandList.Get(),
                    frame.ViewConstantBuffer.GetGpuVirtualAddress(),
                    frame.GBufferPassConstantBuffer.GetGpuVirtualAddress());
                drawGBufferItems(prepassedDraws);
            }
            mDepthNormalView = mGBufferPass.End(
                mCommandList.Get(),
                mDeferredRenderTargets);
        });

    mHzbPass.ExecuteProfiled(
        mPerformanceMonitor,
        mCommandList.Get(),
        [&]()
        {
            mHzbPass.Build(
                mCommandList.Get(),
                mResourceHeap,
                {mDepthNormalView},
                mHierarchicalDepth);
        });

    TrTexture* probeIrradiance = &mScreenProbeResources.GetIrradiance();
    D3D12_GPU_DESCRIPTOR_HANDLE probeIrradianceSrv =
        mScreenProbeResources.GetIrradianceSrv().GpuHandle;
    const UINT renderFrameNumber = mFrameNumber > 0 ? mFrameNumber - 1u : 0u;
    const bool indirectLightingEnabled = mPipelineFeatures.IsEnabled(
        TrPipelineFeature::IndirectLighting);
    if(indirectLightingEnabled)
    {
        const TrScreenProbePass::Outputs screenProbeOutputs =
            mScreenProbePass.ExecuteProfiled(
                mPerformanceMonitor,
                mCommandList.Get(),
                [&]()
                {
                    return mScreenProbePass.Build(
                        mCommandList.Get(),
                        mResourceHeap,
                        frame.ViewConstantBuffer.GetGpuVirtualAddress(),
                        {mDepthNormalView},
                        mScreenProbeResources);
                });
        mScreenTracePass.ExecuteProfiled(
            mPerformanceMonitor,
            mCommandList.Get(),
            [&]()
            {
                mScreenTracePass.Trace(
                    mCommandList.Get(),
                    mResourceHeap,
                    frame.ViewConstantBuffer.GetGpuVirtualAddress(),
                    {&mHierarchicalDepth, screenProbeOutputs.ScreenProbes},
                    mScreenProbeResources);
            });

        mScreenProbeRadiancePass.ExecuteProfiled(
            mPerformanceMonitor,
            mCommandList.Get(),
            [&]()
            {
                mScreenProbeRadiancePass.Resolve(
                    mCommandList.Get(),
                    mResourceHeap,
                    frame.SceneConstantBuffer.GetGpuVirtualAddress(),
                    frame.LightingPassConstantBuffer.GetGpuVirtualAddress(),
                    mDeferredRenderTargets,
                    mScreenProbeResources);
            });
        mScreenProbeIrradiancePass.ExecuteProfiled(
            mPerformanceMonitor,
            mCommandList.Get(),
            [&]()
            {
                mScreenProbeIrradiancePass.Integrate(
                    mCommandList.Get(),
                    mResourceHeap,
                    mScreenProbeResources);
            });
        const TrScreenProbeTemporalPass::Outputs probeTemporalOutputs =
            mScreenProbeTemporalPass.ExecuteProfiled(
                mPerformanceMonitor,
                mCommandList.Get(),
                [&]()
                {
                    return mScreenProbeTemporalPass.Resolve(
                        mCommandList.Get(),
                        mResourceHeap,
                        frame.ViewConstantBuffer.GetGpuVirtualAddress(),
                        renderFrameNumber,
                        mScreenProbeResources);
                });
        probeIrradiance = probeTemporalOutputs.Irradiance;
        probeIrradianceSrv = probeTemporalOutputs.IrradianceSrv;
        mGpuDebug.UpdateViewSource(
            mProbeTemporalDebugViewIndex,
            probeIrradianceSrv);
        mScreenProbeResources.AdvanceHistory();
    }

    mDeferredLightingPass.ExecuteProfiled(
        mPerformanceMonitor,
        mCommandList.Get(),
        [&]()
        {
            mDeferredLightingPass.Render(
                mCommandList.Get(),
                mDeferredRenderTargets,
                mResourceHeap,
                frame.SceneConstantBuffer.GetGpuVirtualAddress(),
                frame.ViewConstantBuffer.GetGpuVirtualAddress(),
                frame.LightingPassConstantBuffer.GetGpuVirtualAddress(),
                mScreenProbeResources,
                *probeIrradiance,
                probeIrradianceSrv);
        });

    if(!transparentDraws.empty())
    {
        mForwardTransparentPass.ExecuteProfiled(
            mPerformanceMonitor,
            mCommandList.Get(),
            [&]()
            {
                std::sort(
                    transparentDraws.begin(),
                    transparentDraws.end(),
                    [](const SceneDraw& left, const SceneDraw& right)
                    {
                        return left.CameraDistanceSquared >
                            right.CameraDistanceSquared;
                    });
                mForwardTransparentPass.Begin(
                    mCommandList.Get(),
                    mDeferredRenderTargets,
                    mResourceHeap,
                    mSamplerHeap,
                    frame.SceneConstantBuffer.GetGpuVirtualAddress(),
                    frame.ViewConstantBuffer.GetGpuVirtualAddress(),
                    frame.ForwardTransparentPassConstantBuffer
                        .GetGpuVirtualAddress());
                for(const SceneDraw& draw : transparentDraws)
                {
                    const TrRuntimeInstance& instance =
                        instances[draw.InstanceIndex];
                    const TrRuntimeMesh& mesh =
                        mRuntimeScene.GetMesh(instance.MeshId);
                    const TrRuntimePrimitive& primitive =
                        mesh.Primitives[draw.PrimitiveIndex];
                    const TrMaterialGpuBinding& material =
                        mMaterialResources.Get(primitive.MaterialId);
                    mesh.Geometry->Bind(mCommandList.Get());

                    TrDrawConstants drawConstants = {};
                    drawConstants.PrimitiveId = primitive.PrimitiveId;
                    drawConstants.MaterialId = primitive.MaterialId;
                    drawConstants.LocalPrimitiveIndex =
                        primitive.LocalPrimitiveIndex;
                    mForwardTransparentPass.SetDrawBindings(
                        mCommandList.Get(),
                        frame.PrimitiveConstantBuffers[draw.InstanceIndex]
                            ->GetGpuVirtualAddress(),
                        material.Constants,
                        material.TextureTable,
                        material.SamplerTable,
                        drawConstants);
                    mesh.Geometry->DrawRange(
                        mCommandList.Get(),
                        primitive.IndexCount,
                        primitive.FirstIndex);
                }
                mForwardTransparentPass.End(
                    mCommandList.Get(),
                    mDeferredRenderTargets);
            });
    }

    TrTaaConstants taaConstants;
    taaConstants.Width = mWidth;
    taaConstants.Height = mHeight;
    taaConstants.HistoryValid =
        mTaaColorHistory.IsValid() && mTaaDepthHistory.IsValid() ? 1u : 0u;
    taaConstants.FrameNumber = renderFrameNumber;
    taaConstants.CurrentJitterNdc = mTemporalJitter;
    taaConstants.PreviousJitterNdc = mPreviousTemporalJitter;
    taaConstants.NearPlane = mCameraNearPlane;
    taaConstants.FarPlane = mCameraFarPlane;
    const D3D12_GPU_DESCRIPTOR_HANDLE taaOutputSrv =
        mTaaPass.ExecuteProfiled(
            mPerformanceMonitor,
            mCommandList.Get(),
            [&]()
            {
                return mTaaPass.Resolve(
                    mCommandList.Get(),
                    mResourceHeap,
                    {
                        &mDeferredRenderTargets.GetHdrLighting(),
                        mDeferredRenderTargets.GetHdrLightingSrv().GpuHandle,
                        &mDeferredRenderTargets.GetVelocity(),
                        mDeferredRenderTargets.GetVelocitySrv().GpuHandle,
                        &mDeferredRenderTargets.GetDepth(),
                        mDeferredRenderTargets.GetDepthSrv().GpuHandle
                    },
                    taaConstants,
                    mTaaColorHistory,
                    mTaaDepthHistory);
            });
    mTaaColorHistory.AdvanceFrame();
    mTaaDepthHistory.AdvanceFrame();

    const TrGpuDebugView& selectedDebugView = mGpuDebug.GetSelectedView();
    const D3D12_GPU_DESCRIPTOR_HANDLE compositeSource =
        mGpuDebug.GetSelectedIndex() == 0u
            ? taaOutputSrv
            : selectedDebugView.SourceSrv;

    mCompositePass.ExecuteProfiled(
        mPerformanceMonitor,
        mCommandList.Get(),
        [&]()
        {
            mCompositePass.Render(
                mCommandList.Get(),
                mResourceHeap,
                compositeSource,
                frame.CompositePassConstantBuffer.GetGpuVirtualAddress(),
                mRenderTargets[mFrameIndex],
                mRenderTargetViews[mFrameIndex].CpuHandle);
        });

    mGpuDebugPanel.Render(mCommandList.Get(), mResourceHeap);
    mRenderTargets[mFrameIndex].Transition(
        mCommandList.Get(),
        D3D12_RESOURCE_STATE_PRESENT);

    mPerformanceMonitor.EndFrame(mCommandList.Get());
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
    mPerformanceMonitor.CollectFrame(mFrameIndex);
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
