#pragma once

#include "Resources/TrDescriptorHeap.h"
#include "Resources/TrDepthNormalView.h"
#include "Resources/TrTexture.h"

class TrDeferredRenderTargets
{
public:
    static constexpr UINT RtvDescriptorCount = 4;
    static constexpr UINT DsvDescriptorCount = 2;
    static constexpr UINT ShaderResourceDescriptorCount = 6;

    static constexpr DXGI_FORMAT BaseColorRoughnessFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    static constexpr DXGI_FORMAT NormalMetallicFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
    static constexpr DXGI_FORMAT EmissiveOcclusionFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
    static constexpr DXGI_FORMAT DepthResourceFormat = DXGI_FORMAT_R32G8X24_TYPELESS;
    static constexpr DXGI_FORMAT DepthStencilViewFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    static constexpr DXGI_FORMAT DepthSrvFormat = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
    static constexpr DXGI_FORMAT StencilSrvFormat = DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
    static constexpr DXGI_FORMAT HdrLightingFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

    void Initialize(
        ID3D12Device* device,
        UINT width,
        UINT height,
        TrDescriptorHeap& rtvHeap,
        TrDescriptorHeap& dsvHeap,
        TrDescriptorHeap& resourceHeap,
        float depthClearValue);
    void Resize(ID3D12Device* device, UINT width, UINT height);

    void BeginDepthNormalPass(ID3D12GraphicsCommandList* commandList);
    void EndDepthNormalPass(ID3D12GraphicsCommandList* commandList);
    void BeginGBufferPass(
        ID3D12GraphicsCommandList* commandList,
        bool preserveDepthNormal);
    void EndGBufferPass(ID3D12GraphicsCommandList* commandList);
    void BeginDeferredLightingPass(ID3D12GraphicsCommandList* commandList);
    void EndDeferredLightingPass(ID3D12GraphicsCommandList* commandList);
    void BeginForwardTransparentPass(ID3D12GraphicsCommandList* commandList);
    void EndForwardTransparentPass(ID3D12GraphicsCommandList* commandList);

    TrTexture& GetBaseColorRoughness() { return mBaseColorRoughness; }
    const TrTexture& GetBaseColorRoughness() const { return mBaseColorRoughness; }
    const TrTexture& GetNormalMetallic() const { return mNormalMetallic; }
    const TrTexture& GetEmissiveOcclusion() const { return mEmissiveOcclusion; }
    const TrTexture& GetDepth() const { return mDepth; }
    const TrTexture& GetHdrLighting() const { return mHdrLighting; }

    const TrDescriptorAllocation& GetBaseColorSrv() const { return mBaseColorSrv; }
    const TrDescriptorAllocation& GetNormalSrv() const { return mNormalSrv; }
    const TrDescriptorAllocation& GetDepthSrv() const { return mDepthSrv; }
    const TrDescriptorAllocation& GetStencilSrv() const { return mStencilSrv; }
    const TrDescriptorAllocation& GetEmissiveSrv() const { return mEmissiveSrv; }
    const TrDescriptorAllocation& GetHdrLightingSrv() const { return mHdrLightingSrv; }
    TrDepthNormalView GetDepthNormalView() const;

private:
    void CreateResources(ID3D12Device* device, UINT width, UINT height);

    TrTexture mBaseColorRoughness;
    TrTexture mNormalMetallic;
    TrTexture mDepth;
    TrTexture mEmissiveOcclusion;
    TrTexture mHdrLighting;

    TrDescriptorAllocation mBaseColorRtv;
    TrDescriptorAllocation mNormalRtv;
    TrDescriptorAllocation mEmissiveRtv;
    TrDescriptorAllocation mHdrLightingRtv;
    TrDescriptorAllocation mDepthDsv;
    TrDescriptorAllocation mReadOnlyDepthDsv;

    TrDescriptorAllocation mBaseColorSrv;
    TrDescriptorAllocation mNormalSrv;
    TrDescriptorAllocation mDepthSrv;
    TrDescriptorAllocation mEmissiveSrv;
    TrDescriptorAllocation mHdrLightingSrv;
    TrDescriptorAllocation mStencilSrv;
    float mDepthClearValue = 1.0f;
};
