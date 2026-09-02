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
    CD3DX12_DESCRIPTOR_RANGE historyRange;
    historyRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    CD3DX12_DESCRIPTOR_RANGE velocityRange;
    velocityRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
    CD3DX12_DESCRIPTOR_RANGE depthRange;
    depthRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);
    CD3DX12_DESCRIPTOR_RANGE outputRange;
    outputRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

    CD3DX12_ROOT_PARAMETER rootParameters[6];
    rootParameters[0].InitAsDescriptorTable(1, &currentColorRange);
    rootParameters[1].InitAsDescriptorTable(1, &historyRange);
    rootParameters[2].InitAsDescriptorTable(1, &velocityRange);
    rootParameters[3].InitAsDescriptorTable(1, &depthRange);
    rootParameters[4].InitAsDescriptorTable(1, &outputRange);
    rootParameters[5].InitAsConstants(
        sizeof(TrTaaConstants) / sizeof(std::uint32_t),
        TrConstantRegister::Pass);

    D3D12_STATIC_SAMPLER_DESC linearClampSampler = {};
    linearClampSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    linearClampSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    linearClampSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    linearClampSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    linearClampSampler.MipLODBias = 0.0f;
    linearClampSampler.MaxAnisotropy = 1;
    linearClampSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    linearClampSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
    linearClampSampler.MinLOD = 0.0f;
    linearClampSampler.MaxLOD = D3D12_FLOAT32_MAX;
    linearClampSampler.ShaderRegister = 0;
    linearClampSampler.RegisterSpace = 0;
    linearClampSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(
        _countof(rootParameters),
        rootParameters,
        1,
        &linearClampSampler,
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
    TrHistoryTexture& history)
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
    const D3D12_RESOURCE_DESC& outputDesc =
        history.GetCurrent().GetDescription();
    if(colorDesc.Width != constants.Width || colorDesc.Height != constants.Height ||
       velocityDesc.Width != constants.Width || velocityDesc.Height != constants.Height ||
       depthDesc.Width != constants.Width || depthDesc.Height != constants.Height ||
       outputDesc.Width != constants.Width || outputDesc.Height != constants.Height)
    {
        throw std::logic_error("TAA input dimensions do not match the output.");
    }

    TrTexture& output = history.GetCurrent();
    TrTexture& previous = history.GetPrevious();
    output.Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    previous.Transition(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
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
    commandList->SetComputeRootDescriptorTable(1, history.GetPreviousSrv().GpuHandle);
    commandList->SetComputeRootDescriptorTable(2, inputs.VelocitySrv);
    commandList->SetComputeRootDescriptorTable(3, inputs.DepthSrv);
    commandList->SetComputeRootDescriptorTable(4, history.GetCurrentUav().GpuHandle);
    commandList->SetComputeRoot32BitConstants(
        5,
        sizeof(TrTaaConstants) / sizeof(std::uint32_t),
        &constants,
        0);
    commandList->Dispatch(
        (constants.Width + 7u) / 8u,
        (constants.Height + 7u) / 8u,
        1);

    output.UavBarrier(commandList);
    output.Transition(commandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    inputs.CurrentColor->Transition(
        commandList,
        D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    inputs.Velocity->Transition(
        commandList,
        D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    inputs.Depth->Transition(
        commandList,
        D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    return history.GetCurrentSrv().GpuHandle;
}
