#pragma once

#include "TrDescriptorHeap.h"
#include "TrTexture.h"

class TrDeferredRenderTargets
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
        TrDescriptorHeap& rtvHeap,
        TrDescriptorHeap& dsvHeap,
        TrDescriptorHeap& resourceHeap);

    void BeginGBufferPass(ID3D12GraphicsCommandList* commandList);
    void EndGBufferPass(ID3D12GraphicsCommandList* commandList);
    void BeginDeferredLightingPass(ID3D12GraphicsCommandList* commandList);
    void EndDeferredLightingPass(ID3D12GraphicsCommandList* commandList);

    TrTexture& GetBaseColorRoughness() { return mBaseColorRoughness; }
    const TrTexture& GetBaseColorRoughness() const { return mBaseColorRoughness; }
    const TrTexture& GetNormalMetallic() const { return mNormalMetallic; }
    const TrTexture& GetDepth() const { return mDepth; }
    const TrTexture& GetHdrLighting() const { return mHdrLighting; }

    const TrDescriptorAllocation& GetBaseColorSrv() const { return mBaseColorSrv; }
    const TrDescriptorAllocation& GetNormalSrv() const { return mNormalSrv; }
    const TrDescriptorAllocation& GetDepthSrv() const { return mDepthSrv; }
    const TrDescriptorAllocation& GetHdrLightingSrv() const { return mHdrLightingSrv; }

private:
    TrTexture mBaseColorRoughness;
    TrTexture mNormalMetallic;
    TrTexture mDepth;
    TrTexture mHdrLighting;

    TrDescriptorAllocation mBaseColorRtv;
    TrDescriptorAllocation mNormalRtv;
    TrDescriptorAllocation mHdrLightingRtv;
    TrDescriptorAllocation mDepthDsv;

    TrDescriptorAllocation mBaseColorSrv;
    TrDescriptorAllocation mNormalSrv;
    TrDescriptorAllocation mDepthSrv;
    TrDescriptorAllocation mHdrLightingSrv;
};
