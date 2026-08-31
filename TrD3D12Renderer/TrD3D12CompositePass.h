#pragma once

#include "TrD3D12DescriptorHeap.h"
#include "TrD3D12GraphicsPipeline.h"
#include "TrD3D12RenderConstants.h"
#include "TrD3D12Texture.h"

class TrD3D12CompositePass
{
public:
    void Initialize(ID3D12Device* device, const std::wstring& shaderPath);
    void Render(
        ID3D12GraphicsCommandList* commandList,
        TrD3D12DescriptorHeap& resourceHeap,
        D3D12_GPU_DESCRIPTOR_HANDLE hdrLightingSrv,
        D3D12_GPU_VIRTUAL_ADDRESS compositeConstants,
        TrD3D12Texture& backBuffer,
        D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtv);

private:
    TrD3D12GraphicsPipeline mPipeline;
};
