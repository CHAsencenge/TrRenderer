#include "TrScreenProbeResources.h"

#include <stdexcept>

namespace
{
    UINT DivideRoundUp(UINT value, UINT divisor)
    {
        return (value + divisor - 1u) / divisor;
    }
}

void TrScreenProbeResources::Initialize(
    ID3D12Device* device,
    UINT width,
    UINT height,
    TrDescriptorHeap& resourceHeap)
{
    if(device == nullptr || width == 0 || height == 0 ||
       resourceHeap.Get() == nullptr || !resourceHeap.IsShaderVisible() ||
       resourceHeap.GetType() != D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
    {
        throw std::invalid_argument(
            "Screen Probe initialization inputs are invalid.");
    }
    if(mPositionSrv.Index != UINT_MAX)
    {
        throw std::logic_error(
            "Screen Probe resources have already been initialized.");
    }

    mPositionSrv = resourceHeap.Allocate();
    mPositionUav = resourceHeap.Allocate();
    mNormalDepthSrv = resourceHeap.Allocate();
    mNormalDepthUav = resourceHeap.Allocate();
    mTraceHitSrv = resourceHeap.Allocate();
    mTraceHitUav = resourceHeap.Allocate();
    mTraceDebugSrv = resourceHeap.Allocate();
    mTraceDebugUav = resourceHeap.Allocate();
    mRadianceSrv = resourceHeap.Allocate();
    mRadianceUav = resourceHeap.Allocate();
    mIrradianceSrv = resourceHeap.Allocate();
    mIrradianceUav = resourceHeap.Allocate();
    CreateResources(device, width, height);
}

void TrScreenProbeResources::Resize(
    ID3D12Device* device,
    UINT width,
    UINT height)
{
    if(mPositionSrv.Index == UINT_MAX || mPositionUav.Index == UINT_MAX ||
       mNormalDepthSrv.Index == UINT_MAX || mNormalDepthUav.Index == UINT_MAX ||
       mTraceHitSrv.Index == UINT_MAX || mTraceHitUav.Index == UINT_MAX ||
       mTraceDebugSrv.Index == UINT_MAX || mTraceDebugUav.Index == UINT_MAX ||
       mRadianceSrv.Index == UINT_MAX || mRadianceUav.Index == UINT_MAX ||
       mIrradianceSrv.Index == UINT_MAX || mIrradianceUav.Index == UINT_MAX)
    {
        throw std::logic_error(
            "Screen Probe resources have not been initialized.");
    }
    CreateResources(device, width, height);
}

void TrScreenProbeResources::CreateResources(
    ID3D12Device* device,
    UINT width,
    UINT height)
{
    if(device == nullptr || width == 0 || height == 0)
    {
        throw std::invalid_argument("Screen Probe dimensions are invalid.");
    }

    const UINT probeCountX = DivideRoundUp(width, TrScreenProbeLayout::TileSize);
    const UINT probeCountY = DivideRoundUp(height, TrScreenProbeLayout::TileSize);
    const UINT traceWidth =
        probeCountX * TrScreenProbeLayout::RayGridDimension;
    const UINT traceHeight =
        probeCountY * TrScreenProbeLayout::RayGridDimension;
    if(traceWidth > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
       traceHeight > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION)
    {
        throw std::out_of_range("Screen Probe trace atlas is too large.");
    }

    mLayout.RenderWidth = width;
    mLayout.RenderHeight = height;
    mLayout.ProbeCountX = probeCountX;
    mLayout.ProbeCountY = probeCountY;
    mLayout.TraceAtlasWidth = traceWidth;
    mLayout.TraceAtlasHeight = traceHeight;

    constexpr D3D12_RESOURCE_FLAGS flags =
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    constexpr D3D12_RESOURCE_STATES initialState =
        D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
    mPositionValidity.Initialize2D(
        device,
        probeCountX,
        probeCountY,
        PositionFormat,
        flags,
        initialState,
        nullptr,
        L"Lumen Screen Probe Position Validity");
    mNormalDepth.Initialize2D(
        device,
        probeCountX,
        probeCountY,
        NormalDepthFormat,
        flags,
        initialState,
        nullptr,
        L"Lumen Screen Probe Normal Depth");
    mTraceHit.Initialize2D(
        device,
        traceWidth,
        traceHeight,
        TraceHitFormat,
        flags,
        initialState,
        nullptr,
        L"Lumen Screen Trace Hit Payload");
    mTraceDebug.Initialize2D(
        device,
        traceWidth,
        traceHeight,
        TraceDebugFormat,
        flags,
        initialState,
        nullptr,
        L"Lumen Screen Trace Debug");
    mRadiance.Initialize2D(
        device,
        traceWidth,
        traceHeight,
        RadianceFormat,
        flags,
        initialState,
        nullptr,
        L"Lumen Screen Probe Radiance");
    mIrradiance.Initialize2D(
        device,
        probeCountX,
        probeCountY,
        IrradianceFormat,
        flags,
        initialState,
        nullptr,
        L"Lumen Screen Probe Irradiance");

    mPositionValidity.CreateShaderResourceView(
        device,
        mPositionSrv.CpuHandle);
    mPositionValidity.CreateUnorderedAccessView(
        device,
        mPositionUav.CpuHandle);
    mNormalDepth.CreateShaderResourceView(
        device,
        mNormalDepthSrv.CpuHandle);
    mNormalDepth.CreateUnorderedAccessView(
        device,
        mNormalDepthUav.CpuHandle);
    mTraceHit.CreateShaderResourceView(
        device,
        mTraceHitSrv.CpuHandle);
    mTraceHit.CreateUnorderedAccessView(
        device,
        mTraceHitUav.CpuHandle);
    mTraceDebug.CreateShaderResourceView(
        device,
        mTraceDebugSrv.CpuHandle);
    mTraceDebug.CreateUnorderedAccessView(
        device,
        mTraceDebugUav.CpuHandle);
    mRadiance.CreateShaderResourceView(
        device,
        mRadianceSrv.CpuHandle);
    mRadiance.CreateUnorderedAccessView(
        device,
        mRadianceUav.CpuHandle);
    mIrradiance.CreateShaderResourceView(
        device,
        mIrradianceSrv.CpuHandle);
    mIrradiance.CreateUnorderedAccessView(
        device,
        mIrradianceUav.CpuHandle);
}
