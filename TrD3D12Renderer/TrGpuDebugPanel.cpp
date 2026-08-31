#include "TrGpuDebugPanel.h"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#include "imgui.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"

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
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
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
    float& exposure,
    float& depthVisualizationRange)
{
    if(!IsInitialized() || gpuDebug.GetViewCount() == 0)
    {
        throw std::logic_error("GPU debug panel has not been initialized.");
    }

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();
    constexpr float panelWidth = 260.0f;
    constexpr float panelMargin = 12.0f;
    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x - panelWidth - panelMargin, panelMargin),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelWidth, 0.0f), ImGuiCond_Always);

    bool selectedViewChanged = false;
    const ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings;
    if(ImGui::Begin("GPU Debug", nullptr, windowFlags))
    {
        ImGui::TextUnformatted("Visualization");
        for(UINT index = 0; index < gpuDebug.GetViewCount(); ++index)
        {
            const bool selected = index == gpuDebug.GetSelectedIndex();
            if(selected)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            }

            const std::string label = ToUtf8(gpuDebug.GetView(index).Name) +
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

        ImGui::Separator();
        ImGui::TextUnformatted("Exposure (0.01 - 32)");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##Exposure", mExposureText, sizeof(mExposureText));
        ImGui::TextUnformatted("Depth Range (0.1 - 10000)");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##DepthRange", mDepthRangeText, sizeof(mDepthRangeText));

        if(ImGui::Button("Apply"))
        {
            float newExposure = exposure;
            float newDepthRange = depthVisualizationRange;
            if(TryParseFloat(mExposureText, 0.01f, 32.0f, newExposure) &&
               TryParseFloat(mDepthRangeText, 0.1f, 10000.0f, newDepthRange))
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
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.3f, 1.0f));
        }
        ImGui::TextUnformatted(mStatus.c_str());
        if(!mInputValid)
        {
            ImGui::PopStyleColor();
        }
    }
    ImGui::End();
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
