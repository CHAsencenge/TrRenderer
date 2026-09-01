#include "TrDepthNormalPass.h"

#include "Renderer/TrRenderConfig.h"

#include <stdexcept>

void TrDepthNormalPass::Initialize(
    ID3D12Device* device,
    const std::wstring& shaderPath)
{
    CD3DX12_DESCRIPTOR_RANGE textureRange;
    textureRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 0);
    CD3DX12_DESCRIPTOR_RANGE samplerRange;
    samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 5, 0);

    CD3DX12_ROOT_PARAMETER rootParameters[6];
    rootParameters[0].InitAsConstantBufferView(
        TrConstantRegister::View,
        0,
        D3D12_SHADER_VISIBILITY_VERTEX);
    rootParameters[1].InitAsConstantBufferView(
        TrConstantRegister::Primitive,
        0,
        D3D12_SHADER_VISIBILITY_ALL);
    rootParameters[2].InitAsConstantBufferView(
        TrConstantRegister::Material,
        0,
        D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[3].InitAsConstants(
        sizeof(TrDrawConstants) / sizeof(std::uint32_t),
        TrConstantRegister::Draw,
        0,
        D3D12_SHADER_VISIBILITY_ALL);
    rootParameters[4].InitAsDescriptorTable(
        1,
        &textureRange,
        D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[5].InitAsDescriptorTable(
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
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };

    TrGraphicsPipelineDesc pipelineDesc;
    pipelineDesc.ShaderPath = shaderPath;
    pipelineDesc.RootSignatureDesc = &rootSignatureDesc;
    pipelineDesc.InputElements = inputElements;
    pipelineDesc.InputElementCount = _countof(inputElements);
    pipelineDesc.RenderTargetFormats[0] =
        TrDeferredRenderTargets::NormalMetallicFormat;
    pipelineDesc.RenderTargetCount = 1;
    pipelineDesc.DepthStencilFormat =
        TrDeferredRenderTargets::DepthStencilViewFormat;
    pipelineDesc.CullMode = D3D12_CULL_MODE_NONE;
    pipelineDesc.DepthFunc = TrRenderConfig::DepthComparison;
    pipelineDesc.DepthWriteEnabled = true;
    pipelineDesc.ShaderDefines = TrRenderConfig::GetDepthShaderDefines();
    mPipeline.Initialize(device, pipelineDesc);
}

void TrDepthNormalPass::Begin(
    ID3D12GraphicsCommandList* commandList,
    TrDeferredRenderTargets& renderTargets,
    TrDescriptorHeap& resourceHeap,
    TrDescriptorHeap& samplerHeap,
    D3D12_GPU_VIRTUAL_ADDRESS viewConstants)
{
    if(commandList == nullptr || resourceHeap.Get() == nullptr ||
       samplerHeap.Get() == nullptr || !resourceHeap.IsShaderVisible() ||
       !samplerHeap.IsShaderVisible() || viewConstants == 0)
    {
        throw std::invalid_argument("Depth/Normal pass inputs are incomplete.");
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
    renderTargets.BeginDepthNormalPass(commandList);
}

void TrDepthNormalPass::SetDrawBindings(
    ID3D12GraphicsCommandList* commandList,
    D3D12_GPU_VIRTUAL_ADDRESS primitiveConstants,
    D3D12_GPU_VIRTUAL_ADDRESS materialConstants,
    D3D12_GPU_DESCRIPTOR_HANDLE textureTable,
    D3D12_GPU_DESCRIPTOR_HANDLE samplerTable,
    const TrDrawConstants& drawConstants)
{
    if(commandList == nullptr || primitiveConstants == 0 ||
       materialConstants == 0 || textureTable.ptr == 0 || samplerTable.ptr == 0)
    {
        throw std::invalid_argument("Depth/Normal draw bindings are incomplete.");
    }

    commandList->SetGraphicsRootConstantBufferView(1, primitiveConstants);
    commandList->SetGraphicsRootConstantBufferView(2, materialConstants);
    commandList->SetGraphicsRoot32BitConstants(
        3,
        sizeof(TrDrawConstants) / sizeof(std::uint32_t),
        &drawConstants,
        0);
    commandList->SetGraphicsRootDescriptorTable(4, textureTable);
    commandList->SetGraphicsRootDescriptorTable(5, samplerTable);
}

void TrDepthNormalPass::End(
    ID3D12GraphicsCommandList* commandList,
    TrDeferredRenderTargets& renderTargets)
{
    renderTargets.EndDepthNormalPass(commandList);
}
