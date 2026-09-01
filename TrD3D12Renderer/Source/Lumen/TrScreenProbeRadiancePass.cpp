#include "TrScreenProbeRadiancePass.h"

#include "Renderer/TrRenderConstants.h"

#include <stdexcept>

void TrScreenProbeRadiancePass::Initialize(
    ID3D12Device* device,
    const std::wstring& shaderPath)
{
    CD3DX12_DESCRIPTOR_RANGE traceHitRange;
    traceHitRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE gBufferRange;
    gBufferRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 1);
    CD3DX12_DESCRIPTOR_RANGE radianceRange;
    radianceRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

    CD3DX12_ROOT_PARAMETER rootParameters[5];
    rootParameters[0].InitAsConstantBufferView(TrConstantRegister::Scene);
    rootParameters[1].InitAsConstantBufferView(TrConstantRegister::Pass);
    rootParameters[2].InitAsDescriptorTable(1, &traceHitRange);
    rootParameters[3].InitAsDescriptorTable(1, &gBufferRange);
    rootParameters[4].InitAsDescriptorTable(1, &radianceRange);

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(
        _countof(rootParameters),
        rootParameters,
        0,
        nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_NONE);

    TrComputePipelineDesc pipelineDesc;
    pipelineDesc.ShaderPath = shaderPath;
    pipelineDesc.RootSignatureDesc = &rootSignatureDesc;
    mPipeline.Initialize(device, pipelineDesc);
}

TrScreenProbeRadiancePass::Outputs TrScreenProbeRadiancePass::Resolve(
    ID3D12GraphicsCommandList* commandList,
    TrDescriptorHeap& resourceHeap,
    D3D12_GPU_VIRTUAL_ADDRESS sceneConstants,
    D3D12_GPU_VIRTUAL_ADDRESS lightingPassConstants,
    const TrDeferredRenderTargets& renderTargets,
    TrScreenProbeResources& screenProbes)
{
    if(commandList == nullptr || resourceHeap.Get() == nullptr ||
       !resourceHeap.IsShaderVisible() || sceneConstants == 0 ||
       lightingPassConstants == 0)
    {
        throw std::invalid_argument(
            "Screen Probe Radiance pass inputs are invalid.");
    }

    const TrScreenProbeLayout& layout = screenProbes.GetLayout();
    const D3D12_RESOURCE_DESC& gBufferDescription =
        renderTargets.GetBaseColorRoughness().GetDescription();
    const D3D12_RESOURCE_DESC& hitDescription =
        screenProbes.GetTraceHit().GetDescription();
    const D3D12_RESOURCE_DESC& radianceDescription =
        screenProbes.GetRadiance().GetDescription();
    if(gBufferDescription.Width != layout.RenderWidth ||
       gBufferDescription.Height != layout.RenderHeight ||
       hitDescription.Width != layout.TraceAtlasWidth ||
       hitDescription.Height != layout.TraceAtlasHeight ||
       radianceDescription.Width != layout.TraceAtlasWidth ||
       radianceDescription.Height != layout.TraceAtlasHeight)
    {
        throw std::logic_error(
            "Screen Probe Radiance resources have incompatible dimensions.");
    }

    TrTexture& traceHit = screenProbes.GetTraceHit();
    TrTexture& radiance = screenProbes.GetRadiance();
    traceHit.Transition(
        commandList,
        D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    radiance.Transition(
        commandList,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    ID3D12DescriptorHeap* descriptorHeaps[] = {resourceHeap.Get()};
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    commandList->SetPipelineState(mPipeline.GetPipelineState());
    commandList->SetComputeRootSignature(mPipeline.GetRootSignature());
    commandList->SetComputeRootConstantBufferView(0, sceneConstants);
    commandList->SetComputeRootConstantBufferView(
        1,
        lightingPassConstants);
    commandList->SetComputeRootDescriptorTable(
        2,
        screenProbes.GetTraceHitSrv().GpuHandle);
    commandList->SetComputeRootDescriptorTable(
        3,
        renderTargets.GetBaseColorSrv().GpuHandle);
    commandList->SetComputeRootDescriptorTable(
        4,
        screenProbes.GetRadianceUav().GpuHandle);
    commandList->Dispatch(
        (layout.TraceAtlasWidth + 7u) / 8u,
        (layout.TraceAtlasHeight + 7u) / 8u,
        1);

    radiance.UavBarrier(commandList);
    radiance.Transition(
        commandList,
        D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

    Outputs outputs;
    outputs.Radiance = &radiance;
    return outputs;
}
