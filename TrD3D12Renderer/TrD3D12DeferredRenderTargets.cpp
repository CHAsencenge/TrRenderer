#include "TrD3D12DeferredRenderTargets.h"

#include <stdexcept>

void TrD3D12DeferredRenderTargets::Initialize(
    ID3D12Device* device,
    UINT width,
    UINT height,
    TrD3D12DescriptorHeap& rtvHeap,
    TrD3D12DescriptorHeap& dsvHeap,
    TrD3D12DescriptorHeap& resourceHeap)
{
    D3D12_CLEAR_VALUE baseColorClear = {};
    baseColorClear.Format = BaseColorRoughnessFormat;
    baseColorClear.Color[3] = 1.0f;
    mBaseColorRoughness.Initialize2D(
        device,
        width,
        height,
        BaseColorRoughnessFormat,
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &baseColorClear,
        L"GBuffer BaseColor Roughness");

    D3D12_CLEAR_VALUE normalClear = {};
    normalClear.Format = NormalMetallicFormat;
    mNormalMetallic.Initialize2D(
        device,
        width,
        height,
        NormalMetallicFormat,
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &normalClear,
        L"GBuffer WorldNormal Metallic");

    D3D12_CLEAR_VALUE depthClear = {};
    depthClear.Format = DepthViewFormat;
    depthClear.DepthStencil.Depth = 1.0f;
    mDepth.Initialize2D(
        device,
        width,
        height,
        DepthResourceFormat,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthClear,
        L"GBuffer Depth");

    D3D12_CLEAR_VALUE hdrClear = {};
    hdrClear.Format = HdrLightingFormat;
    hdrClear.Color[3] = 1.0f;
    mHdrLighting.Initialize2D(
        device,
        width,
        height,
        HdrLightingFormat,
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &hdrClear,
        L"HDR Lighting");

    mBaseColorRtv = rtvHeap.Allocate();
    mNormalRtv = rtvHeap.Allocate();
    mHdrLightingRtv = rtvHeap.Allocate();
    mDepthDsv = dsvHeap.Allocate();

    mBaseColorSrv = resourceHeap.Allocate();
    mNormalSrv = resourceHeap.Allocate();
    mDepthSrv = resourceHeap.Allocate();
    mHdrLightingSrv = resourceHeap.Allocate();

    if(mNormalSrv.Index != mBaseColorSrv.Index + 1 ||
       mDepthSrv.Index != mBaseColorSrv.Index + 2)
    {
        throw std::logic_error("GBuffer SRVs must occupy one contiguous descriptor table.");
    }

    mBaseColorRoughness.CreateRenderTargetView(device, mBaseColorRtv.CpuHandle);
    mNormalMetallic.CreateRenderTargetView(device, mNormalRtv.CpuHandle);
    mHdrLighting.CreateRenderTargetView(device, mHdrLightingRtv.CpuHandle);
    mDepth.CreateDepthStencilView(device, mDepthDsv.CpuHandle, DepthViewFormat);

    mBaseColorRoughness.CreateShaderResourceView(device, mBaseColorSrv.CpuHandle);
    mNormalMetallic.CreateShaderResourceView(device, mNormalSrv.CpuHandle);
    mDepth.CreateShaderResourceView(device, mDepthSrv.CpuHandle, DepthSrvFormat);
    mHdrLighting.CreateShaderResourceView(device, mHdrLightingSrv.CpuHandle);
}

void TrD3D12DeferredRenderTargets::BeginGBufferPass(
    ID3D12GraphicsCommandList* commandList)
{
    mBaseColorRoughness.Transition(commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
    mNormalMetallic.Transition(commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
    mDepth.Transition(commandList, D3D12_RESOURCE_STATE_DEPTH_WRITE);

    const D3D12_CPU_DESCRIPTOR_HANDLE renderTargets[] =
    {
        mBaseColorRtv.CpuHandle,
        mNormalRtv.CpuHandle
    };
    commandList->OMSetRenderTargets(
        _countof(renderTargets),
        renderTargets,
        FALSE,
        &mDepthDsv.CpuHandle);

    const float baseColorClear[] = {0.0f, 0.0f, 0.0f, 1.0f};
    const float normalClear[] = {0.0f, 0.0f, 0.0f, 0.0f};
    commandList->ClearRenderTargetView(
        mBaseColorRtv.CpuHandle,
        baseColorClear,
        0,
        nullptr);
    commandList->ClearRenderTargetView(
        mNormalRtv.CpuHandle,
        normalClear,
        0,
        nullptr);
    commandList->ClearDepthStencilView(
        mDepthDsv.CpuHandle,
        D3D12_CLEAR_FLAG_DEPTH,
        1.0f,
        0,
        0,
        nullptr);
}

void TrD3D12DeferredRenderTargets::EndGBufferPass(
    ID3D12GraphicsCommandList* commandList)
{
    mBaseColorRoughness.Transition(
        commandList,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    mNormalMetallic.Transition(
        commandList,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    mDepth.Transition(
        commandList,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void TrD3D12DeferredRenderTargets::BeginDeferredLightingPass(
    ID3D12GraphicsCommandList* commandList)
{
    mHdrLighting.Transition(commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList->OMSetRenderTargets(
        1,
        &mHdrLightingRtv.CpuHandle,
        FALSE,
        nullptr);

    const float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
    commandList->ClearRenderTargetView(
        mHdrLightingRtv.CpuHandle,
        clearColor,
        0,
        nullptr);
}

void TrD3D12DeferredRenderTargets::EndDeferredLightingPass(
    ID3D12GraphicsCommandList* commandList)
{
    mHdrLighting.Transition(
        commandList,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}
