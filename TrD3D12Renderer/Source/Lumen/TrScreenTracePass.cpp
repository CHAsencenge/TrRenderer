#include "TrScreenTracePass.h"

#include "Renderer/TrRenderConfig.h"
#include "Renderer/TrRenderConstants.h"

#include <algorithm>
#include <stdexcept>

void TrScreenTracePass::Initialize(
    ID3D12Device* device,
    const std::wstring& shaderPath)
{
    CD3DX12_DESCRIPTOR_RANGE positionRange;
    positionRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE normalDepthRange;
    normalDepthRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    CD3DX12_DESCRIPTOR_RANGE hzbRange;
    hzbRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
    CD3DX12_DESCRIPTOR_RANGE resultRange;
    resultRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

    CD3DX12_ROOT_PARAMETER rootParameters[6];
    rootParameters[0].InitAsDescriptorTable(1, &positionRange);
    rootParameters[1].InitAsDescriptorTable(1, &normalDepthRange);
    rootParameters[2].InitAsDescriptorTable(1, &hzbRange);
    rootParameters[3].InitAsDescriptorTable(1, &resultRange);
    rootParameters[4].InitAsConstantBufferView(TrConstantRegister::View);
    rootParameters[5].InitAsConstants(
        sizeof(TrScreenTraceConstants) / sizeof(std::uint32_t),
        TrConstantRegister::Pass);

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
    pipelineDesc.ShaderDefines = TrRenderConfig::GetDepthShaderDefines();
    mPipeline.Initialize(device, pipelineDesc);
}

TrScreenTracePass::Outputs TrScreenTracePass::Trace(
    ID3D12GraphicsCommandList* commandList,
    TrDescriptorHeap& resourceHeap,
    D3D12_GPU_VIRTUAL_ADDRESS viewConstants,
    const Inputs& inputs,
    TrScreenProbeResources& screenProbes)
{
    if(commandList == nullptr || resourceHeap.Get() == nullptr ||
       !resourceHeap.IsShaderVisible() || viewConstants == 0 ||
       inputs.HierarchicalDepth == nullptr || inputs.ScreenProbes == nullptr ||
       inputs.ScreenProbes != &screenProbes)
    {
        throw std::invalid_argument("Screen Trace pass inputs are incomplete.");
    }

    const TrScreenProbeLayout& layout = screenProbes.GetLayout();
    const TrHierarchicalDepthDesc& hzbDescription =
        inputs.HierarchicalDepth->GetDescription();
    if(layout.RenderWidth != hzbDescription.Width ||
       layout.RenderHeight != hzbDescription.Height ||
       hzbDescription.MipCount == 0)
    {
        throw std::logic_error(
            "Screen Trace inputs have incompatible dimensions.");
    }

    ID3D12DescriptorHeap* descriptorHeaps[] = {resourceHeap.Get()};
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    commandList->SetPipelineState(mPipeline.GetPipelineState());
    commandList->SetComputeRootSignature(mPipeline.GetRootSignature());

    TrTexture& traceResult = screenProbes.GetTraceResult();
    traceResult.Transition(
        commandList,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    commandList->SetComputeRootDescriptorTable(
        0,
        screenProbes.GetPositionSrv().GpuHandle);
    commandList->SetComputeRootDescriptorTable(
        1,
        screenProbes.GetNormalDepthSrv().GpuHandle);
    commandList->SetComputeRootDescriptorTable(
        2,
        inputs.HierarchicalDepth->GetSrv().GpuHandle);
    commandList->SetComputeRootDescriptorTable(
        3,
        screenProbes.GetTraceResultUav().GpuHandle);
    commandList->SetComputeRootConstantBufferView(4, viewConstants);

    const TrScreenTraceConstants constants =
    {
        layout.ProbeCountX,
        layout.ProbeCountY,
        layout.TraceAtlasWidth,
        layout.TraceAtlasHeight,
        TrScreenProbeLayout::RayGridDimension,
        hzbDescription.MipCount,
        std::min(4u, hzbDescription.MipCount - 1u),
        96u,
        20.0f,
        0.02f,
        0.08f,
        0.04f
    };
    commandList->SetComputeRoot32BitConstants(
        5,
        sizeof(constants) / sizeof(std::uint32_t),
        &constants,
        0);
    commandList->Dispatch(
        (layout.TraceAtlasWidth + 7u) / 8u,
        (layout.TraceAtlasHeight + 7u) / 8u,
        1);

    traceResult.UavBarrier(commandList);
    traceResult.Transition(
        commandList,
        D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

    Outputs outputs;
    outputs.TraceResult = &traceResult;
    return outputs;
}
