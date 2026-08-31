#pragma once

#include "TrDescriptorHeap.h"
#include "TrGpuDebug.h"

#include <cstddef>
#include <string>

class TrGpuDebugPanel
{
public:
    void Initialize(
        HWND parent,
        ID3D12Device* device,
        UINT frameCount,
        DXGI_FORMAT renderTargetFormat,
        TrDescriptorHeap& resourceHeap,
        float exposure,
        float depthVisualizationRange);
    void Shutdown();

    // Starts and completes the ImGui CPU frame. Returns true when a debug view
    // changed and the native window title should be refreshed.
    bool BuildFrame(
        TrGpuDebug& gpuDebug,
        float& exposure,
        float& depthVisualizationRange);
    void Render(
        ID3D12GraphicsCommandList* commandList,
        TrDescriptorHeap& resourceHeap) const;

    bool IsInitialized() const { return mInitialized; }

private:
    static bool TryParseFloat(
        const char* text,
        float minimum,
        float maximum,
        float& value);
    static void SetFloatText(char* destination, std::size_t size, float value);
    static std::string ToUtf8(const std::wstring& text);

    TrDescriptorAllocation mFontDescriptor;
    char mExposureText[32] = {};
    char mDepthRangeText[32] = {};
    std::string mStatus = "Ready";
    bool mInputValid = true;
    bool mInitialized = false;
};
