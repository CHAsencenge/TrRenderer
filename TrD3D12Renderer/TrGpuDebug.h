#pragma once

#include "TrUtil.h"

#include <cstdint>
#include <string>
#include <vector>

// Values are shared with composite.hlsl.
enum class TrDebugVisualization : std::uint32_t
{
    HdrColor = 0,
    LinearColor = 1,
    WorldNormal = 2,
    ScalarRed = 3,
    ScalarGreen = 4,
    ScalarBlue = 5,
    ScalarAlpha = 6,
    DeviceDepth = 7
};

struct TrGpuDebugView
{
    std::wstring Name;
    D3D12_GPU_DESCRIPTOR_HANDLE SourceSrv = {};
    TrDebugVisualization Visualization = TrDebugVisualization::LinearColor;
};

class TrGpuDebug
{
public:
    static constexpr UINT MaxViews = 10;

    void Reset();
    void RegisterView(
        const wchar_t* name,
        D3D12_GPU_DESCRIPTOR_HANDLE sourceSrv,
        TrDebugVisualization visualization);

    bool SelectView(UINT index);

    const TrGpuDebugView& GetSelectedView() const;
    const TrGpuDebugView& GetView(UINT index) const;
    UINT GetSelectedIndex() const { return mSelectedIndex; }
    UINT GetViewCount() const { return static_cast<UINT>(mViews.size()); }

private:
    std::vector<TrGpuDebugView> mViews;
    UINT mSelectedIndex = 0;
};
