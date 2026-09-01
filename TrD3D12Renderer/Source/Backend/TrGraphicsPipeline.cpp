#include "TrGraphicsPipeline.h"
#include "TrLog.h"
#include "TrShaderCompiler.h"

#include <stdexcept>

void TrGraphicsPipeline::Initialize(
    ID3D12Device* device,
    const TrGraphicsPipelineDesc& desc)
{
    if(device == nullptr || desc.RootSignatureDesc == nullptr ||
       (desc.InputElementCount > 0 && desc.InputElements == nullptr) ||
       desc.RenderTargetCount > D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT ||
       desc.ShaderPath.empty())
    {
        throw std::invalid_argument("Incomplete graphics pipeline description.");
    }

    Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSignature;
    Microsoft::WRL::ComPtr<ID3DBlob> rootSignatureErrors;
    const HRESULT serializeResult = D3D12SerializeRootSignature(
        desc.RootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &serializedRootSignature,
        &rootSignatureErrors);
    if(rootSignatureErrors != nullptr)
    {
        const char* message = static_cast<const char*>(
            rootSignatureErrors->GetBufferPointer());
        if(FAILED(serializeResult))
        {
            TrLog::Error(message);
        }
        else
        {
            TrLog::Warn(message);
        }
    }
    ThrowIfFailed(serializeResult);

    ThrowIfFailed(device->CreateRootSignature(
        0,
        serializedRootSignature->GetBufferPointer(),
        serializedRootSignature->GetBufferSize(),
        IID_PPV_ARGS(&mRootSignature)));

    const Microsoft::WRL::ComPtr<IDxcBlob> vertexShader =
        TrShaderCompiler::Compile(
            desc.ShaderPath,
            L"VSMain",
            L"vs_6_5",
            desc.ShaderDefines);
    const Microsoft::WRL::ComPtr<IDxcBlob> pixelShader =
        TrShaderCompiler::Compile(
            desc.ShaderPath,
            L"PSMain",
            L"ps_6_5",
            desc.ShaderDefines);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = {};
    pipelineDesc.InputLayout = {desc.InputElements, desc.InputElementCount};
    pipelineDesc.pRootSignature = mRootSignature.Get();
    pipelineDesc.VS = CD3DX12_SHADER_BYTECODE(
        vertexShader->GetBufferPointer(),
        vertexShader->GetBufferSize());
    pipelineDesc.PS = CD3DX12_SHADER_BYTECODE(
        pixelShader->GetBufferPointer(),
        pixelShader->GetBufferSize());
    pipelineDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    if(desc.AlphaBlendEnabled)
    {
        D3D12_RENDER_TARGET_BLEND_DESC& blend =
            pipelineDesc.BlendState.RenderTarget[0];
        blend.BlendEnable = TRUE;
        blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        blend.BlendOp = D3D12_BLEND_OP_ADD;
        blend.SrcBlendAlpha = D3D12_BLEND_ONE;
        blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }
    pipelineDesc.SampleMask = UINT_MAX;
    pipelineDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    pipelineDesc.RasterizerState.CullMode = desc.CullMode;
    pipelineDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    pipelineDesc.DepthStencilState.DepthEnable = desc.DepthEnabled;
    pipelineDesc.DepthStencilState.DepthFunc = desc.DepthFunc;
    pipelineDesc.DepthStencilState.DepthWriteMask = desc.DepthWriteEnabled
        ? D3D12_DEPTH_WRITE_MASK_ALL
        : D3D12_DEPTH_WRITE_MASK_ZERO;
    pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineDesc.NumRenderTargets = desc.RenderTargetCount;
    for(UINT index = 0; index < desc.RenderTargetCount; ++index)
    {
        pipelineDesc.RTVFormats[index] = desc.RenderTargetFormats[index];
    }
    pipelineDesc.DSVFormat = desc.DepthStencilFormat;
    pipelineDesc.SampleDesc.Count = 1;
    ThrowIfFailed(device->CreateGraphicsPipelineState(
        &pipelineDesc,
        IID_PPV_ARGS(&mPipelineState)));
}
