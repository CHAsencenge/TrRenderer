#pragma once

#include "TrD3D12Util.h"

struct TrD3D12ComputePipelineDesc
{
    std::wstring ShaderPath;
    std::wstring EntryPoint = L"CSMain";
    const D3D12_ROOT_SIGNATURE_DESC* RootSignatureDesc = nullptr;
};

class TrD3D12ComputePipeline
{
public:
    void Initialize(ID3D12Device* device, const TrD3D12ComputePipelineDesc& desc);

    ID3D12RootSignature* GetRootSignature() const { return mRootSignature.Get(); }
    ID3D12PipelineState* GetPipelineState() const { return mPipelineState.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mPipelineState;
};
