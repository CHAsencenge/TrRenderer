#include "TrHzbPass.h"

#include "Renderer/TrRenderConfig.h"
#include "Renderer/TrRenderConstants.h"

#include <stdexcept>

void TrHzbPass::Initialize(
    ID3D12Device* device,
    const std::wstring& shaderPath)
{
    CD3DX12_DESCRIPTOR_RANGE sourceRange;
    sourceRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE destinationRange;
    destinationRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

    CD3DX12_ROOT_PARAMETER rootParameters[3];
    rootParameters[0].InitAsDescriptorTable(1, &sourceRange);
    rootParameters[1].InitAsDescriptorTable(1, &destinationRange);
    rootParameters[2].InitAsConstants(
        sizeof(TrHzbBuildConstants) / sizeof(std::uint32_t),
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

TrHzbPass::Outputs TrHzbPass::Build(
    ID3D12GraphicsCommandList* commandList,
    TrDescriptorHeap& resourceHeap,
    const Inputs& inputs,
    TrHierarchicalDepth& hierarchicalDepth)
{
    if(commandList == nullptr || resourceHeap.Get() == nullptr ||
       !resourceHeap.IsShaderVisible())
    {
        throw std::invalid_argument("HZB pass inputs are incomplete.");
    }
    inputs.DepthNormal.ValidateForCompute();

    const TrHierarchicalDepthDesc& description =
        hierarchicalDepth.GetDescription();
    if(description.Width != inputs.DepthNormal.GetWidth() ||
       description.Height != inputs.DepthNormal.GetHeight() ||
       description.MipCount == 0)
    {
        throw std::logic_error("HZB dimensions do not match the source depth.");
    }

    ID3D12DescriptorHeap* descriptorHeaps[] = {resourceHeap.Get()};
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    commandList->SetPipelineState(mPipeline.GetPipelineState());
    commandList->SetComputeRootSignature(mPipeline.GetRootSignature());

    TrTexture& texture = hierarchicalDepth.GetTexture();
    for(UINT destinationMip = 0;
        destinationMip < description.MipCount;
        ++destinationMip)
    {
        const UINT sourceWidth = destinationMip == 0
            ? inputs.DepthNormal.GetWidth()
            : hierarchicalDepth.GetMipWidth(destinationMip - 1);
        const UINT sourceHeight = destinationMip == 0
            ? inputs.DepthNormal.GetHeight()
            : hierarchicalDepth.GetMipHeight(destinationMip - 1);
        const UINT destinationWidth =
            hierarchicalDepth.GetMipWidth(destinationMip);
        const UINT destinationHeight =
            hierarchicalDepth.GetMipHeight(destinationMip);
        const D3D12_GPU_DESCRIPTOR_HANDLE sourceSrv = destinationMip == 0
            ? inputs.DepthNormal.GetDepthSrv().GpuHandle
            : hierarchicalDepth.GetMipSrv(destinationMip - 1).GpuHandle;

        texture.Transition(
            commandList,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            destinationMip);
        commandList->SetComputeRootDescriptorTable(0, sourceSrv);
        commandList->SetComputeRootDescriptorTable(
            1,
            hierarchicalDepth.GetMipUav(destinationMip).GpuHandle);
        const TrHzbBuildConstants constants =
        {
            sourceWidth,
            sourceHeight,
            destinationWidth,
            destinationHeight
        };
        commandList->SetComputeRoot32BitConstants(
            2,
            sizeof(constants) / sizeof(std::uint32_t),
            &constants,
            0);
        commandList->Dispatch(
            (destinationWidth + 7) / 8,
            (destinationHeight + 7) / 8,
            1);

        texture.UavBarrier(commandList);
        texture.Transition(
            commandList,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            destinationMip);
    }

    Outputs outputs;
    outputs.HierarchicalDepth = &hierarchicalDepth;
    return outputs;
}
