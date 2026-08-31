#include "TrD3D12ComputePipeline.h"
#include "TrD3D12ShaderCompiler.h"

#include <stdexcept>

void TrD3D12ComputePipeline::Initialize(
    ID3D12Device* device,
    const TrD3D12ComputePipelineDesc& desc)
{
    if(device == nullptr || desc.RootSignatureDesc == nullptr ||
       desc.ShaderPath.empty() || desc.EntryPoint.empty())
    {
        throw std::invalid_argument("Incomplete compute pipeline description.");
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
        OutputDebugStringA(
            static_cast<const char*>(rootSignatureErrors->GetBufferPointer()));
    }
    ThrowIfFailed(serializeResult);

    ThrowIfFailed(device->CreateRootSignature(
        0,
        serializedRootSignature->GetBufferPointer(),
        serializedRootSignature->GetBufferSize(),
        IID_PPV_ARGS(&mRootSignature)));

    const Microsoft::WRL::ComPtr<IDxcBlob> computeShader =
        TrD3D12ShaderCompiler::Compile(
            desc.ShaderPath,
            desc.EntryPoint.c_str(),
            L"cs_6_5");

    D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineDesc = {};
    pipelineDesc.pRootSignature = mRootSignature.Get();
    pipelineDesc.CS = CD3DX12_SHADER_BYTECODE(
        computeShader->GetBufferPointer(),
        computeShader->GetBufferSize());
    ThrowIfFailed(device->CreateComputePipelineState(
        &pipelineDesc,
        IID_PPV_ARGS(&mPipelineState)));
}
