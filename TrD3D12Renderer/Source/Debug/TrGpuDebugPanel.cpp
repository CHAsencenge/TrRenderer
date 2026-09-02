#include "TrGpuDebugPanel.h"

#include "Renderer/TrRenderConstants.h"
#include "Scene/TrRuntimeScene.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#include "imgui.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"

namespace
{
    const char* GetGeometryVisualizationName(TrGeometryVisualization visualization)
    {
        switch(visualization)
        {
        case TrGeometryVisualization::Shaded:
            return "Shaded";
        case TrGeometryVisualization::Hierarchy:
            return "Hierarchy";
        case TrGeometryVisualization::PrimitiveDraw:
            return "Primitive Draw";
        }
        return "Unknown";
    }

    const char* GetFeatureName(TrDebugPanelFeature feature)
    {
        switch(feature)
        {
        case TrDebugPanelFeature::GeometryView:
            return "Geometry View";
        case TrDebugPanelFeature::DisplaySettings:
            return "Display Settings";
        case TrDebugPanelFeature::PipelineFeatures:
            return "Pipeline Features";
        case TrDebugPanelFeature::Performance:
            return "Performance";
        case TrDebugPanelFeature::RuntimeHierarchy:
            return "Runtime Hierarchy";
        case TrDebugPanelFeature::SceneSelection:
            return "Scene Selection";
        }
        return "Unknown";
    }

