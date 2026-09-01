#pragma once

#include "TrShaderCompiler.h"

struct TrComputePipelineDesc
{
    std::wstring ShaderPath;
    std::wstring EntryPoint = L"CSMain";
    const D3D12_ROOT_SIGNATURE_DESC* RootSignatureDesc = nullptr;
    std::vector<TrShaderDefine> ShaderDefines;
};

class TrComputePipeline
{
public:
    void Initialize(ID3D12Device* device, const TrComputePipelineDesc& desc);

    ID3D12RootSignature* GetRootSignature() const { return mRootSignature.Get(); }
    ID3D12PipelineState* GetPipelineState() const { return mPipelineState.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mPipelineState;
};
