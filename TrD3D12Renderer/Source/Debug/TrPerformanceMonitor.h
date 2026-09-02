#pragma once

#include "Utilities/TrUtil.h"

#include <array>
#include <bitset>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

struct TrModuleTiming
{
    std::string Name;
    float CpuMilliseconds = 0.0f;
    float GpuMilliseconds = 0.0f;
    bool CpuValid = false;
    bool GpuValid = false;
};

struct TrPerformanceSnapshot
{
    float FramesPerSecond = 0.0f;
    float FrameTimeMilliseconds = 0.0f;
    std::uint64_t TotalFrameCount = 0;
    bool Valid = false;
    std::vector<TrModuleTiming> Modules;
};

class TrPerformanceMonitor;

// One scope writes a pair of GPU timestamps and measures the CPU time spent
// recording the enclosed commands. Query results are collected only after the
// owning frame fence completes.
class TrProfileScope
{
public:
    TrProfileScope() = default;
    TrProfileScope(const TrProfileScope&) = delete;
    TrProfileScope& operator=(const TrProfileScope&) = delete;
    TrProfileScope(TrProfileScope&& other) noexcept;
    TrProfileScope& operator=(TrProfileScope&&) = delete;
    ~TrProfileScope();

private:
    friend class TrPerformanceMonitor;
    TrProfileScope(
        TrPerformanceMonitor* monitor,
        UINT scopeIndex,
        ID3D12GraphicsCommandList* commandList);

    TrPerformanceMonitor* mMonitor = nullptr;
    ID3D12GraphicsCommandList* mCommandList = nullptr;
    std::chrono::steady_clock::time_point mCpuBegin;
    UINT mScopeIndex = UINT_MAX;
};

// Tracks presentation cadence and per-pass CPU/GPU timings. The GPU path uses
// timestamp queries and a per-frame readback slice, so collecting results never
// introduces an additional GPU wait.
class TrPerformanceMonitor
{
public:
    static constexpr UINT MaxProfileScopes = 32;

    TrPerformanceMonitor();
    ~TrPerformanceMonitor();

    void Initialize(
        ID3D12Device* device,
        ID3D12CommandQueue* commandQueue,
        UINT frameCount);
    void Reset();
    void Tick();
    void BeginFrame(
        UINT frameIndex,
        ID3D12GraphicsCommandList* commandList);
    void EndFrame(ID3D12GraphicsCommandList* commandList);
    void CollectFrame(UINT frameIndex);

    UINT RegisterScope(const char* name);
    TrProfileScope Profile(
        UINT scopeIndex,
        ID3D12GraphicsCommandList* commandList);

    const TrPerformanceSnapshot& GetSnapshot() const { return mSnapshot; }

private:
    friend class TrProfileScope;
    using Clock = std::chrono::steady_clock;

    static constexpr double UpdateIntervalSeconds = 0.5;
    static constexpr double DiscontinuitySeconds = 1.0;
    static constexpr float TimingSmoothingFactor = 0.2f;

    struct TrFrameProfileData
    {
        std::array<double, MaxProfileScopes> CpuMilliseconds = {};
        std::bitset<MaxProfileScopes> CpuActive;
        std::bitset<MaxProfileScopes> GpuActive;
        bool Submitted = false;
    };

    void BeginScope(
        UINT scopeIndex,
        ID3D12GraphicsCommandList* commandList);
    void EndScope(
        UINT scopeIndex,
        ID3D12GraphicsCommandList* commandList,
        Clock::time_point cpuBegin) noexcept;
    UINT GetQueryIndex(UINT frameIndex, UINT scopeIndex, bool end) const;
    UINT64 GetReadbackOffset(UINT frameIndex, UINT scopeIndex) const;
    void UpdateModuleTiming(
        TrModuleTiming& timing,
        const TrFrameProfileData& frame,
        UINT scopeIndex,
        UINT frameIndex);

    Clock::time_point mLastFrameTime;
    Clock::time_point mFrameCpuBegin;
    double mAccumulatedSeconds = 0.0;
    std::uint32_t mAccumulatedFrames = 0;
    std::vector<std::string> mScopeNames;
    std::vector<TrFrameProfileData> mFrames;
    Microsoft::WRL::ComPtr<ID3D12QueryHeap> mTimestampQueryHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> mTimestampReadback;
    const UINT64* mMappedTimestamps = nullptr;
    ID3D12GraphicsCommandList* mActiveCommandList = nullptr;
    UINT64 mTimestampFrequency = 0;
    UINT mFrameCount = 0;
    UINT mActiveFrameIndex = UINT_MAX;
    UINT mFrameScopeIndex = UINT_MAX;
    TrPerformanceSnapshot mSnapshot;
    bool mFrameRateInitialized = false;
    bool mGpuInitialized = false;
};
