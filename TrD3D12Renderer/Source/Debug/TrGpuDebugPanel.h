#pragma once

#include "Resources/TrDescriptorHeap.h"
#include "TrGpuDebug.h"
#include "TrPerformanceMonitor.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

enum class TrGeometryVisualization : std::uint32_t;
class TrRuntimeScene;

enum class TrDebugPanelFeature : std::uint32_t
{
    GeometryView,
    DisplaySettings,
    Performance,
    RuntimeHierarchy,
    SceneSelection
};

struct TrSceneSelectionEntry
{
    std::wstring DisplayName;
    std::wstring Detail;
    std::wstring ScenePath;
};

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
        const TrRuntimeScene& runtimeScene,
        const TrPerformanceSnapshot& performance,
        TrGeometryVisualization& geometryVisualization,
        float& exposure,
        float& depthVisualizationRange,
        const std::vector<TrSceneSelectionEntry>& sceneEntries,
        std::size_t currentSceneIndex,
        std::optional<std::wstring>& sceneChangeRequest);
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
    TrDebugPanelFeature mSelectedFeature =
        TrDebugPanelFeature::GeometryView;
    std::size_t mSelectedSceneIndex = std::numeric_limits<std::size_t>::max();
    bool mShowPerformanceOverlay = true;
    bool mInputValid = true;
    bool mInitialized = false;
};
