#include "TrScreenProbeTemporalPass.h"

#include "Renderer/TrRenderConfig.h"
#include "Renderer/TrRenderConstants.h"

#include <stdexcept>

void TrScreenProbeTemporalPass::Initialize(
    ID3D12Device* device,
    const std::wstring& shaderPath)
{
    CD3DX12_DESCRIPTOR_RANGE currentIrradianceRange;
    currentIrradianceRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE currentPositionRange;
    currentPositionRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    CD3DX12_DESCRIPTOR_RANGE currentNormalDepthRange;
    currentNormalDepthRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
    CD3DX12_DESCRIPTOR_RANGE previousIrradianceRange;
    previousIrradianceRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);
    CD3DX12_DESCRIPTOR_RANGE previousPositionRange;
    previousPositionRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4);
    CD3DX12_DESCRIPTOR_RANGE previousNormalDepthRange;
    previousNormalDepthRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5);
    CD3DX12_DESCRIPTOR_RANGE outputIrradianceRange;
    outputIrradianceRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE outputPositionRange;
    outputPositionRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);
    CD3DX12_DESCRIPTOR_RANGE outputNormalDepthRange;
    outputNormalDepthRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2);

    CD3DX12_ROOT_PARAMETER rootParameters[11];
    rootParameters[0].InitAsDescriptorTable(1, &currentIrradianceRange);
    rootParameters[1].InitAsDescriptorTable(1, &currentPositionRange);
    rootParameters[2].InitAsDescriptorTable(1, &currentNormalDepthRange);
    rootParameters[3].InitAsDescriptorTable(1, &previousIrradianceRange);
    rootParameters[4].InitAsDescriptorTable(1, &previousPositionRange);
    rootParameters[5].InitAsDescriptorTable(1, &previousNormalDepthRange);
    rootParameters[6].InitAsDescriptorTable(1, &outputIrradianceRange);
    rootParameters[7].InitAsDescriptorTable(1, &outputPositionRange);
    rootParameters[8].InitAsDescriptorTable(1, &outputNormalDepthRange);
    rootParameters[9].InitAsConstantBufferView(TrConstantRegister::View);
    rootParameters[10].InitAsConstants(
        sizeof(TrScreenProbeTemporalConstants) / sizeof(std::uint32_t),
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

TrScreenProbeTemporalPass::Outputs TrScreenProbeTemporalPass::Resolve(
    ID3D12GraphicsCommandList* commandList,
    TrDescriptorHeap& resourceHeap,
    D3D12_GPU_VIRTUAL_ADDRESS viewConstants,
    UINT frameNumber,
    TrScreenProbeResources& screenProbes)
{
    if(commandList == nullptr || resourceHeap.Get() == nullptr ||
       !resourceHeap.IsShaderVisible() || viewConstants == 0)
    {
        throw std::invalid_argument(
            "Screen Probe Temporal pass inputs are invalid.");
    }

    const TrScreenProbeLayout& layout = screenProbes.GetLayout();
    TrHistoryTexture& irradianceHistory =
        screenProbes.GetIrradianceHistory();
    TrHistoryTexture& positionHistory = screenProbes.GetPositionHistory();
    TrHistoryTexture& normalDepthHistory =
        screenProbes.GetNormalDepthHistory();

    const auto hasProbeDimensions = [&layout](const TrTexture& texture)
    {
        const D3D12_RESOURCE_DESC& description = texture.GetDescription();
        return description.Width == layout.ProbeCountX &&
            description.Height == layout.ProbeCountY;
    };
    if(layout.ProbeCountX == 0 || layout.ProbeCountY == 0 ||
       !hasProbeDimensions(screenProbes.GetIrradiance()) ||
       !hasProbeDimensions(screenProbes.GetPositionValidity()) ||
       !hasProbeDimensions(screenProbes.GetNormalDepth()) ||
       !hasProbeDimensions(irradianceHistory.GetCurrent()) ||
       !hasProbeDimensions(positionHistory.GetCurrent()) ||
       !hasProbeDimensions(normalDepthHistory.GetCurrent()))
    {
        throw std::logic_error(
            "Screen Probe Temporal resources have incompatible dimensions.");
    }

    TrTexture& currentIrradiance = screenProbes.GetIrradiance();
    TrTexture& currentPosition = screenProbes.GetPositionValidity();
    TrTexture& currentNormalDepth = screenProbes.GetNormalDepth();
    TrTexture& outputIrradiance = irradianceHistory.GetCurrent();
    TrTexture& outputPosition = positionHistory.GetCurrent();
    TrTexture& outputNormalDepth = normalDepthHistory.GetCurrent();
    TrTexture& previousIrradiance = irradianceHistory.GetPrevious();
    TrTexture& previousPosition = positionHistory.GetPrevious();
    TrTexture& previousNormalDepth = normalDepthHistory.GetPrevious();

    currentIrradiance.Transition(
        commandList,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    currentPosition.Transition(
        commandList,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    currentNormalDepth.Transition(
        commandList,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    previousIrradiance.Transition(
        commandList,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    previousPosition.Transition(
        commandList,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    previousNormalDepth.Transition(
        commandList,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    outputIrradiance.Transition(
        commandList,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    outputPosition.Transition(
        commandList,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    outputNormalDepth.Transition(
        commandList,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    ID3D12DescriptorHeap* descriptorHeaps[] = {resourceHeap.Get()};
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    commandList->SetPipelineState(mPipeline.GetPipelineState());
    commandList->SetComputeRootSignature(mPipeline.GetRootSignature());
    commandList->SetComputeRootDescriptorTable(
        0,
        screenProbes.GetIrradianceSrv().GpuHandle);
    commandList->SetComputeRootDescriptorTable(
        1,
        screenProbes.GetPositionSrv().GpuHandle);
    commandList->SetComputeRootDescriptorTable(
        2,
        screenProbes.GetNormalDepthSrv().GpuHandle);
    commandList->SetComputeRootDescriptorTable(
        3,
        irradianceHistory.GetPreviousSrv().GpuHandle);
    commandList->SetComputeRootDescriptorTable(
        4,
        positionHistory.GetPreviousSrv().GpuHandle);
    commandList->SetComputeRootDescriptorTable(
        5,
        normalDepthHistory.GetPreviousSrv().GpuHandle);
    commandList->SetComputeRootDescriptorTable(
        6,
        irradianceHistory.GetCurrentUav().GpuHandle);
    commandList->SetComputeRootDescriptorTable(
        7,
        positionHistory.GetCurrentUav().GpuHandle);
    commandList->SetComputeRootDescriptorTable(
        8,
        normalDepthHistory.GetCurrentUav().GpuHandle);
    commandList->SetComputeRootConstantBufferView(9, viewConstants);

    TrScreenProbeTemporalConstants constants;
    constants.ProbeCountX = layout.ProbeCountX;
    constants.ProbeCountY = layout.ProbeCountY;
    constants.HistoryValid = screenProbes.IsHistoryValid() ? 1u : 0u;
    constants.FrameNumber = frameNumber;
    commandList->SetComputeRoot32BitConstants(
        10,
        sizeof(constants) / sizeof(std::uint32_t),
        &constants,
        0);
    commandList->Dispatch(
        (layout.ProbeCountX + 7u) / 8u,
        (layout.ProbeCountY + 7u) / 8u,
        1);

    outputIrradiance.UavBarrier(commandList);
    outputPosition.UavBarrier(commandList);
    outputNormalDepth.UavBarrier(commandList);
    outputIrradiance.Transition(
        commandList,
        D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    outputPosition.Transition(
        commandList,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    outputNormalDepth.Transition(
        commandList,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    currentIrradiance.Transition(
        commandList,
        D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    currentPosition.Transition(
        commandList,
        D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    currentNormalDepth.Transition(
        commandList,
        D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

    Outputs outputs;
    outputs.Irradiance = &outputIrradiance;
    outputs.IrradianceSrv = irradianceHistory.GetCurrentSrv().GpuHandle;
    return outputs;
}
