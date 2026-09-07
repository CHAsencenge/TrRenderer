#pragma once

#include "Resources/TrDescriptorHeap.h"
#include "Resources/TrHistoryTexture.h"
#include "Resources/TrTexture.h"
#include "TrScreenTraceHit.h"

struct TrScreenProbeLayout
{
    static constexpr UINT TileSize = 16;

    static constexpr UINT RayGridDimension = 4; // 每个 probe 在 Trace Atlas 中占据的块的边长
    static constexpr UINT RaysPerProbe =
        RayGridDimension * RayGridDimension;
   
    static constexpr UINT DirectionGridDimension = 8; // 固定世界空间方向 atlas 的边长。
    static constexpr UINT DirectionsPerProbe =
        DirectionGridDimension * DirectionGridDimension;

    static constexpr UINT ShCoefficientCount = 9;
    static constexpr UINT ShCoefficientGridDimension = 3;
    static_assert(
        ShCoefficientCount ==
        ShCoefficientGridDimension * ShCoefficientGridDimension);

    UINT RenderWidth = 0;
    UINT RenderHeight = 0;
    UINT ProbeCountX = 0;
    UINT ProbeCountY = 0;
    UINT TraceAtlasWidth = 0;
    UINT TraceAtlasHeight = 0;
    UINT DirectionAtlasWidth = 0;
    UINT DirectionAtlasHeight = 0;
    UINT IrradianceAtlasWidth = 0;
    UINT IrradianceAtlasHeight = 0;
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
    static constexpr DXGI_FORMAT TraceRadianceFormat =
        DXGI_FORMAT_R16G16B16A16_FLOAT;
    static constexpr DXGI_FORMAT DirectionRadianceFormat =
        DXGI_FORMAT_R16G16B16A16_FLOAT;
    static constexpr DXGI_FORMAT DirectionHitDistanceFormat =
        DXGI_FORMAT_R16_FLOAT;
    // x = temporal hit distance
    // y = history length
    // z = luminance first moment
    // w = luminance second moment
    static constexpr DXGI_FORMAT DirectionHistoryAuxFormat = 
        DXGI_FORMAT_R16G16B16A16_FLOAT;
    static constexpr DXGI_FORMAT IrradianceFormat =
        DXGI_FORMAT_R16G16B16A16_FLOAT;
    static constexpr DXGI_FORMAT TemporalDebugFormat =
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
    TrTexture& GetRadiance() { return mTraceRadiance; }
    TrTexture& GetIrradiance() { return mIrradiance; }
    TrTexture& GetTemporalDebug() { return mTemporalDebug; }
    const TrTexture& GetPositionValidity() const { return mPositionValidity; }
    const TrTexture& GetNormalDepth() const { return mNormalDepth; }
    const TrTexture& GetTraceHit() const { return mTraceHit; }
    const TrTexture& GetTraceDebug() const { return mTraceDebug; }
    const TrTexture& GetRadiance() const { return mTraceRadiance; }
    const TrTexture& GetIrradiance() const { return mIrradiance; }
    const TrTexture& GetTemporalDebug() const { return mTemporalDebug; }

    const TrDescriptorAllocation& GetPositionSrv() const { return mPositionSrv; }
    const TrDescriptorAllocation& GetPositionUav() const { return mPositionUav; }
    const TrDescriptorAllocation& GetNormalDepthSrv() const { return mNormalDepthSrv; }
    const TrDescriptorAllocation& GetNormalDepthUav() const { return mNormalDepthUav; }
    const TrDescriptorAllocation& GetTraceHitSrv() const { return mTraceHitSrv; }
    const TrDescriptorAllocation& GetTraceHitUav() const { return mTraceHitUav; }
    const TrDescriptorAllocation& GetTraceDebugSrv() const { return mTraceDebugSrv; }
    const TrDescriptorAllocation& GetTraceDebugUav() const { return mTraceDebugUav; }
    const TrDescriptorAllocation& GetTraceRadianceSrv() const { return mTraceRadianceSrv; }
    const TrDescriptorAllocation& GetTraceRadianceUav() const { return mTraceRadianceUav; }
    const TrDescriptorAllocation& GetIrradianceSrv() const { return mIrradianceSrv; }
    const TrDescriptorAllocation& GetIrradianceUav() const { return mIrradianceUav; }
    const TrDescriptorAllocation& GetTemporalDebugSrv() const { return mTemporalDebugSrv; }
    const TrDescriptorAllocation& GetTemporalDebugUav() const { return mTemporalDebugUav; }

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
    TrTexture mTraceRadiance;
    // Each probe occupies a 3x3 block containing its nine world-space SH L2
    // diffuse-irradiance coefficients.
    TrTexture mIrradiance;
    // Per-probe temporal diagnostics: RGB stores the history status color and
    // alpha stores the resolved history blend weight.
    TrTexture mTemporalDebug;
    // RGB = 当前方向 radiance 
    // A = 当前方向 lighting support / confidence
    TrTexture mDirectionRadiance;
    TrTexture mDirectionHitDistance;
    TrTexture mFilteredDirectionRadiance;
    TrTexture mDirectionTemporalDebug;

    TrHistoryTexture mDirectionRadianceHistory;
    // R = resolved hit distance
    // G = history length
    // B = luminance 一阶矩
    // A = luminance 二阶矩
    TrHistoryTexture mDirectionAuxHistory;
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
    TrDescriptorAllocation mTraceRadianceSrv;
    TrDescriptorAllocation mTraceRadianceUav;
    TrDescriptorAllocation mIrradianceSrv;
    TrDescriptorAllocation mIrradianceUav;
    TrDescriptorAllocation mTemporalDebugSrv;
    TrDescriptorAllocation mTemporalDebugUav;
};