    bool DrawGeometryVisualizationButton(
        const char* label,
        TrGeometryVisualization value,
        TrGeometryVisualization& selected)
    {
        const bool isSelected = value == selected;
        if(isSelected)
        {
            ImGui::PushStyleColor(
                ImGuiCol_Button,
                ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }
        const bool clicked = ImGui::Button(label, ImVec2(-1.0f, 0.0f));
        if(isSelected)
        {
            ImGui::PopStyleColor();
        }
        if(clicked)
        {
            selected = value;
        }
        return clicked && !isSelected;
    }

    void DrawRuntimeNode(
        const TrRuntimeScene& runtimeScene,
        TrNodeId nodeId)
    {
        const TrRuntimeNode& node = runtimeScene.GetNode(nodeId);
        if(!node.Active)
        {
            return;
        }

        const TrRuntimeInstance* instance = runtimeScene.FindInstanceByNode(nodeId);
        std::size_t activeChildCount = 0;
        for(const TrNodeId childId : node.Children)
        {
            activeChildCount += runtimeScene.GetNode(childId).Active ? 1u : 0u;
        }
        const bool hasDetails = instance != nullptr || activeChildCount != 0;

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
        if(node.HierarchyDepth < 3)
        {
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }
        if(!hasDetails)
        {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }

        const std::string visibleName = node.Name.empty() ? "Unnamed Node" : node.Name;
        const std::string label = visibleName + "  [N:" +
            std::to_string(node.NodeId) + " D:" +
            std::to_string(node.HierarchyDepth) + "]";
        const bool open = ImGui::TreeNodeEx(
            reinterpret_cast<const void*>(static_cast<std::uintptr_t>(nodeId) + 1u),
            flags,
            "%s",
            label.c_str());
        if(!open || !hasDetails)
        {
            return;
        }

        if(instance != nullptr)
        {
            const std::string parentLabel = node.ParentNodeId == TrInvalidRuntimeId
                ? "Root"
                : std::to_string(node.ParentNodeId);
            ImGui::TextDisabled(
                "Instance %u | Mesh %u | Parent %s",
                instance->InstanceId,
                instance->MeshId,
                parentLabel.c_str());
            const TrRuntimeMesh& mesh = runtimeScene.GetMesh(instance->MeshId);
            for(const TrRuntimePrimitive& primitive : mesh.Primitives)
            {
                if(primitive.MaterialId == TrInvalidRuntimeId)
                {
                    ImGui::BulletText(
                        "Draw P:%u Local:%u Material:none",
                        primitive.PrimitiveId,
                        primitive.LocalPrimitiveIndex);
                }
                else
                {
                    ImGui::BulletText(
                        "Draw P:%u Local:%u Material:%u",
                        primitive.PrimitiveId,
                        primitive.LocalPrimitiveIndex,
                        primitive.MaterialId);
                }
            }
        }
        for(const TrNodeId childId : node.Children)
        {
            DrawRuntimeNode(runtimeScene, childId);
        }
        ImGui::TreePop();
    }
}

void TrGpuDebugPanel::Initialize(
    HWND parent,
    ID3D12Device* device,
    UINT frameCount,
    DXGI_FORMAT renderTargetFormat,
    TrDescriptorHeap& resourceHeap,
    float exposure,
    float depthVisualizationRange)
{
    if(parent == nullptr || device == nullptr || frameCount == 0 ||
       renderTargetFormat == DXGI_FORMAT_UNKNOWN || resourceHeap.Get() == nullptr ||
       !resourceHeap.IsShaderVisible())
    {
        throw std::invalid_argument("GPU debug panel inputs are invalid.");
    }
    if(IsInitialized())
    {
        throw std::logic_error("GPU debug panel has already been initialized.");
    }

    mFontDescriptor = resourceHeap.Allocate();
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |=
        ImGuiConfigFlags_NavEnableKeyboard |
        ImGuiConfigFlags_NavNoCaptureKeyboard;
    ImGui::StyleColorsDark();

    if(!ImGui_ImplWin32_Init(parent))
    {
        ImGui::DestroyContext();
        throw std::runtime_error("Failed to initialize the ImGui Win32 backend.");
    }
    if(!ImGui_ImplDX12_Init(
           device,
           static_cast<int>(frameCount),
           renderTargetFormat,
           resourceHeap.Get(),
           mFontDescriptor.CpuHandle,
           mFontDescriptor.GpuHandle))
    {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        throw std::runtime_error("Failed to initialize the ImGui DX12 backend.");
    }

    SetFloatText(mExposureText, sizeof(mExposureText), exposure);
    SetFloatText(mDepthRangeText, sizeof(mDepthRangeText), depthVisualizationRange);
    mInitialized = true;
}

void TrGpuDebugPanel::Shutdown()
{
    if(!IsInitialized())
    {
        return;
    }

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    mInitialized = false;
}

bool TrGpuDebugPanel::BuildFrame(
    TrGpuDebug& gpuDebug,
    const TrRuntimeScene& runtimeScene,
    const TrPerformanceSnapshot& performance,
    TrPipelineFeatures& pipelineFeatures,
    TrGeometryVisualization& geometryVisualization,
    float& exposure,
    float& depthVisualizationRange,
    const std::vector<TrSceneSelectionEntry>& sceneEntries,
    std::size_t currentSceneIndex,
    std::optional<std::wstring>& sceneChangeRequest)
{
    if(!IsInitialized() || gpuDebug.GetViewCount() == 0)
    {
        throw std::logic_error("GPU debug panel has not been initialized.");
    }

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();
    constexpr float panelWidth = 420.0f;
    constexpr float panelMargin = 12.0f;
    constexpr float performanceOverlayReservedHeight = 102.0f;
    const float panelTop = panelMargin +
        (mShowPerformanceOverlay ? performanceOverlayReservedHeight : 0.0f);
    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x - panelWidth - panelMargin, panelTop),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        ImVec2(
            panelWidth,
            std::max(io.DisplaySize.y - panelTop - panelMargin, 320.0f)),
        ImGuiCond_Always);

    bool selectedViewChanged = false;
    const ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings;
    if(ImGui::Begin("Renderer Tools", nullptr, windowFlags))
    {
        ImGui::TextUnformatted("Functions");
        const float categorySpacing = ImGui::GetStyle().ItemSpacing.x;
        const float categoryWidth =
            (ImGui::GetContentRegionAvail().x - categorySpacing) * 0.5f;
        if(ImGui::Button("Rendering  v", ImVec2(categoryWidth, 0.0f)))
        {
            ImGui::OpenPopup("RenderingFunctions");
        }
        if(ImGui::BeginPopup("RenderingFunctions"))
        {
            if(ImGui::MenuItem(
                   "Geometry View",
                   nullptr,
                   mSelectedFeature == TrDebugPanelFeature::GeometryView))
            {
                mSelectedFeature = TrDebugPanelFeature::GeometryView;
            }
            if(ImGui::MenuItem(
                   "Display Settings",
                   nullptr,
                   mSelectedFeature == TrDebugPanelFeature::DisplaySettings))
            {
                mSelectedFeature = TrDebugPanelFeature::DisplaySettings;
            }
            if(ImGui::MenuItem(
                   "Pipeline Features",
                   nullptr,
                   mSelectedFeature == TrDebugPanelFeature::PipelineFeatures))
            {
                mSelectedFeature = TrDebugPanelFeature::PipelineFeatures;
            }
            if(ImGui::MenuItem(
                   "Performance",
                   nullptr,
                   mSelectedFeature == TrDebugPanelFeature::Performance))
            {
                mSelectedFeature = TrDebugPanelFeature::Performance;
            }
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        if(ImGui::Button("Scene  v", ImVec2(categoryWidth, 0.0f)))
        {
            ImGui::OpenPopup("SceneFunctions");
        }
        if(ImGui::BeginPopup("SceneFunctions"))
        {
            if(ImGui::MenuItem(
                   "Runtime Hierarchy",
                   nullptr,
                   mSelectedFeature == TrDebugPanelFeature::RuntimeHierarchy))
            {
                mSelectedFeature = TrDebugPanelFeature::RuntimeHierarchy;
            }
            if(ImGui::MenuItem(
                   "Scene Selection",
                   nullptr,
                   mSelectedFeature == TrDebugPanelFeature::SceneSelection))
            {
                mSelectedFeature = TrDebugPanelFeature::SceneSelection;
            }
            ImGui::EndPopup();
        }

        ImGui::Text("Selected: %s", GetFeatureName(mSelectedFeature));
        ImGui::TextDisabled("Camera: WASD move | Hold RMB and drag to look");
        const float featureHeight = std::clamp(
            io.DisplaySize.y * 0.30f,
            190.0f,
            300.0f);
        if(ImGui::BeginChild(
               "FeatureWorkspace",
               ImVec2(0.0f, featureHeight),
               true))
        {
            if(mSelectedFeature == TrDebugPanelFeature::GeometryView)
            {
                ImGui::TextUnformatted("Final Geometry View");
                bool geometryViewChanged = DrawGeometryVisualizationButton(
                    "Shaded",
                    TrGeometryVisualization::Shaded,
                    geometryVisualization);
                geometryViewChanged |= DrawGeometryVisualizationButton(
                    "Hierarchy (parent + depth)",
                    TrGeometryVisualization::Hierarchy,
                    geometryVisualization);
                geometryViewChanged |= DrawGeometryVisualizationButton(
                    "Primitive Draw (primitive + instance)",
                    TrGeometryVisualization::PrimitiveDraw,
                    geometryVisualization);
                selectedViewChanged |= geometryViewChanged;
                if(geometryViewChanged)
                {
                    mStatus = GetGeometryVisualizationName(
                        geometryVisualization);
                    mInputValid = true;
                }
                if(geometryVisualization != TrGeometryVisualization::Shaded)
                {
                    selectedViewChanged |= gpuDebug.SelectView(0);
                }
                ImGui::TextWrapped(
                    geometryVisualization == TrGeometryVisualization::Hierarchy
                        ? "Similar hues share a parent; brightness encodes hierarchy depth."
                        : geometryVisualization == TrGeometryVisualization::PrimitiveDraw
                            ? "Every PrimitiveID + InstanceID draw receives a distinct color."
                            : "Materials and lighting are shown normally.");
            }
            else if(mSelectedFeature == TrDebugPanelFeature::DisplaySettings)
            {
                ImGui::Checkbox(
                    "Show Performance Overlay",
                    &mShowPerformanceOverlay);
                ImGui::TextDisabled(
                    "FPS is averaged over the latest 0.5 seconds.");
                ImGui::Separator();
                ImGui::TextUnformatted("Exposure (0.01 - 32)");
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::InputText(
                    "##Exposure",
                    mExposureText,
                    sizeof(mExposureText));
                ImGui::TextUnformatted("Depth Range (0.1 - 10000)");
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::InputText(
                    "##DepthRange",
                    mDepthRangeText,
                    sizeof(mDepthRangeText));

                if(ImGui::Button("Apply"))
                {
                    float newExposure = exposure;
                    float newDepthRange = depthVisualizationRange;
                    if(TryParseFloat(
                           mExposureText,
                           0.01f,
                           32.0f,
                           newExposure) &&
                       TryParseFloat(
                           mDepthRangeText,
                           0.1f,
                           10000.0f,
                           newDepthRange))
                    {
                        exposure = newExposure;
                        depthVisualizationRange = newDepthRange;
                        mStatus = "Applied";
                        mInputValid = true;
                    }
                    else
                    {
                        mStatus = "Invalid value";
                        mInputValid = false;
                    }
                }

                ImGui::SameLine();
                if(!mInputValid)
                {
                    ImGui::PushStyleColor(
                        ImGuiCol_Text,
                        ImVec4(1.0f, 0.35f, 0.3f, 1.0f));
                }
                ImGui::TextUnformatted(mStatus.c_str());
                if(!mInputValid)
                {
                    ImGui::PopStyleColor();
                }
            }
            else if(mSelectedFeature == TrDebugPanelFeature::PipelineFeatures)
            {
                ImGui::TextUnformatted("Runtime Pipeline Features");
                ImGui::TextDisabled(
                    "Changes apply to the command list recorded this frame.");
                ImGui::Separator();

                bool indirectLighting = pipelineFeatures.IsEnabled(
                    TrPipelineFeature::IndirectLighting);
                if(ImGui::Checkbox(
                       "Screen Probe Indirect Lighting",
                       &indirectLighting))
                {
                    pipelineFeatures.SetEnabled(
                        TrPipelineFeature::IndirectLighting,
                        indirectLighting);
                }
                ImGui::TextWrapped(
                    "Disabling this skips Screen Probe placement, tracing, "
                    "radiance resolve, irradiance integration and temporal resolve. "
                    "Direct and ambient lighting remain enabled.");
            }
            else if(mSelectedFeature == TrDebugPanelFeature::Performance)
            {
                ImGui::TextUnformatted("Render Pass Timings");
                ImGui::TextDisabled(
                    "CPU = command recording; GPU = timestamp interval.");
                ImGui::TextDisabled(
                    "GPU samples arrive after the frame fence completes.");
                ImGui::Separator();

                constexpr ImGuiTableFlags tableFlags =
                    ImGuiTableFlags_BordersInnerV |
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_SizingStretchProp |
                    ImGuiTableFlags_ScrollY;
                if(ImGui::BeginTable(
                       "PassTimingTable",
                       3,
                       tableFlags,
                       ImVec2(0.0f, 0.0f)))
                {
                    ImGui::TableSetupColumn(
                        "Module",
                        ImGuiTableColumnFlags_WidthStretch,
                        2.2f);
                    ImGui::TableSetupColumn(
                        "CPU ms",
                        ImGuiTableColumnFlags_WidthStretch,
                        1.0f);
                    ImGui::TableSetupColumn(
                        "GPU ms",
                        ImGuiTableColumnFlags_WidthStretch,
                        1.0f);
                    ImGui::TableHeadersRow();

                    for(const TrModuleTiming& timing : performance.Modules)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted(timing.Name.c_str());
                        ImGui::TableSetColumnIndex(1);
                        if(timing.CpuValid)
                        {
                            ImGui::Text("%.3f", timing.CpuMilliseconds);
                        }
                        else
                        {
                            ImGui::TextUnformatted("--");
                        }
                        ImGui::TableSetColumnIndex(2);
                        if(timing.GpuValid)
                        {
                            ImGui::Text("%.3f", timing.GpuMilliseconds);
                        }
                        else
                        {
                            ImGui::TextUnformatted("--");
                        }
                    }
                    ImGui::EndTable();
                }
            }
            else if(mSelectedFeature == TrDebugPanelFeature::RuntimeHierarchy)
            {
                ImGui::Text(
                    "Runtime Hierarchy | %s",
                    GetGeometryVisualizationName(geometryVisualization));
                for(const TrNodeId rootNodeId :
                    runtimeScene.GetSourceScene().RootNodes)
                {
                    DrawRuntimeNode(runtimeScene, rootNodeId);
                }
            }
            else
            {
                if(mSelectedSceneIndex >= sceneEntries.size())
                {
                    mSelectedSceneIndex = currentSceneIndex < sceneEntries.size()
                        ? currentSceneIndex
                        : 0;
                }

                ImGui::TextUnformatted("Available Scenes");
                if(currentSceneIndex < sceneEntries.size())
                {
                    ImGui::TextWrapped(
                        "Current: %s",
                        ToUtf8(sceneEntries[currentSceneIndex].DisplayName).c_str());
                }
                ImGui::Separator();

                const float loadButtonHeight = ImGui::GetFrameHeightWithSpacing();
                if(ImGui::BeginChild(
                       "SceneList",
                       ImVec2(0.0f, -loadButtonHeight),
                       false))
                {
                    for(std::size_t index = 0; index < sceneEntries.size(); ++index)
                    {
                        ImGui::PushID(static_cast<int>(index));
                        const TrSceneSelectionEntry& entry = sceneEntries[index];
                        const std::string displayName = ToUtf8(entry.DisplayName);
                        if(ImGui::Selectable(
                               displayName.c_str(),
                               mSelectedSceneIndex == index))
                        {
                            mSelectedSceneIndex = index;
                        }
                        if(index == currentSceneIndex)
                        {
                            ImGui::SameLine();
                            ImGui::TextDisabled("(loaded)");
                        }
                        if(ImGui::IsItemHovered() && !entry.Detail.empty())
                        {
                            ImGui::SetTooltip("%s", ToUtf8(entry.Detail).c_str());
                        }
                        ImGui::PopID();
                    }
                }
                ImGui::EndChild();

                const bool canLoad =
                    mSelectedSceneIndex < sceneEntries.size() &&
                    mSelectedSceneIndex != currentSceneIndex;
                if(!canLoad)
                {
                    ImGui::BeginDisabled();
                }
                if(ImGui::Button("Load Selected Scene", ImVec2(-1.0f, 0.0f)) &&
                   canLoad)
                {
                    sceneChangeRequest =
                        sceneEntries[mSelectedSceneIndex].ScenePath;
                }
                if(!canLoad)
                {
                    ImGui::EndDisabled();
                }
            }
        }
        ImGui::EndChild();

        ImGui::Separator();
        ImGui::Text("GPU Visualization | %s", ToUtf8(
            gpuDebug.GetSelectedView().Name).c_str());
        if(ImGui::BeginChild(
               "GpuVisualizationWorkspace",
               ImVec2(0.0f, 0.0f),
               true))
        {
            const bool geometryViewLocksFinalLighting =
                geometryVisualization != TrGeometryVisualization::Shaded;
            if(geometryViewLocksFinalLighting)
            {
                ImGui::TextWrapped(
                    "Geometry diagnostics lock the output to Final Lighting.");
                ImGui::BeginDisabled();
            }
            for(UINT index = 0; index < gpuDebug.GetViewCount(); ++index)
            {
                const bool selected = index == gpuDebug.GetSelectedIndex();
                if(selected)
                {
                    ImGui::PushStyleColor(
                        ImGuiCol_Button,
                        ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                }

                const std::string label = ToUtf8(
                    gpuDebug.GetView(index).Name) +
                    "##View" + std::to_string(index);
                if(ImGui::Button(label.c_str(), ImVec2(-1.0f, 0.0f)))
                {
                    selectedViewChanged |= gpuDebug.SelectView(index);
                    mStatus = ToUtf8(gpuDebug.GetView(index).Name);
                    mInputValid = true;
                }

                if(selected)
                {
                    ImGui::PopStyleColor();
                }
            }
            if(geometryViewLocksFinalLighting)
            {
                ImGui::EndDisabled();
            }
            if(gpuDebug.GetSelectedView().Visualization ==
               TrDebugVisualization::ScreenTrace)
            {
                ImGui::TextWrapped(
                    "Screen Trace: green=hit, red=max distance, "
                    "blue=left screen, yellow=iteration limit, "
                    "black=invalid probe.");
            }
        }
        ImGui::EndChild();
    }
    ImGui::End();

    if(mShowPerformanceOverlay)
    {
        const ImGuiWindowFlags overlayFlags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoInputs;
        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x - panelMargin, panelMargin),
            ImGuiCond_Always,
            ImVec2(1.0f, 0.0f));
        ImGui::SetNextWindowBgAlpha(0.72f);
        if(ImGui::Begin("Performance Overlay", nullptr, overlayFlags))
        {
            if(performance.Valid)
            {
                ImGui::Text("FPS   %6.1f", performance.FramesPerSecond);
                ImGui::Text(
                    "Frame %6.2f ms",
                    performance.FrameTimeMilliseconds);
            }
            else
            {
                ImGui::TextUnformatted("FPS   collecting...");
                ImGui::TextUnformatted("Frame -- ms");
            }

            const TrModuleTiming* frameTiming = performance.Modules.empty()
                ? nullptr
                : &performance.Modules.front();
            if(frameTiming != nullptr && frameTiming->CpuValid)
            {
                ImGui::Text("CPU   %6.2f ms", frameTiming->CpuMilliseconds);
            }
            else
            {
                ImGui::TextUnformatted("CPU      -- ms");
            }
            if(frameTiming != nullptr && frameTiming->GpuValid)
            {
                ImGui::Text("GPU   %6.2f ms", frameTiming->GpuMilliseconds);
            }
            else
            {
                ImGui::TextUnformatted("GPU      -- ms");
            }
        }
        ImGui::End();
    }
    ImGui::Render();

    return selectedViewChanged;
}

