/* note:
 * DXGI: DirectX Graphics Infrastructure
 * IID_PPV_ARGS: ComPtr -> RIID + void**
 */

# pragma once
#include "TrD3D12Util.h"
#include "TrD3D12RendererBase.h"

class TrWindowApp;

class TrD3D12RendererRaster : public TrD3D12RendererBase
{
public:

    TrD3D12RendererRaster(UINT width, UINT height, std::wstring title);
    /*TrD3D12RendererRaster(const TrD3D12RendererRaster& other) = delete;
    TrD3D12RendererRaster(const TrD3D12RendererRaster&& other) = delete;
    TrD3D12RendererRaster& operator=(const TrD3D12RendererRaster& other) = delete;
    TrD3D12RendererRaster& operator=(const TrD3D12RendererRaster&& other) = delete;*/
   
public:
    virtual void OnInitialize() override;
    virtual void OnUpdate() override;
    virtual void OnRender() override;
    virtual void OnDestroy() override;

    virtual void OnKeyDown(UINT8 wParam) override;
    virtual void OnKeyUp(UINT8 wParam) override;


private:

    virtual void LoadPipeline();
    virtual void LoadAssets(const std::wstring filename);

    // populate: add datas to...
    virtual void PopulateCommandList();

    virtual void WaitForPreviousFrame();

    // device is singleton to adapter
    static void GetHardwareAdapter(IDXGIFactory4* pFactory, REFIID riid, void** ppAdapter);

private:
    /* note:
     * const variable can not be modified after initialization. The value is determined at runtime
     * constexpr variable's value is evaluated at compile time. It must be initialized with a constant expression and a value.
     */
    static constexpr UINT SwapFrameCount = 2;

    // pipeline objects
    CD3DX12_VIEWPORT mViewport;
    CD3DX12_RECT mScissorRect;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> mSwapChain;
    Microsoft::WRL::ComPtr<ID3D12Device> mDevice;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mCommandAllocator;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mPipelineState;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;
    
    Microsoft::WRL::ComPtr<ID3D12Resource> mRenderTargets[SwapFrameCount];
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;
    UINT mRtvDescriptorSize;

    // app resources
    Microsoft::WRL::ComPtr<ID3D12Resource> mVertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;

    // synchronization objects
    UINT64 mFenceValue;
    Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
    HANDLE mFenceEvent;  // handle to object fence

    UINT mFrameIndex;


    
};
