#pragma once

#include "Resources/TrDescriptorHeap.h"
#include "Backend/TrGraphicsPipeline.h"
#include "Passes/TrRenderPass.h"
#include "Renderer/TrRenderConstants.h"
#include "Resources/TrTexture.h"

class TrCompositePass : public TrRenderPass
{
public:
    TrCompositePass() : TrRenderPass("Composite") {}

    void Initialize(ID3D12Device* device, const std::wstring& shaderPath);
    void Render(
        ID3D12GraphicsCommandList* commandList,
        TrDescriptorHeap& resourceHeap,
        D3D12_GPU_DESCRIPTOR_HANDLE sourceSrv,
        D3D12_GPU_VIRTUAL_ADDRESS compositeConstants,
        TrTexture& backBuffer,
        D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtv);

private:
    TrGraphicsPipeline mPipeline;
};
