/* note:
 * DXGI: DirectX Graphics Infrastructure
 * IID_PPV_ARGS: ComPtr -> RIID + void**
 */

# pragma once
#include "TrD3D12Util.h"
#include "TrD3D12RendererBase.h"
#include "TrD3D12CompositePass.h"
#include "TrD3D12ConstantBuffer.h"
#include "TrD3D12DeferredLightingPass.h"
#include "TrD3D12DeferredRenderTargets.h"
#include "TrD3D12DescriptorHeap.h"
#include "TrD3D12GBufferPass.h"
#include "TrD3D12Mesh.h"
#include "TrD3D12RenderConstants.h"

class TrWindowApp;

class TrD3D12RendererRaster : public TrD3D12RendererBase
{
public:
    TrD3D12RendererRaster(UINT width, UINT height, std::wstring title);
    void OnInitialize() override;
    void OnUpdate() override;
    void OnRender() override;
    void OnDestroy() override;

    void OnKeyDown(UINT8 wParam) override;
    void OnKeyUp(UINT8 wParam) override;

private:
    void LoadPipeline();
    void LoadAssetsCornellBox();

    // populate: add datas to...
    void PopulateCommandList();

    // Mark the submitted frame, switch to the next back buffer, and wait only
    // when that frame's resources are still in use by the GPU.
    void MoveToNextFrame();

    // Wait for all work currently submitted to the direct queue. This is used
    // for shutdown or an explicit full-queue synchronization, never per frame.
    void FlushCommandQueue();
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

    struct FrameContext
    {
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandAllocator;
        TrD3D12ConstantBuffer SceneConstantBuffer;
        TrD3D12ConstantBuffer ViewConstantBuffer;
        TrD3D12ConstantBuffer GBufferPassConstantBuffer;
        TrD3D12ConstantBuffer PrimitiveConstantBuffer;
        TrD3D12ConstantBuffer MaterialConstantBuffer;
        TrD3D12ConstantBuffer LightingPassConstantBuffer;
        TrD3D12ConstantBuffer CompositePassConstantBuffer;
        UINT64 FenceValue = 0;
    };

    // pipeline objects
    CD3DX12_VIEWPORT mViewport;
    CD3DX12_RECT mScissorRect;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> mSwapChain;
    Microsoft::WRL::ComPtr<ID3D12Device> mDevice;
    FrameContext mFrameContexts[SwapFrameCount];
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;
    TrD3D12GBufferPass mGBufferPass;
    TrD3D12DeferredLightingPass mDeferredLightingPass;
    TrD3D12CompositePass mCompositePass;
    
    TrD3D12Texture mRenderTargets[SwapFrameCount];
    TrD3D12DescriptorAllocation mRenderTargetViews[SwapFrameCount];
    TrD3D12DescriptorHeap mRtvHeap;
    TrD3D12DescriptorHeap mDsvHeap;
    TrD3D12DescriptorHeap mResourceHeap;
    TrD3D12DeferredRenderTargets mDeferredRenderTargets;

    // app resources
    TrD3D12Mesh mSceneMesh;

    // synchronization objects
    UINT64 mNextFenceValue = 1;
    Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
    HANDLE mFenceEvent = nullptr;  // handle to object fence

    UINT mFrameIndex;
    UINT mFrameNumber = 0;
    DirectX::XMFLOAT4X4 mPreviousViewProjection;
};
