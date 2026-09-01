#include "TrCompositePass.h"

#include <stdexcept>

void TrCompositePass::Initialize(
    ID3D12Device* device,
    const std::wstring& shaderPath)
{
    CD3DX12_DESCRIPTOR_RANGE lightingRange;
    lightingRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_ROOT_PARAMETER rootParameters[2];
    rootParameters[0].InitAsConstantBufferView(
        TrConstantRegister::Pass,
        0,
        D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[1].InitAsDescriptorTable(
        1,
        &lightingRange,
        D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(
        _countof(rootParameters),
        rootParameters,
        0,
        nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    TrGraphicsPipelineDesc pipelineDesc;
    pipelineDesc.ShaderPath = shaderPath;
    pipelineDesc.RootSignatureDesc = &rootSignatureDesc;
    pipelineDesc.RenderTargetFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pipelineDesc.RenderTargetCount = 1;
    pipelineDesc.DepthStencilFormat = DXGI_FORMAT_UNKNOWN;
    pipelineDesc.DepthEnabled = false;
    pipelineDesc.CullMode = D3D12_CULL_MODE_NONE;
    mPipeline.Initialize(device, pipelineDesc);
}

void TrCompositePass::Render(
    ID3D12GraphicsCommandList* commandList,
    TrDescriptorHeap& resourceHeap,
    D3D12_GPU_DESCRIPTOR_HANDLE sourceSrv,
    D3D12_GPU_VIRTUAL_ADDRESS compositeConstants,
    TrTexture& backBuffer,
    D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtv)
{
    if(commandList == nullptr || resourceHeap.Get() == nullptr ||
       !resourceHeap.IsShaderVisible() || sourceSrv.ptr == 0 ||
       compositeConstants == 0)
    {
        throw std::invalid_argument("Composite pass inputs are invalid.");
    }

    ID3D12DescriptorHeap* descriptorHeaps[] = {resourceHeap.Get()};
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    commandList->SetPipelineState(mPipeline.GetPipelineState());
    commandList->SetGraphicsRootSignature(mPipeline.GetRootSignature());
    commandList->SetGraphicsRootConstantBufferView(0, compositeConstants);
    commandList->SetGraphicsRootDescriptorTable(1, sourceSrv);

    backBuffer.Transition(commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList->OMSetRenderTargets(1, &backBufferRtv, FALSE, nullptr);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(3, 1, 0, 0);
}
