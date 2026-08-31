#include "TrGBufferPass.h"

#include <stdexcept>

void TrGBufferPass::Initialize(
    ID3D12Device* device,
    const std::wstring& shaderPath)
{
    CD3DX12_ROOT_PARAMETER rootParameters[5];
    rootParameters[0].InitAsConstantBufferView(
        TrConstantRegister::View,
        0,
        D3D12_SHADER_VISIBILITY_VERTEX);
    rootParameters[1].InitAsConstantBufferView(
        TrConstantRegister::Pass,
        0,
        D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[2].InitAsConstantBufferView(
        TrConstantRegister::Primitive,
        0,
        D3D12_SHADER_VISIBILITY_VERTEX);
    rootParameters[3].InitAsConstantBufferView(
        TrConstantRegister::Material,
        0,
        D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[4].InitAsConstants(
        sizeof(TrDrawConstants) / sizeof(std::uint32_t),
        TrConstantRegister::Draw,
        0,
        D3D12_SHADER_VISIBILITY_ALL);

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(
        _countof(rootParameters),
        rootParameters,
        0,
        nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    const D3D12_INPUT_ELEMENT_DESC inputElements[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };

    TrGraphicsPipelineDesc pipelineDesc;
    pipelineDesc.ShaderPath = shaderPath;
    pipelineDesc.RootSignatureDesc = &rootSignatureDesc;
    pipelineDesc.InputElements = inputElements;
    pipelineDesc.InputElementCount = _countof(inputElements);
    pipelineDesc.RenderTargetFormats[0] =
        TrDeferredRenderTargets::BaseColorRoughnessFormat;
    pipelineDesc.RenderTargetFormats[1] =
        TrDeferredRenderTargets::NormalMetallicFormat;
    pipelineDesc.RenderTargetCount = 2;
    pipelineDesc.DepthStencilFormat =
        TrDeferredRenderTargets::DepthViewFormat;
    pipelineDesc.CullMode = D3D12_CULL_MODE_NONE;
    mPipeline.Initialize(device, pipelineDesc);
}

void TrGBufferPass::Begin(
    ID3D12GraphicsCommandList* commandList,
    TrDeferredRenderTargets& renderTargets,
    D3D12_GPU_VIRTUAL_ADDRESS viewConstants,
    D3D12_GPU_VIRTUAL_ADDRESS passConstants,
    D3D12_GPU_VIRTUAL_ADDRESS primitiveConstants,
    D3D12_GPU_VIRTUAL_ADDRESS materialConstants,
    const TrDrawConstants& drawConstants)
{
    if(commandList == nullptr || viewConstants == 0 || passConstants == 0 ||
       primitiveConstants == 0 || materialConstants == 0)
    {
        throw std::invalid_argument("GBuffer pass constant buffers are incomplete.");
    }

    commandList->SetPipelineState(mPipeline.GetPipelineState());
    commandList->SetGraphicsRootSignature(mPipeline.GetRootSignature());
    commandList->SetGraphicsRootConstantBufferView(0, viewConstants);
    commandList->SetGraphicsRootConstantBufferView(1, passConstants);
    commandList->SetGraphicsRootConstantBufferView(2, primitiveConstants);
    commandList->SetGraphicsRootConstantBufferView(3, materialConstants);
    commandList->SetGraphicsRoot32BitConstants(
        4,
        sizeof(TrDrawConstants) / sizeof(std::uint32_t),
        &drawConstants,
        0);
    renderTargets.BeginGBufferPass(commandList);
}

void TrGBufferPass::End(
    ID3D12GraphicsCommandList* commandList,
    TrDeferredRenderTargets& renderTargets)
{
    renderTargets.EndGBufferPass(commandList);
}
