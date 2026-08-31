#pragma once

#include "TrD3D12DescriptorHeap.h"
#include "TrD3D12Texture.h"

class TrD3D12DeferredRenderTargets
{
public:
    static constexpr UINT RtvDescriptorCount = 3;
    static constexpr UINT DsvDescriptorCount = 1;
    static constexpr UINT ShaderResourceDescriptorCount = 4;

    static constexpr DXGI_FORMAT BaseColorRoughnessFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    static constexpr DXGI_FORMAT NormalMetallicFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
    static constexpr DXGI_FORMAT DepthResourceFormat = DXGI_FORMAT_R32_TYPELESS;
    static constexpr DXGI_FORMAT DepthViewFormat = DXGI_FORMAT_D32_FLOAT;
    static constexpr DXGI_FORMAT DepthSrvFormat = DXGI_FORMAT_R32_FLOAT;
    static constexpr DXGI_FORMAT HdrLightingFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

    void Initialize(
        ID3D12Device* device,
        UINT width,
        UINT height,
        TrD3D12DescriptorHeap& rtvHeap,
        TrD3D12DescriptorHeap& dsvHeap,
        TrD3D12DescriptorHeap& resourceHeap);

    void BeginGBufferPass(ID3D12GraphicsCommandList* commandList);
    void EndGBufferPass(ID3D12GraphicsCommandList* commandList);
    void BeginDeferredLightingPass(ID3D12GraphicsCommandList* commandList);
    void EndDeferredLightingPass(ID3D12GraphicsCommandList* commandList);

    TrD3D12Texture& GetBaseColorRoughness() { return mBaseColorRoughness; }
    const TrD3D12Texture& GetBaseColorRoughness() const { return mBaseColorRoughness; }
    const TrD3D12Texture& GetNormalMetallic() const { return mNormalMetallic; }
    const TrD3D12Texture& GetDepth() const { return mDepth; }
    const TrD3D12Texture& GetHdrLighting() const { return mHdrLighting; }

    const TrD3D12DescriptorAllocation& GetBaseColorSrv() const { return mBaseColorSrv; }
    const TrD3D12DescriptorAllocation& GetNormalSrv() const { return mNormalSrv; }
    const TrD3D12DescriptorAllocation& GetDepthSrv() const { return mDepthSrv; }
    const TrD3D12DescriptorAllocation& GetHdrLightingSrv() const { return mHdrLightingSrv; }

private:
    TrD3D12Texture mBaseColorRoughness;
    TrD3D12Texture mNormalMetallic;
    TrD3D12Texture mDepth;
    TrD3D12Texture mHdrLighting;

    TrD3D12DescriptorAllocation mBaseColorRtv;
    TrD3D12DescriptorAllocation mNormalRtv;
    TrD3D12DescriptorAllocation mHdrLightingRtv;
    TrD3D12DescriptorAllocation mDepthDsv;

    TrD3D12DescriptorAllocation mBaseColorSrv;
    TrD3D12DescriptorAllocation mNormalSrv;
    TrD3D12DescriptorAllocation mDepthSrv;
    TrD3D12DescriptorAllocation mHdrLightingSrv;
};
