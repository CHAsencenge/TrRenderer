#include "TrComputePipeline.h"
#include "TrLog.h"
#include "TrShaderCompiler.h"

#include <stdexcept>

void TrComputePipeline::Initialize(
    ID3D12Device* device,
    const TrComputePipelineDesc& desc)
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

    const Microsoft::WRL::ComPtr<IDxcBlob> computeShader =
        TrShaderCompiler::Compile(
            desc.ShaderPath,
            desc.EntryPoint.c_str(),
            L"cs_6_5",
            desc.ShaderDefines);

    D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineDesc = {};
    pipelineDesc.pRootSignature = mRootSignature.Get();
    pipelineDesc.CS = CD3DX12_SHADER_BYTECODE(
        computeShader->GetBufferPointer(),
        computeShader->GetBufferSize());
    ThrowIfFailed(device->CreateComputePipelineState(
        &pipelineDesc,
        IID_PPV_ARGS(&mPipelineState)));
}
