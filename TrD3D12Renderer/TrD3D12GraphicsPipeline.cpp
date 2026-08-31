#include "TrD3D12GraphicsPipeline.h"

#include <stdexcept>

void TrD3D12GraphicsPipeline::Initialize(
    ID3D12Device* device,
    const TrD3D12GraphicsPipelineDesc& desc)
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
        OutputDebugStringA(static_cast<const char*>(rootSignatureErrors->GetBufferPointer()));
    }
    ThrowIfFailed(serializeResult);

    ThrowIfFailed(device->CreateRootSignature(
        0,
        serializedRootSignature->GetBufferPointer(),
        serializedRootSignature->GetBufferSize(),
        IID_PPV_ARGS(&mRootSignature)));

    const Microsoft::WRL::ComPtr<ID3DBlob> vertexShader =
        CompileShader(desc.ShaderPath, "VSMain", "vs_5_0");
    const Microsoft::WRL::ComPtr<ID3DBlob> pixelShader =
        CompileShader(desc.ShaderPath, "PSMain", "ps_5_0");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = {};
    pipelineDesc.InputLayout = {desc.InputElements, desc.InputElementCount};
    pipelineDesc.pRootSignature = mRootSignature.Get();
    pipelineDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
    pipelineDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
    pipelineDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    pipelineDesc.SampleMask = UINT_MAX;
    pipelineDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    pipelineDesc.RasterizerState.CullMode = desc.CullMode;
    pipelineDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    pipelineDesc.DepthStencilState.DepthEnable = desc.DepthEnabled;
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

Microsoft::WRL::ComPtr<ID3DBlob> TrD3D12GraphicsPipeline::CompileShader(
    const std::wstring& filename,
    const char* entryPoint,
    const char* target)
{
#if defined(_DEBUG)
    const UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    const UINT compileFlags = 0;
#endif

    Microsoft::WRL::ComPtr<ID3DBlob> shader;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    const HRESULT compileResult = D3DCompileFromFile(
        filename.c_str(),
        nullptr,
        nullptr,
        entryPoint,
        target,
        compileFlags,
        0,
        &shader,
        &errors);
    if(errors != nullptr)
    {
        OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer()));
    }
    ThrowIfFailed(compileResult);
    return shader;
}
