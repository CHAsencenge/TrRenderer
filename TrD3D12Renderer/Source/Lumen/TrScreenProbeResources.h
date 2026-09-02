#pragma once

#include "Resources/TrDescriptorHeap.h"
#include "Resources/TrHistoryTexture.h"
#include "Resources/TrTexture.h"
#include "TrScreenTraceHit.h"

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

// Persistent allocations shared by Screen Probe placement, Screen Trace and
// lighting evaluation. Per-frame working textures are overwritten each frame;
// the temporal histories retain resolved irradiance and the geometry identity
// used to validate reprojection. Resize rewrites descriptors in place.
class TrScreenProbeResources
{
public:
    static constexpr DXGI_FORMAT PositionFormat =
        DXGI_FORMAT_R32G32B32A32_FLOAT;
    static constexpr DXGI_FORMAT NormalDepthFormat =
        DXGI_FORMAT_R16G16B16A16_FLOAT;
    static constexpr DXGI_FORMAT TraceHitFormat =
        DXGI_FORMAT_R32G32B32A32_UINT;
    static constexpr DXGI_FORMAT TraceDebugFormat =
        DXGI_FORMAT_R16G16B16A16_FLOAT;
    static constexpr DXGI_FORMAT RadianceFormat =
        DXGI_FORMAT_R16G16B16A16_FLOAT;
    static constexpr DXGI_FORMAT IrradianceFormat =
        DXGI_FORMAT_R16G16B16A16_FLOAT;

    void Initialize(
        ID3D12Device* device,
        UINT width,
        UINT height,
        TrDescriptorHeap& resourceHeap);
    void Resize(ID3D12Device* device, UINT width, UINT height);
    void AdvanceHistory();
    void InvalidateHistory();

    bool IsHistoryValid() const;

    const TrScreenProbeLayout& GetLayout() const { return mLayout; }

    TrTexture& GetPositionValidity() { return mPositionValidity; }
    TrTexture& GetNormalDepth() { return mNormalDepth; }
    TrTexture& GetTraceHit() { return mTraceHit; }
    TrTexture& GetTraceDebug() { return mTraceDebug; }
    TrTexture& GetRadiance() { return mRadiance; }
    TrTexture& GetIrradiance() { return mIrradiance; }
    const TrTexture& GetPositionValidity() const { return mPositionValidity; }
    const TrTexture& GetNormalDepth() const { return mNormalDepth; }
    const TrTexture& GetTraceHit() const { return mTraceHit; }
    const TrTexture& GetTraceDebug() const { return mTraceDebug; }
    const TrTexture& GetRadiance() const { return mRadiance; }
    const TrTexture& GetIrradiance() const { return mIrradiance; }

    const TrDescriptorAllocation& GetPositionSrv() const { return mPositionSrv; }
    const TrDescriptorAllocation& GetPositionUav() const { return mPositionUav; }
    const TrDescriptorAllocation& GetNormalDepthSrv() const { return mNormalDepthSrv; }
    const TrDescriptorAllocation& GetNormalDepthUav() const { return mNormalDepthUav; }
    const TrDescriptorAllocation& GetTraceHitSrv() const { return mTraceHitSrv; }
    const TrDescriptorAllocation& GetTraceHitUav() const { return mTraceHitUav; }
    const TrDescriptorAllocation& GetTraceDebugSrv() const { return mTraceDebugSrv; }
    const TrDescriptorAllocation& GetTraceDebugUav() const { return mTraceDebugUav; }
    const TrDescriptorAllocation& GetRadianceSrv() const { return mRadianceSrv; }
    const TrDescriptorAllocation& GetRadianceUav() const { return mRadianceUav; }
    const TrDescriptorAllocation& GetIrradianceSrv() const { return mIrradianceSrv; }
    const TrDescriptorAllocation& GetIrradianceUav() const { return mIrradianceUav; }

    TrHistoryTexture& GetIrradianceHistory() { return mIrradianceHistory; }
    TrHistoryTexture& GetPositionHistory() { return mPositionHistory; }
    TrHistoryTexture& GetNormalDepthHistory() { return mNormalDepthHistory; }
    const TrHistoryTexture& GetIrradianceHistory() const { return mIrradianceHistory; }
    const TrHistoryTexture& GetPositionHistory() const { return mPositionHistory; }
    const TrHistoryTexture& GetNormalDepthHistory() const { return mNormalDepthHistory; }

private:
    void CreateResources(ID3D12Device* device, UINT width, UINT height);

    TrScreenProbeLayout mLayout;
    TrTexture mPositionValidity;
    TrTexture mNormalDepth;
    TrTexture mTraceHit;
    TrTexture mTraceDebug;
    TrTexture mRadiance;
    TrTexture mIrradiance;
    TrHistoryTexture mIrradianceHistory;
    TrHistoryTexture mPositionHistory;
    TrHistoryTexture mNormalDepthHistory;
    TrDescriptorAllocation mPositionSrv;
    TrDescriptorAllocation mPositionUav;
    TrDescriptorAllocation mNormalDepthSrv;
    TrDescriptorAllocation mNormalDepthUav;
    TrDescriptorAllocation mTraceHitSrv;
    TrDescriptorAllocation mTraceHitUav;
    TrDescriptorAllocation mTraceDebugSrv;
    TrDescriptorAllocation mTraceDebugUav;
    TrDescriptorAllocation mRadianceSrv;
    TrDescriptorAllocation mRadianceUav;
    TrDescriptorAllocation mIrradianceSrv;
    TrDescriptorAllocation mIrradianceUav;
};
