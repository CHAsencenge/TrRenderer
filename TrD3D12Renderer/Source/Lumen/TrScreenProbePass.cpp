#include "TrScreenProbePass.h"

#include "Renderer/TrRenderConfig.h"
#include "Renderer/TrRenderConstants.h"

#include <stdexcept>

void TrScreenProbePass::Initialize(
    ID3D12Device* device,
    const std::wstring& shaderPath)
{
    CD3DX12_DESCRIPTOR_RANGE depthRange;
    depthRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE normalRange;
    normalRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    CD3DX12_DESCRIPTOR_RANGE positionRange;
    positionRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE normalDepthRange;
    normalDepthRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);

    CD3DX12_ROOT_PARAMETER rootParameters[6];
    rootParameters[0].InitAsDescriptorTable(1, &depthRange);
    rootParameters[1].InitAsDescriptorTable(1, &normalRange);
    rootParameters[2].InitAsDescriptorTable(1, &positionRange);
    rootParameters[3].InitAsDescriptorTable(1, &normalDepthRange);
    rootParameters[4].InitAsConstantBufferView(TrConstantRegister::View);
    rootParameters[5].InitAsConstants(
        sizeof(TrScreenProbeBuildConstants) / sizeof(std::uint32_t),
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

TrScreenProbePass::Outputs TrScreenProbePass::Build(
    ID3D12GraphicsCommandList* commandList,
    TrDescriptorHeap& resourceHeap,
    D3D12_GPU_VIRTUAL_ADDRESS viewConstants,
    const Inputs& inputs,
    TrScreenProbeResources& screenProbes)
{
    if(commandList == nullptr || resourceHeap.Get() == nullptr ||
       !resourceHeap.IsShaderVisible() || viewConstants == 0)
    {
        throw std::invalid_argument("Screen Probe pass inputs are incomplete.");
    }
    inputs.DepthNormal.ValidateForCompute();

    const TrScreenProbeLayout& layout = screenProbes.GetLayout();
    if(layout.RenderWidth != inputs.DepthNormal.GetWidth() ||
       layout.RenderHeight != inputs.DepthNormal.GetHeight() ||
       layout.ProbeCountX == 0 || layout.ProbeCountY == 0)
    {
        throw std::logic_error(
            "Screen Probe resources do not match the source depth.");
    }

    ID3D12DescriptorHeap* descriptorHeaps[] = {resourceHeap.Get()};
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    commandList->SetPipelineState(mPipeline.GetPipelineState());
    commandList->SetComputeRootSignature(mPipeline.GetRootSignature());

    TrTexture& position = screenProbes.GetPositionValidity();
    TrTexture& normalDepth = screenProbes.GetNormalDepth();
    position.Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    normalDepth.Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    commandList->SetComputeRootDescriptorTable(
        0,
        inputs.DepthNormal.GetDepthSrv().GpuHandle);
    commandList->SetComputeRootDescriptorTable(
        1,
        inputs.DepthNormal.GetWorldNormalSrv().GpuHandle);
    commandList->SetComputeRootDescriptorTable(
        2,
        screenProbes.GetPositionUav().GpuHandle);
    commandList->SetComputeRootDescriptorTable(
        3,
        screenProbes.GetNormalDepthUav().GpuHandle);
    commandList->SetComputeRootConstantBufferView(4, viewConstants);

    const TrScreenProbeBuildConstants constants =
    {
        layout.RenderWidth,
        layout.RenderHeight,
        layout.ProbeCountX,
        layout.ProbeCountY,
        TrScreenProbeLayout::TileSize
    };
    commandList->SetComputeRoot32BitConstants(
        5,
        sizeof(constants) / sizeof(std::uint32_t),
        &constants,
        0);
    commandList->Dispatch(
        (layout.ProbeCountX + 7u) / 8u,
        (layout.ProbeCountY + 7u) / 8u,
        1);

    position.UavBarrier(commandList);
    normalDepth.UavBarrier(commandList);
    position.Transition(
        commandList,
        D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    normalDepth.Transition(
        commandList,
        D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

    Outputs outputs;
    outputs.ScreenProbes = &screenProbes;
    return outputs;
}
