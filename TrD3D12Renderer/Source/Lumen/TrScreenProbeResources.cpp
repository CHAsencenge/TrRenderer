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
    mTraceResultSrv = resourceHeap.Allocate();
    mTraceResultUav = resourceHeap.Allocate();
    CreateResources(device, width, height);
}

void TrScreenProbeResources::Resize(
    ID3D12Device* device,
    UINT width,
    UINT height)
{
    if(mPositionSrv.Index == UINT_MAX || mPositionUav.Index == UINT_MAX ||
       mNormalDepthSrv.Index == UINT_MAX || mNormalDepthUav.Index == UINT_MAX ||
       mTraceResultSrv.Index == UINT_MAX || mTraceResultUav.Index == UINT_MAX)
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
    mTraceResult.Initialize2D(
        device,
        traceWidth,
        traceHeight,
        TraceResultFormat,
        flags,
        initialState,
        nullptr,
        L"Lumen Screen Trace Result");

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
    mTraceResult.CreateShaderResourceView(
        device,
        mTraceResultSrv.CpuHandle);
    mTraceResult.CreateUnorderedAccessView(
        device,
        mTraceResultUav.CpuHandle);
}
