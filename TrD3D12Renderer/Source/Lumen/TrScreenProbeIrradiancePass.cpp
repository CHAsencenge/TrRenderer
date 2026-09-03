#include "TrScreenProbeIrradiancePass.h"

#include "Renderer/TrRenderConstants.h"

#include <stdexcept>

void TrScreenProbeIrradiancePass::Initialize(
    ID3D12Device* device,
    const std::wstring& shaderPath)
{
    CD3DX12_DESCRIPTOR_RANGE radianceRange;
    radianceRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE normalDepthRange;
    normalDepthRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    CD3DX12_DESCRIPTOR_RANGE irradianceRange;
    irradianceRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

    CD3DX12_ROOT_PARAMETER rootParameters[4];
    rootParameters[0].InitAsDescriptorTable(1, &radianceRange);
    rootParameters[1].InitAsDescriptorTable(1, &normalDepthRange);
    rootParameters[2].InitAsDescriptorTable(1, &irradianceRange);
    rootParameters[3].InitAsConstants(
        sizeof(TrScreenProbeIrradianceConstants) / sizeof(std::uint32_t),
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
    mPipeline.Initialize(device, pipelineDesc);
}

TrScreenProbeIrradiancePass::Outputs
TrScreenProbeIrradiancePass::Integrate(
    ID3D12GraphicsCommandList* commandList,
    TrDescriptorHeap& resourceHeap,
    UINT frameNumber,
    TrScreenProbeResources& screenProbes)
{
    if(commandList == nullptr || resourceHeap.Get() == nullptr ||
       !resourceHeap.IsShaderVisible())
    {
        throw std::invalid_argument(
            "Screen Probe Irradiance pass inputs are invalid.");
    }

    const TrScreenProbeLayout& layout = screenProbes.GetLayout();
    const D3D12_RESOURCE_DESC& radianceDescription =
        screenProbes.GetRadiance().GetDescription();
    const D3D12_RESOURCE_DESC& irradianceDescription =
        screenProbes.GetIrradiance().GetDescription();
    if(radianceDescription.Width != layout.TraceAtlasWidth ||
       radianceDescription.Height != layout.TraceAtlasHeight ||
       irradianceDescription.Width != layout.IrradianceAtlasWidth ||
       irradianceDescription.Height != layout.IrradianceAtlasHeight)
    {
        throw std::logic_error(
            "Screen Probe Irradiance resources have incompatible dimensions.");
    }

    TrTexture& radiance = screenProbes.GetRadiance();
    TrTexture& normalDepth = screenProbes.GetNormalDepth();
    TrTexture& irradiance = screenProbes.GetIrradiance();
    radiance.Transition(
        commandList,
        D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    normalDepth.Transition(
        commandList,
        D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    irradiance.Transition(
        commandList,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    ID3D12DescriptorHeap* descriptorHeaps[] = {resourceHeap.Get()};
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    commandList->SetPipelineState(mPipeline.GetPipelineState());
    commandList->SetComputeRootSignature(mPipeline.GetRootSignature());
    commandList->SetComputeRootDescriptorTable(
        0,
        screenProbes.GetRadianceSrv().GpuHandle);
    commandList->SetComputeRootDescriptorTable(
        1,
        screenProbes.GetNormalDepthSrv().GpuHandle);
    commandList->SetComputeRootDescriptorTable(
        2,
        screenProbes.GetIrradianceUav().GpuHandle);

    const TrScreenProbeIrradianceConstants constants =
    {
        layout.ProbeCountX,
        layout.ProbeCountY,
        TrScreenProbeLayout::RayGridDimension,
        TrScreenProbeLayout::RaysPerProbe,
        frameNumber
    };
    commandList->SetComputeRoot32BitConstants(
        3,
        sizeof(constants) / sizeof(std::uint32_t),
        &constants,
        0);
    commandList->Dispatch(
        (layout.ProbeCountX + 7u) / 8u,
        (layout.ProbeCountY + 7u) / 8u,
        1);

    irradiance.UavBarrier(commandList);
    irradiance.Transition(
        commandList,
        D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

    Outputs outputs;
    outputs.Irradiance = &irradiance;
    return outputs;
}
