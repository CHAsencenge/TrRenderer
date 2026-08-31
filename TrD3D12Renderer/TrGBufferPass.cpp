#include "TrGBufferPass.h"

#include <stdexcept>

void TrGBufferPass::Initialize(
    ID3D12Device* device,
    const std::wstring& shaderPath)
{
    CD3DX12_DESCRIPTOR_RANGE textureRange;
    textureRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 0);
    CD3DX12_DESCRIPTOR_RANGE samplerRange;
    samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 5, 0);

    CD3DX12_ROOT_PARAMETER rootParameters[7];
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
    rootParameters[5].InitAsDescriptorTable(
        1,
        &textureRange,
        D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[6].InitAsDescriptorTable(
        1,
        &samplerRange,
        D3D12_SHADER_VISIBILITY_PIXEL);

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
        {"COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
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
    pipelineDesc.RenderTargetFormats[2] =
        TrDeferredRenderTargets::EmissiveOcclusionFormat;
    pipelineDesc.RenderTargetCount = 3;
    pipelineDesc.DepthStencilFormat =
        TrDeferredRenderTargets::DepthViewFormat;
    pipelineDesc.CullMode = D3D12_CULL_MODE_NONE;
    mPipeline.Initialize(device, pipelineDesc);
}

void TrGBufferPass::Begin(
    ID3D12GraphicsCommandList* commandList,
    TrDeferredRenderTargets& renderTargets,
    TrDescriptorHeap& resourceHeap,
    TrDescriptorHeap& samplerHeap,
    D3D12_GPU_VIRTUAL_ADDRESS viewConstants,
    D3D12_GPU_VIRTUAL_ADDRESS passConstants,
    D3D12_GPU_VIRTUAL_ADDRESS primitiveConstants)
{
    if(commandList == nullptr || resourceHeap.Get() == nullptr ||
       samplerHeap.Get() == nullptr || !resourceHeap.IsShaderVisible() ||
       !samplerHeap.IsShaderVisible() || viewConstants == 0 ||
       passConstants == 0 || primitiveConstants == 0)
    {
        throw std::invalid_argument("GBuffer pass inputs are incomplete.");
    }

    ID3D12DescriptorHeap* descriptorHeaps[] =
    {
        resourceHeap.Get(),
        samplerHeap.Get()
    };
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    commandList->SetPipelineState(mPipeline.GetPipelineState());
    commandList->SetGraphicsRootSignature(mPipeline.GetRootSignature());
    commandList->SetGraphicsRootConstantBufferView(0, viewConstants);
    commandList->SetGraphicsRootConstantBufferView(1, passConstants);
    commandList->SetGraphicsRootConstantBufferView(2, primitiveConstants);
    renderTargets.BeginGBufferPass(commandList);
}

void TrGBufferPass::SetMaterialAndDrawConstants(
    ID3D12GraphicsCommandList* commandList,
    D3D12_GPU_VIRTUAL_ADDRESS materialConstants,
    D3D12_GPU_DESCRIPTOR_HANDLE textureTable,
    D3D12_GPU_DESCRIPTOR_HANDLE samplerTable,
    const TrDrawConstants& drawConstants)
{
    if(commandList == nullptr || materialConstants == 0 ||
       textureTable.ptr == 0 || samplerTable.ptr == 0)
    {
        throw std::invalid_argument("GBuffer material bindings are incomplete.");
    }

    commandList->SetGraphicsRootConstantBufferView(3, materialConstants);
    commandList->SetGraphicsRoot32BitConstants(
        4,
        sizeof(TrDrawConstants) / sizeof(std::uint32_t),
        &drawConstants,
        0);
    commandList->SetGraphicsRootDescriptorTable(5, textureTable);
    commandList->SetGraphicsRootDescriptorTable(6, samplerTable);
}

void TrGBufferPass::End(
    ID3D12GraphicsCommandList* commandList,
    TrDeferredRenderTargets& renderTargets)
{
    renderTargets.EndGBufferPass(commandList);
}