void TrGpuDebugPanel::Render(
    ID3D12GraphicsCommandList* commandList,
    TrDescriptorHeap& resourceHeap) const
{
    if(!IsInitialized() || commandList == nullptr || resourceHeap.Get() == nullptr ||
       !resourceHeap.IsShaderVisible())
    {
        throw std::invalid_argument("ImGui render inputs are invalid.");
    }

    ID3D12DescriptorHeap* descriptorHeaps[] = {resourceHeap.Get()};
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}

bool TrGpuDebugPanel::TryParseFloat(
    const char* text,
    float minimum,
    float maximum,
    float& value)
{
    if(text == nullptr || text[0] == '\0')
    {
        return false;
    }

    errno = 0;
    char* end = nullptr;
    const float parsedValue = std::strtof(text, &end);
    while(end != nullptr && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
    {
        ++end;
    }
    if(end == text || end == nullptr || *end != '\0' || errno == ERANGE ||
       !std::isfinite(parsedValue) || parsedValue < minimum || parsedValue > maximum)
    {
        return false;
    }

    value = parsedValue;
    return true;
}

void TrGpuDebugPanel::SetFloatText(
    char* destination,
    std::size_t size,
    float value)
{
    sprintf_s(destination, size, "%.3f", value);
}

std::string TrGpuDebugPanel::ToUtf8(const std::wstring& text)
{
    if(text.empty())
    {
        return {};
    }

    const int byteCount = WideCharToMultiByte(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if(byteCount <= 0)
    {
        throw std::runtime_error("Failed to convert an ImGui label to UTF-8.");
    }

    std::string result(static_cast<std::size_t>(byteCount), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        result.data(),
        byteCount,
        nullptr,
        nullptr);
    return result;
}
