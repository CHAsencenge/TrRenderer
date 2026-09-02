#include "TrGpuDebug.h"

#include <stdexcept>

void TrGpuDebug::Reset()
{
    mViews.clear();
    mSelectedIndex = 0;
}

void TrGpuDebug::RegisterView(
    const wchar_t* name,
    D3D12_GPU_DESCRIPTOR_HANDLE sourceSrv,
    TrDebugVisualization visualization)
{
    if(name == nullptr || name[0] == L'\0' || sourceSrv.ptr == 0)
    {
        throw std::invalid_argument("GPU debug view requires a name and SRV.");
    }
    if(mViews.size() >= MaxViews)
    {
        throw std::length_error("GPU debug view registry capacity has been reached.");
    }

    TrGpuDebugView view;
    view.Name = name;
    view.SourceSrv = sourceSrv;
    view.Visualization = visualization;
    mViews.push_back(view);
}

void TrGpuDebug::UpdateViewSource(
    UINT index,
    D3D12_GPU_DESCRIPTOR_HANDLE sourceSrv)
{
    if(index >= mViews.size() || sourceSrv.ptr == 0)
    {
        throw std::out_of_range(
            "GPU debug view source update is outside the registry.");
    }
    mViews[index].SourceSrv = sourceSrv;
}

bool TrGpuDebug::SelectView(UINT index)
{
    if(index >= mViews.size())
    {
        throw std::out_of_range("GPU debug view index is outside the registry.");
    }

    if(index == mSelectedIndex)
    {
        return false;
    }
    mSelectedIndex = index;
    return true;
}

const TrGpuDebugView& TrGpuDebug::GetSelectedView() const
{
    if(mViews.empty() || mSelectedIndex >= mViews.size())
    {
        throw std::logic_error("No GPU debug view has been selected.");
    }
    return mViews[mSelectedIndex];
}

const TrGpuDebugView& TrGpuDebug::GetView(UINT index) const
{
    if(index >= mViews.size())
    {
        throw std::out_of_range("GPU debug view index is outside the registry.");
    }
    return mViews[index];
}
