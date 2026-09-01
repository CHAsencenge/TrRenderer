#include "TrForwardTransparentPass.h"

#include "Renderer/TrRenderConfig.h"

#include <stdexcept>

void TrForwardTransparentPass::Initialize(
    ID3D12Device* device,
    const std::wstring& shaderPath)
{
    CD3DX12_DESCRIPTOR_RANGE textureRange;
    textureRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 0);
    CD3DX12_DESCRIPTOR_RANGE samplerRange;
    samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 5, 0);

    CD3DX12_ROOT_PARAMETER rootParameters[8];
    rootParameters[0].InitAsConstantBufferView(
        TrConstantRegister::Scene,
        0,
        D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[1].InitAsConstantBufferView(
        TrConstantRegister::View,
        0,
        D3D12_SHADER_VISIBILITY_ALL);
    rootParameters[2].InitAsConstantBufferView(
        TrConstantRegister::Pass,
        0,
        D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[3].InitAsConstantBufferView(
        TrConstantRegister::Primitive,
        0,
        D3D12_SHADER_VISIBILITY_ALL);
    rootParameters[4].InitAsConstantBufferView(
        TrConstantRegister::Material,
        0,
        D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[5].InitAsConstants(
        sizeof(TrDrawConstants) / sizeof(std::uint32_t),
        TrConstantRegister::Draw,
        0,
        D3D12_SHADER_VISIBILITY_ALL);
    rootParameters[6].InitAsDescriptorTable(
        1,
        &textureRange,
        D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[7].InitAsDescriptorTable(
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
        TrDeferredRenderTargets::HdrLightingFormat;
    pipelineDesc.RenderTargetCount = 1;
    pipelineDesc.DepthStencilFormat =
        TrDeferredRenderTargets::DepthStencilViewFormat;
    pipelineDesc.CullMode = D3D12_CULL_MODE_NONE;
    pipelineDesc.DepthFunc = TrRenderConfig::DepthComparison;
    pipelineDesc.DepthWriteEnabled = false;
    pipelineDesc.AlphaBlendEnabled = true;
    pipelineDesc.ShaderDefines = TrRenderConfig::GetDepthShaderDefines();
    mPipeline.Initialize(device, pipelineDesc);
}

void TrForwardTransparentPass::Begin(
    ID3D12GraphicsCommandList* commandList,
    TrDeferredRenderTargets& renderTargets,
    TrDescriptorHeap& resourceHeap,
    TrDescriptorHeap& samplerHeap,
    D3D12_GPU_VIRTUAL_ADDRESS sceneConstants,
    D3D12_GPU_VIRTUAL_ADDRESS viewConstants,
    D3D12_GPU_VIRTUAL_ADDRESS passConstants)
{
    if(commandList == nullptr || resourceHeap.Get() == nullptr ||
       samplerHeap.Get() == nullptr || !resourceHeap.IsShaderVisible() ||
       !samplerHeap.IsShaderVisible() || sceneConstants == 0 ||
       viewConstants == 0 || passConstants == 0)
    {
        throw std::invalid_argument("Forward transparent pass inputs are incomplete.");
    }

    ID3D12DescriptorHeap* descriptorHeaps[] =
    {
        resourceHeap.Get(),
        samplerHeap.Get()
    };
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    commandList->SetPipelineState(mPipeline.GetPipelineState());
    commandList->SetGraphicsRootSignature(mPipeline.GetRootSignature());
    commandList->SetGraphicsRootConstantBufferView(0, sceneConstants);
    commandList->SetGraphicsRootConstantBufferView(1, viewConstants);
    commandList->SetGraphicsRootConstantBufferView(2, passConstants);
    renderTargets.BeginForwardTransparentPass(commandList);
}

void TrForwardTransparentPass::SetDrawBindings(
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
        throw std::invalid_argument(
            "Forward transparent draw bindings are incomplete.");
    }

    commandList->SetGraphicsRootConstantBufferView(3, primitiveConstants);
    commandList->SetGraphicsRootConstantBufferView(4, materialConstants);
    commandList->SetGraphicsRoot32BitConstants(
        5,
        sizeof(TrDrawConstants) / sizeof(std::uint32_t),
        &drawConstants,
        0);
    commandList->SetGraphicsRootDescriptorTable(6, textureTable);
    commandList->SetGraphicsRootDescriptorTable(7, samplerTable);
}

void TrForwardTransparentPass::End(
    ID3D12GraphicsCommandList* commandList,
    TrDeferredRenderTargets& renderTargets)
{
    renderTargets.EndForwardTransparentPass(commandList);
}
