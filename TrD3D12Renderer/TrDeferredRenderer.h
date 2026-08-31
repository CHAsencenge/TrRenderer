/* note:
 * DXGI: DirectX Graphics Infrastructure
 * IID_PPV_ARGS: ComPtr -> RIID + void**
 */

# pragma once
#include "TrUtil.h"
#include "TrRendererBase.h"
#include "TrCompositePass.h"
#include "TrConstantBuffer.h"
#include "TrDeferredLightingPass.h"
#include "TrDeferredRenderTargets.h"
#include "TrDescriptorHeap.h"
#include "TrGBufferPass.h"
#include "TrHistoryTexture.h"
#include "TrMesh.h"
#include "TrRenderConstants.h"

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
    void LoadAssetsCornellBox();
    void CreateBackBufferResources();

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
    static constexpr UINT ResourceDescriptorCount = 64;

    struct TrFrameContext
    {
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandAllocator;
        TrConstantBuffer SceneConstantBuffer;
        TrConstantBuffer ViewConstantBuffer;
        TrConstantBuffer GBufferPassConstantBuffer;
        TrConstantBuffer PrimitiveConstantBuffer;
        TrConstantBuffer MaterialConstantBuffer;
        TrConstantBuffer LightingPassConstantBuffer;
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
    TrGBufferPass mGBufferPass;
    TrDeferredLightingPass mDeferredLightingPass;
    TrCompositePass mCompositePass;
    
    TrTexture mRenderTargets[SwapFrameCount];
    TrDescriptorAllocation mRenderTargetViews[SwapFrameCount];
    TrDescriptorHeap mRtvHeap;
    TrDescriptorHeap mDsvHeap;
    TrDescriptorHeap mResourceHeap;
    TrDeferredRenderTargets mDeferredRenderTargets;
    TrHistoryTexture mLightingHistory;

    // app resources
    TrMesh mSceneMesh;

    // synchronization objects
    UINT64 mNextFenceValue = 1;
    Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
    HANDLE mFenceEvent = nullptr;  // handle to object fence

    UINT mFrameIndex;
    UINT mFrameNumber = 0;
    DirectX::XMFLOAT4X4 mPreviousViewProjection;
    bool mInitialized = false;
};
