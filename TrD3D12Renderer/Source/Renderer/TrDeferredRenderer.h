/* note:
 * DXGI: DirectX Graphics Infrastructure
 * IID_PPV_ARGS: ComPtr -> RIID + void**
 */

# pragma once
#include "Utilities/TrUtil.h"
#include "App/TrRendererBase.h"
#include "Passes/Raster/TrCompositePass.h"
#include "Passes/Compute/TrHzbPass.h"
#include "Lumen/TrScreenProbePass.h"
#include "Lumen/TrScreenProbeIrradiancePass.h"
#include "Lumen/TrScreenProbeRadiancePass.h"
#include "Lumen/TrScreenProbeResources.h"
#include "Lumen/TrScreenTracePass.h"
#include "Resources/TrConstantBuffer.h"
#include "Passes/Raster/TrDeferredLightingPass.h"
#include "Passes/Raster/TrDepthNormalPass.h"
#include "Passes/Raster/TrForwardTransparentPass.h"
#include "Resources/TrDeferredRenderTargets.h"
#include "Resources/TrDescriptorHeap.h"
#include "Passes/Raster/TrGBufferPass.h"
#include "Debug/TrGpuDebug.h"
#include "Debug/TrGpuDebugPanel.h"
#include "Resources/TrHistoryTexture.h"
#include "Resources/TrMaterialResources.h"
#include "TrRenderConstants.h"
#include "Scene/TrRuntimeScene.h"
#include "TrScene.h"

#include <memory>
#include <vector>

class TrWindowApp;

class TrDeferredRenderer : public TrRendererBase
{
public:
    TrDeferredRenderer(UINT width, UINT height, std::wstring title);
    void OnInitialize() override;
    void OnUpdate() override;
    void OnRender() override;
    void OnResize(UINT width, UINT height) override;
    void OnDestroy() override;

    void OnKeyDown(UINT8 wParam) override;
    void OnKeyUp(UINT8 wParam) override;

private:
    void LoadPipeline();
    void LoadAssets();
    void CreateBackBufferResources();
    void RegisterGpuDebugViews();
    void UpdateWindowTitle() const;

    // populate: add datas to...
    void PopulateCommandList();

    // Mark the submitted frame, switch to the next back buffer, and wait only
    // when that frame's resources are still in use by the GPU.
    void MoveToNextFrame();

    // Wait for all work currently submitted to the direct queue. This is used
    // for shutdown or an explicit full-queue synchronization, never per frame.
    void FlushCommandQueue();
    void ValidateShaderModelSupport() const;
    void ValidateDebugLayer();

    // device is singleton to adapter
    static void GetHardwareAdapter(IDXGIFactory4* pFactory, REFIID riid, void** ppAdapter);

private:
    /* note:
     * const variable can not be modified after initialization. The value is determined at runtime
     * constexpr variable's value is evaluated at compile time. It must be initialized with a constant expression and a value.
     */
    static constexpr UINT SwapFrameCount = 2;
    static constexpr UINT ResourceDescriptorCount = 8192;
    static constexpr UINT SamplerDescriptorCount = 2048;

    struct TrFrameContext
    {
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandAllocator;
        TrConstantBuffer SceneConstantBuffer;
        TrConstantBuffer ViewConstantBuffer;
        TrConstantBuffer GBufferPassConstantBuffer;
        std::vector<std::unique_ptr<TrConstantBuffer>> PrimitiveConstantBuffers;
        TrConstantBuffer LightingPassConstantBuffer;
        TrConstantBuffer ForwardTransparentPassConstantBuffer;
        TrConstantBuffer CompositePassConstantBuffer;
        UINT64 FenceValue = 0;
    };

    // pipeline objects
    CD3DX12_VIEWPORT mViewport;
    CD3DX12_RECT mScissorRect;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> mSwapChain;
    Microsoft::WRL::ComPtr<ID3D12Device> mDevice;
    TrFrameContext mFrameContexts[SwapFrameCount];
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;
    TrDepthNormalPass mDepthNormalPass;
    TrGBufferPass mGBufferPass;
    TrDeferredLightingPass mDeferredLightingPass;
    TrForwardTransparentPass mForwardTransparentPass;
    TrCompositePass mCompositePass;
    
    TrTexture mRenderTargets[SwapFrameCount];
    TrDescriptorAllocation mRenderTargetViews[SwapFrameCount];
    TrDescriptorHeap mRtvHeap;
    TrDescriptorHeap mDsvHeap;
    TrDescriptorHeap mResourceHeap;
    TrDescriptorHeap mSamplerHeap;
    TrDeferredRenderTargets mDeferredRenderTargets;
    TrDepthNormalView mDepthNormalView;
    TrHierarchicalDepth mHierarchicalDepth;
    TrHzbPass mHzbPass;
    TrScreenProbeResources mScreenProbeResources;
    TrScreenProbePass mScreenProbePass;
    TrScreenTracePass mScreenTracePass;
    TrScreenProbeRadiancePass mScreenProbeRadiancePass;
    TrScreenProbeIrradiancePass mScreenProbeIrradiancePass;
    TrHistoryTexture mLightingHistory;
    TrGpuDebug mGpuDebug;
    TrGpuDebugPanel mGpuDebugPanel;

    // app resources
    TrScene mLoadedScene;
    TrRuntimeScene mRuntimeScene;
    TrMaterialResources mMaterialResources;
    bool mUsingImportedScene = false;
    TrNodeId mProceduralAnimationNodeId = TrInvalidRuntimeId;
    TrGeometryVisualization mGeometryVisualization = TrGeometryVisualization::Shaded;

    // synchronization objects
    UINT64 mNextFenceValue = 1;
    Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
    HANDLE mFenceEvent = nullptr;  // handle to object fence

    UINT mFrameIndex;
    UINT mFrameNumber = 0;
    DirectX::XMFLOAT3 mCameraPosition = {0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT4X4 mPreviousViewProjection;
    float mExposure = 1.0f;
    float mDepthVisualizationRange = 10.0f;
    bool mInitialized = false;
};
