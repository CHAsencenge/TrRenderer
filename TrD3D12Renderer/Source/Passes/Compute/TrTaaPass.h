#pragma once

#include "Backend/TrComputePipeline.h"
#include "Passes/TrRenderPass.h"
#include "Resources/TrDescriptorHeap.h"
#include "Resources/TrHistoryTexture.h"
#include "Resources/TrTexture.h"

#include <DirectXMath.h>

struct alignas(16) TrTaaConstants
{
    UINT Width = 0;
    UINT Height = 0;
    UINT HistoryValid = 0;
    UINT FrameNumber = 0;
    DirectX::XMFLOAT2 CurrentJitterNdc = {0.0f, 0.0f};
    DirectX::XMFLOAT2 PreviousJitterNdc = {0.0f, 0.0f};
    float StaticHistoryWeight = 0.9f;
    float MotionHistoryReduction = 1.0f / 16.0f;
    float RelativeDepthThreshold = 0.02f;
    float MinimumDepthThreshold = 0.05f;
    float NearPlane = 0.1f;
    float FarPlane = 100.0f;
    DirectX::XMFLOAT2 Padding = {0.0f, 0.0f};
};

static_assert(sizeof(TrTaaConstants) == 64);

struct TrTaaInputs
{
    TrTexture* CurrentColor = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE CurrentColorSrv = {};
    TrTexture* Velocity = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE VelocitySrv = {};
    TrTexture* Depth = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE DepthSrv = {};
};

class TrTaaPass : public TrRenderPass
{
public:
    TrTaaPass() : TrRenderPass("TAA") {}

    void Initialize(ID3D12Device* device, const std::wstring& shaderPath);

    D3D12_GPU_DESCRIPTOR_HANDLE Resolve(
        ID3D12GraphicsCommandList* commandList,
        TrDescriptorHeap& resourceHeap,
        const TrTaaInputs& inputs,
        const TrTaaConstants& constants,
        TrHistoryTexture& history);

private:
    TrComputePipeline mPipeline;
};
