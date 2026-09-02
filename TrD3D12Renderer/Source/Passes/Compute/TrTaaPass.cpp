#include "TrTaaPass.h"

#include "Renderer/TrRenderConfig.h"
#include "Renderer/TrRenderConstants.h"

#include <stdexcept>

void TrTaaPass::Initialize(
    ID3D12Device* device,
    const std::wstring& shaderPath)
{
    CD3DX12_DESCRIPTOR_RANGE currentColorRange;
    currentColorRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE colorHistoryRange;
    colorHistoryRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    CD3DX12_DESCRIPTOR_RANGE velocityRange;
    velocityRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
    CD3DX12_DESCRIPTOR_RANGE depthRange;
    depthRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);
    CD3DX12_DESCRIPTOR_RANGE depthHistoryRange;
    depthHistoryRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4);
    CD3DX12_DESCRIPTOR_RANGE colorOutputRange;
    colorOutputRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE depthOutputRange;
    depthOutputRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);

    CD3DX12_ROOT_PARAMETER rootParameters[8];
    rootParameters[0].InitAsDescriptorTable(1, &currentColorRange);
    rootParameters[1].InitAsDescriptorTable(1, &colorHistoryRange);
    rootParameters[2].InitAsDescriptorTable(1, &velocityRange);
    rootParameters[3].InitAsDescriptorTable(1, &depthRange);
    rootParameters[4].InitAsDescriptorTable(1, &depthHistoryRange);
    rootParameters[5].InitAsDescriptorTable(1, &colorOutputRange);
    rootParameters[6].InitAsDescriptorTable(1, &depthOutputRange);
    rootParameters[7].InitAsConstants(
        sizeof(TrTaaConstants) / sizeof(std::uint32_t),
        TrConstantRegister::Pass);

    D3D12_STATIC_SAMPLER_DESC samplers[2] = {};
    samplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].MipLODBias = 0.0f;
    samplers[0].MaxAnisotropy = 1;
    samplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    samplers[0].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
    samplers[0].MinLOD = 0.0f;
    samplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    samplers[0].ShaderRegister = 0;
    samplers[0].RegisterSpace = 0;
    samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    samplers[1] = samplers[0];
    samplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    samplers[1].ShaderRegister = 1;

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(
        _countof(rootParameters),
        rootParameters,
        _countof(samplers),
        samplers,
        D3D12_ROOT_SIGNATURE_FLAG_NONE);

    TrComputePipelineDesc pipelineDesc;
    pipelineDesc.ShaderPath = shaderPath;
    pipelineDesc.RootSignatureDesc = &rootSignatureDesc;
    pipelineDesc.ShaderDefines = TrRenderConfig::GetDepthShaderDefines();
    mPipeline.Initialize(device, pipelineDesc);
}

D3D12_GPU_DESCRIPTOR_HANDLE TrTaaPass::Resolve(
    ID3D12GraphicsCommandList* commandList,
    TrDescriptorHeap& resourceHeap,
    const TrTaaInputs& inputs,
    const TrTaaConstants& constants,
    TrHistoryTexture& colorHistory,
    TrHistoryTexture& depthHistory)
{
    if(commandList == nullptr || resourceHeap.Get() == nullptr ||
       !resourceHeap.IsShaderVisible() || inputs.CurrentColor == nullptr ||
       inputs.Velocity == nullptr || inputs.Depth == nullptr ||
       inputs.CurrentColorSrv.ptr == 0 || inputs.VelocitySrv.ptr == 0 ||
       inputs.DepthSrv.ptr == 0 || constants.Width == 0 || constants.Height == 0)
    {
        throw std::invalid_argument("TAA resolve inputs are incomplete.");
    }

    const D3D12_RESOURCE_DESC& colorDesc = inputs.CurrentColor->GetDescription();
    const D3D12_RESOURCE_DESC& velocityDesc = inputs.Velocity->GetDescription();
    const D3D12_RESOURCE_DESC& depthDesc = inputs.Depth->GetDescription();
    const D3D12_RESOURCE_DESC& colorOutputDesc =
        colorHistory.GetCurrent().GetDescription();
    const D3D12_RESOURCE_DESC& depthOutputDesc =
        depthHistory.GetCurrent().GetDescription();
    if(colorDesc.Width != constants.Width || colorDesc.Height != constants.Height ||
       velocityDesc.Width != constants.Width || velocityDesc.Height != constants.Height ||
       depthDesc.Width != constants.Width || depthDesc.Height != constants.Height ||
       colorOutputDesc.Width != constants.Width ||
       colorOutputDesc.Height != constants.Height ||
       depthOutputDesc.Width != constants.Width ||
       depthOutputDesc.Height != constants.Height)
    {
        throw std::logic_error("TAA input dimensions do not match the output.");
    }
    if(colorHistory.IsValid() != depthHistory.IsValid())
    {
        throw std::logic_error("TAA color and depth histories are out of sync.");
    }

    TrTexture& colorOutput = colorHistory.GetCurrent();
    TrTexture& previousColor = colorHistory.GetPrevious();
    TrTexture& depthOutput = depthHistory.GetCurrent();
    TrTexture& previousDepth = depthHistory.GetPrevious();
    colorOutput.Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    previousColor.Transition(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    depthOutput.Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    previousDepth.Transition(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    inputs.CurrentColor->Transition(
        commandList,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    inputs.Velocity->Transition(
        commandList,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    inputs.Depth->Transition(
        commandList,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    ID3D12DescriptorHeap* descriptorHeaps[] = {resourceHeap.Get()};
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    commandList->SetPipelineState(mPipeline.GetPipelineState());
    commandList->SetComputeRootSignature(mPipeline.GetRootSignature());
    commandList->SetComputeRootDescriptorTable(0, inputs.CurrentColorSrv);
    commandList->SetComputeRootDescriptorTable(
        1,
        colorHistory.GetPreviousSrv().GpuHandle);
    commandList->SetComputeRootDescriptorTable(2, inputs.VelocitySrv);
    commandList->SetComputeRootDescriptorTable(3, inputs.DepthSrv);
    commandList->SetComputeRootDescriptorTable(
        4,
        depthHistory.GetPreviousSrv().GpuHandle);
    commandList->SetComputeRootDescriptorTable(
        5,
        colorHistory.GetCurrentUav().GpuHandle);
    commandList->SetComputeRootDescriptorTable(
        6,
        depthHistory.GetCurrentUav().GpuHandle);
    commandList->SetComputeRoot32BitConstants(
        7,
        sizeof(TrTaaConstants) / sizeof(std::uint32_t),
        &constants,
        0);
    commandList->Dispatch(
        (constants.Width + 7u) / 8u,
        (constants.Height + 7u) / 8u,
        1);

    colorOutput.UavBarrier(commandList);
    depthOutput.UavBarrier(commandList);
    colorOutput.Transition(commandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    depthOutput.Transition(commandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    inputs.CurrentColor->Transition(
        commandList,
        D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    inputs.Velocity->Transition(
        commandList,
        D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    inputs.Depth->Transition(
        commandList,
        D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    return colorHistory.GetCurrentSrv().GpuHandle;
}
