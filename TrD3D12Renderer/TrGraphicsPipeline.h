#pragma once

#include "TrUtil.h"

struct TrGraphicsPipelineDesc
{
    std::wstring ShaderPath;
    const D3D12_ROOT_SIGNATURE_DESC* RootSignatureDesc = nullptr;
    const D3D12_INPUT_ELEMENT_DESC* InputElements = nullptr;
    UINT InputElementCount = 0;
    DXGI_FORMAT RenderTargetFormats[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT] =
    {
        DXGI_FORMAT_R8G8B8A8_UNORM
    };
    UINT RenderTargetCount = 1;
    DXGI_FORMAT DepthStencilFormat = DXGI_FORMAT_D32_FLOAT;
    D3D12_CULL_MODE CullMode = D3D12_CULL_MODE_BACK;
    bool DepthEnabled = true;
};

class TrGraphicsPipeline
{
public:
    void Initialize(ID3D12Device* device, const TrGraphicsPipelineDesc& desc);

    ID3D12RootSignature* GetRootSignature() const { return mRootSignature.Get(); }
    ID3D12PipelineState* GetPipelineState() const { return mPipelineState.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mPipelineState;
};
