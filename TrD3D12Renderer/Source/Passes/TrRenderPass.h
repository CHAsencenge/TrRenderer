#pragma once

#include "Debug/TrPerformanceMonitor.h"

#include <string>
#include <utility>

// Common profiling identity and execution entry for render passes. Calling
// ExecuteProfiled wraps the entire recording callback in one CPU/GPU RAII
// scope, including draw loops between a raster pass's Begin/End methods.
class TrRenderPass
{
public:
    TrRenderPass(const TrRenderPass&) = delete;
    TrRenderPass& operator=(const TrRenderPass&) = delete;

    template<typename TRecordCommands>
    decltype(auto) ExecuteProfiled(
        TrPerformanceMonitor& performanceMonitor,
        ID3D12GraphicsCommandList* commandList,
        TRecordCommands&& recordCommands) const
    {
        TrProfileScope profileScope = Profile(
            performanceMonitor,
            commandList);
        return std::forward<TRecordCommands>(recordCommands)();
    }

    const std::string& GetProfileName() const { return mProfileName; }

protected:
    explicit TrRenderPass(const char* profileName);
    ~TrRenderPass() = default;

private:
    TrProfileScope Profile(
        TrPerformanceMonitor& performanceMonitor,
        ID3D12GraphicsCommandList* commandList) const;

    std::string mProfileName;
    mutable UINT mProfileScopeIndex = UINT_MAX;
};
