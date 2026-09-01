#pragma once

#include "Resources/TrDescriptorHeap.h"
#include "Resources/TrTexture.h"

struct TrScreenProbeLayout
{
    static constexpr UINT TileSize = 16;
    static constexpr UINT RayGridDimension = 4;
    static constexpr UINT RaysPerProbe =
        RayGridDimension * RayGridDimension;

    UINT RenderWidth = 0;
    UINT RenderHeight = 0;
    UINT ProbeCountX = 0;
    UINT ProbeCountY = 0;
    UINT TraceAtlasWidth = 0;
    UINT TraceAtlasHeight = 0;
};

// Persistent screen-sized resources shared by Screen Probe placement and
// Screen Trace. Descriptors are allocated once and rewritten in place on
// resize so GPU debug view handles remain stable.
class TrScreenProbeResources
{
public:
    static constexpr DXGI_FORMAT PositionFormat =
        DXGI_FORMAT_R32G32B32A32_FLOAT;
    static constexpr DXGI_FORMAT NormalDepthFormat =
        DXGI_FORMAT_R16G16B16A16_FLOAT;
    static constexpr DXGI_FORMAT TraceResultFormat =
        DXGI_FORMAT_R16G16B16A16_FLOAT;

    void Initialize(
        ID3D12Device* device,
        UINT width,
        UINT height,
        TrDescriptorHeap& resourceHeap);
    void Resize(ID3D12Device* device, UINT width, UINT height);

    const TrScreenProbeLayout& GetLayout() const { return mLayout; }

    TrTexture& GetPositionValidity() { return mPositionValidity; }
    TrTexture& GetNormalDepth() { return mNormalDepth; }
    TrTexture& GetTraceResult() { return mTraceResult; }
    const TrTexture& GetPositionValidity() const { return mPositionValidity; }
    const TrTexture& GetNormalDepth() const { return mNormalDepth; }
    const TrTexture& GetTraceResult() const { return mTraceResult; }

    const TrDescriptorAllocation& GetPositionSrv() const { return mPositionSrv; }
    const TrDescriptorAllocation& GetPositionUav() const { return mPositionUav; }
    const TrDescriptorAllocation& GetNormalDepthSrv() const { return mNormalDepthSrv; }
    const TrDescriptorAllocation& GetNormalDepthUav() const { return mNormalDepthUav; }
    const TrDescriptorAllocation& GetTraceResultSrv() const { return mTraceResultSrv; }
    const TrDescriptorAllocation& GetTraceResultUav() const { return mTraceResultUav; }

private:
    void CreateResources(ID3D12Device* device, UINT width, UINT height);

    TrScreenProbeLayout mLayout;
    TrTexture mPositionValidity;
    TrTexture mNormalDepth;
    TrTexture mTraceResult;
    TrDescriptorAllocation mPositionSrv;
    TrDescriptorAllocation mPositionUav;
    TrDescriptorAllocation mNormalDepthSrv;
    TrDescriptorAllocation mNormalDepthUav;
    TrDescriptorAllocation mTraceResultSrv;
    TrDescriptorAllocation mTraceResultUav;
};
