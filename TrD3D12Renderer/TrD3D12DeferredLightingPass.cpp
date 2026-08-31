#include "TrD3D12DeferredLightingPass.h"

#include <stdexcept>

void TrD3D12DeferredLightingPass::Initialize(
    ID3D12Device* device,
    const std::wstring& shaderPath)
{
    CD3DX12_ROOT_PARAMETER rootParameters[4];
    rootParameters[0].InitAsConstantBufferView(
        TrD3D12ConstantRegister::Scene,
        0,
        D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[1].InitAsConstantBufferView(
        TrD3D12ConstantRegister::View,
        0,
        D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[2].InitAsConstantBufferView(
        TrD3D12ConstantRegister::Pass,
        0,
        D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_DESCRIPTOR_RANGE gBufferRange;
    gBufferRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0);
    rootParameters[3].InitAsDescriptorTable(
        1,
        &gBufferRange,
        D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(
        _countof(rootParameters),
        rootParameters,
        0,
        nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    TrD3D12GraphicsPipelineDesc pipelineDesc;
    pipelineDesc.ShaderPath = shaderPath;
    pipelineDesc.RootSignatureDesc = &rootSignatureDesc;
    pipelineDesc.RenderTargetFormats[0] =
        TrD3D12DeferredRenderTargets::HdrLightingFormat;
    pipelineDesc.RenderTargetCount = 1;
    pipelineDesc.DepthStencilFormat = DXGI_FORMAT_UNKNOWN;
    pipelineDesc.DepthEnabled = false;
    pipelineDesc.CullMode = D3D12_CULL_MODE_NONE;
    mPipeline.Initialize(device, pipelineDesc);
}

void TrD3D12DeferredLightingPass::Render(
    ID3D12GraphicsCommandList* commandList,
    TrD3D12DeferredRenderTargets& renderTargets,
    TrD3D12DescriptorHeap& resourceHeap,
    D3D12_GPU_VIRTUAL_ADDRESS sceneConstants,
    D3D12_GPU_VIRTUAL_ADDRESS viewConstants,
    D3D12_GPU_VIRTUAL_ADDRESS passConstants)
{
    if(commandList == nullptr || resourceHeap.Get() == nullptr ||
       !resourceHeap.IsShaderVisible() || sceneConstants == 0 ||
       viewConstants == 0 || passConstants == 0)
    {
        throw std::invalid_argument("Deferred lighting pass inputs are invalid.");
    }

    ID3D12DescriptorHeap* descriptorHeaps[] = {resourceHeap.Get()};
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    commandList->SetPipelineState(mPipeline.GetPipelineState());
    commandList->SetGraphicsRootSignature(mPipeline.GetRootSignature());
    commandList->SetGraphicsRootConstantBufferView(0, sceneConstants);
    commandList->SetGraphicsRootConstantBufferView(1, viewConstants);
    commandList->SetGraphicsRootConstantBufferView(2, passConstants);
    commandList->SetGraphicsRootDescriptorTable(
        3,
        renderTargets.GetBaseColorSrv().GpuHandle);

    renderTargets.BeginDeferredLightingPass(commandList);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(3, 1, 0, 0);
    renderTargets.EndDeferredLightingPass(commandList);
}
