#include "TrPerformanceMonitor.h"

#include <stdexcept>
#include <utility>

TrProfileScope::TrProfileScope(
    TrPerformanceMonitor* monitor,
    UINT scopeIndex,
    ID3D12GraphicsCommandList* commandList) :
    mMonitor(monitor),
    mCommandList(commandList),
    mCpuBegin(std::chrono::steady_clock::now()),
    mScopeIndex(scopeIndex)
{
    mMonitor->BeginScope(mScopeIndex, mCommandList);
}

TrProfileScope::TrProfileScope(TrProfileScope&& other) noexcept :
    mMonitor(other.mMonitor),
    mCommandList(other.mCommandList),
    mCpuBegin(other.mCpuBegin),
    mScopeIndex(other.mScopeIndex)
{
    other.mMonitor = nullptr;
    other.mCommandList = nullptr;
    other.mScopeIndex = UINT_MAX;
}

TrProfileScope::~TrProfileScope()
{
    if(mMonitor != nullptr)
    {
        mMonitor->EndScope(
            mScopeIndex,
            mCommandList,
            mCpuBegin);
    }
}

TrPerformanceMonitor::TrPerformanceMonitor()
{
    mFrameScopeIndex = RegisterScope("Frame Command List");
}

TrPerformanceMonitor::~TrPerformanceMonitor()
{
    if(mTimestampReadback != nullptr && mMappedTimestamps != nullptr)
    {
        const D3D12_RANGE writtenRange = {0, 0};
        mTimestampReadback->Unmap(0, &writtenRange);
        mMappedTimestamps = nullptr;
    }
}

void TrPerformanceMonitor::Initialize(
    ID3D12Device* device,
    ID3D12CommandQueue* commandQueue,
    UINT frameCount)
{
    if(device == nullptr || commandQueue == nullptr || frameCount == 0)
    {
        throw std::invalid_argument(
            "Performance monitor initialization inputs are invalid.");
    }
    if(mGpuInitialized)
    {
        throw std::logic_error("Performance monitor is already initialized.");
    }

    mFrameCount = frameCount;
    mFrames.resize(frameCount);

    D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
    queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    queryHeapDesc.Count = frameCount * MaxProfileScopes * 2u;
    ThrowIfFailed(device->CreateQueryHeap(
        &queryHeapDesc,
        IID_PPV_ARGS(&mTimestampQueryHeap)));
    mTimestampQueryHeap->SetName(L"Renderer GPU Timestamp Query Heap");

    const UINT64 readbackSize =
        static_cast<UINT64>(queryHeapDesc.Count) * sizeof(UINT64);
    const CD3DX12_HEAP_PROPERTIES readbackHeapProperties(
        D3D12_HEAP_TYPE_READBACK);
    const CD3DX12_RESOURCE_DESC readbackDescription =
        CD3DX12_RESOURCE_DESC::Buffer(readbackSize);
    ThrowIfFailed(device->CreateCommittedResource(
        &readbackHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &readbackDescription,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&mTimestampReadback)));
    mTimestampReadback->SetName(L"Renderer GPU Timestamp Readback");

    void* mappedData = nullptr;
    const D3D12_RANGE initialReadRange = {
        0,
        static_cast<SIZE_T>(readbackSize)};
    ThrowIfFailed(mTimestampReadback->Map(
        0,
        &initialReadRange,
        &mappedData));
    mMappedTimestamps = static_cast<const UINT64*>(mappedData);
    ThrowIfFailed(commandQueue->GetTimestampFrequency(&mTimestampFrequency));
    if(mTimestampFrequency == 0)
    {
        throw std::runtime_error("GPU timestamp frequency is zero.");
    }

    mGpuInitialized = true;
    Reset();
}

void TrPerformanceMonitor::Reset()
{
    mLastFrameTime = {};
    mAccumulatedSeconds = 0.0;
    mAccumulatedFrames = 0;
    mSnapshot.FramesPerSecond = 0.0f;
    mSnapshot.FrameTimeMilliseconds = 0.0f;
    mSnapshot.TotalFrameCount = 0;
    mSnapshot.Valid = false;
    for(TrModuleTiming& timing : mSnapshot.Modules)
    {
        timing.CpuMilliseconds = 0.0f;
        timing.GpuMilliseconds = 0.0f;
        timing.CpuValid = false;
        timing.GpuValid = false;
    }
    for(TrFrameProfileData& frame : mFrames)
    {
        frame = {};
    }
    mActiveCommandList = nullptr;
    mActiveFrameIndex = UINT_MAX;
    mFrameRateInitialized = false;
}

void TrPerformanceMonitor::Tick()
{
    const Clock::time_point now = Clock::now();
    if(!mFrameRateInitialized)
    {
        mLastFrameTime = now;
        mFrameRateInitialized = true;
        return;
    }

    const double frameSeconds =
        std::chrono::duration<double>(now - mLastFrameTime).count();
    mLastFrameTime = now;
    ++mSnapshot.TotalFrameCount;

    // Do not fold a minimize, breakpoint or long resize stall into the live
    // frame-rate estimate.
    if(frameSeconds <= 0.0 || frameSeconds > DiscontinuitySeconds)
    {
        mAccumulatedSeconds = 0.0;
        mAccumulatedFrames = 0;
        return;
    }

    mAccumulatedSeconds += frameSeconds;
    ++mAccumulatedFrames;
    if(mAccumulatedSeconds < UpdateIntervalSeconds)
    {
        return;
    }

    mSnapshot.FramesPerSecond = static_cast<float>(
        static_cast<double>(mAccumulatedFrames) / mAccumulatedSeconds);
    mSnapshot.FrameTimeMilliseconds = static_cast<float>(
        1000.0 * mAccumulatedSeconds /
        static_cast<double>(mAccumulatedFrames));
    mSnapshot.Valid = true;
    mAccumulatedSeconds = 0.0;
    mAccumulatedFrames = 0;
}

void TrPerformanceMonitor::BeginFrame(
    UINT frameIndex,
    ID3D12GraphicsCommandList* commandList)
{
    if(!mGpuInitialized || frameIndex >= mFrameCount || commandList == nullptr ||
       mActiveFrameIndex != UINT_MAX)
    {
        throw std::logic_error("Performance monitor cannot begin this frame.");
    }

    TrFrameProfileData& frame = mFrames[frameIndex];
    frame = {};
    mActiveFrameIndex = frameIndex;
    mActiveCommandList = commandList;
    mFrameCpuBegin = Clock::now();
    BeginScope(mFrameScopeIndex, commandList);
}

void TrPerformanceMonitor::EndFrame(ID3D12GraphicsCommandList* commandList)
{
    if(!mGpuInitialized || mActiveFrameIndex == UINT_MAX ||
       commandList == nullptr || commandList != mActiveCommandList)
    {
        throw std::logic_error("Performance monitor cannot end this frame.");
    }

    EndScope(
        mFrameScopeIndex,
        commandList,
        mFrameCpuBegin);
    TrFrameProfileData& frame = mFrames[mActiveFrameIndex];
    for(UINT scopeIndex = 0;
        scopeIndex < static_cast<UINT>(mScopeNames.size());
        ++scopeIndex)
    {
        if(!frame.GpuActive.test(scopeIndex))
        {
            continue;
        }
        const UINT firstQuery = GetQueryIndex(
            mActiveFrameIndex,
            scopeIndex,
            false);
        commandList->ResolveQueryData(
            mTimestampQueryHeap.Get(),
            D3D12_QUERY_TYPE_TIMESTAMP,
            firstQuery,
            2,
            mTimestampReadback.Get(),
            GetReadbackOffset(mActiveFrameIndex, scopeIndex));
    }
    frame.Submitted = true;
    mActiveFrameIndex = UINT_MAX;
    mActiveCommandList = nullptr;
}

void TrPerformanceMonitor::CollectFrame(UINT frameIndex)
{
    if(!mGpuInitialized || frameIndex >= mFrameCount)
    {
        throw std::out_of_range(
            "Performance monitor frame index is outside the ring buffer.");
    }

    TrFrameProfileData& frame = mFrames[frameIndex];
    if(!frame.Submitted)
    {
        return;
    }
    for(UINT scopeIndex = 0;
        scopeIndex < static_cast<UINT>(mScopeNames.size());
        ++scopeIndex)
    {
        UpdateModuleTiming(
            mSnapshot.Modules[scopeIndex],
            frame,
            scopeIndex,
            frameIndex);
    }
    frame.Submitted = false;
}

UINT TrPerformanceMonitor::RegisterScope(const char* name)
{
    if(name == nullptr || name[0] == '\0')
    {
        throw std::invalid_argument("Profile scope requires a name.");
    }
    if(mScopeNames.size() >= MaxProfileScopes)
    {
        throw std::length_error("GPU profile scope capacity has been reached.");
    }

    const UINT scopeIndex = static_cast<UINT>(mScopeNames.size());
    mScopeNames.emplace_back(name);
    TrModuleTiming timing;
    timing.Name = name;
    mSnapshot.Modules.push_back(std::move(timing));
    return scopeIndex;
}

TrProfileScope TrPerformanceMonitor::Profile(
    UINT scopeIndex,
    ID3D12GraphicsCommandList* commandList)
{
    if(scopeIndex >= mScopeNames.size())
    {
        throw std::out_of_range("Profile scope index is not registered.");
    }
    return TrProfileScope(this, scopeIndex, commandList);
}

void TrPerformanceMonitor::BeginScope(
    UINT scopeIndex,
    ID3D12GraphicsCommandList* commandList)
{
    if(!mGpuInitialized || mActiveFrameIndex == UINT_MAX ||
       commandList == nullptr || commandList != mActiveCommandList ||
       scopeIndex >= mScopeNames.size())
    {
        throw std::logic_error("Profile scope is outside an active frame.");
    }

    TrFrameProfileData& frame = mFrames[mActiveFrameIndex];
    if(frame.GpuActive.test(scopeIndex))
    {
        throw std::logic_error(
            "A profile scope can execute only once in one frame.");
    }
    frame.GpuActive.set(scopeIndex);
    commandList->EndQuery(
        mTimestampQueryHeap.Get(),
        D3D12_QUERY_TYPE_TIMESTAMP,
        GetQueryIndex(mActiveFrameIndex, scopeIndex, false));
}

void TrPerformanceMonitor::EndScope(
    UINT scopeIndex,
    ID3D12GraphicsCommandList* commandList,
    Clock::time_point cpuBegin) noexcept
{
    if(!mGpuInitialized || mActiveFrameIndex == UINT_MAX ||
       commandList == nullptr || commandList != mActiveCommandList ||
       scopeIndex >= mScopeNames.size())
    {
        return;
    }

    commandList->EndQuery(
        mTimestampQueryHeap.Get(),
        D3D12_QUERY_TYPE_TIMESTAMP,
        GetQueryIndex(mActiveFrameIndex, scopeIndex, true));
    TrFrameProfileData& frame = mFrames[mActiveFrameIndex];
    frame.CpuMilliseconds[scopeIndex] +=
        std::chrono::duration<double, std::milli>(
            Clock::now() - cpuBegin).count();
    frame.CpuActive.set(scopeIndex);
}

UINT TrPerformanceMonitor::GetQueryIndex(
    UINT frameIndex,
    UINT scopeIndex,
    bool end) const
{
    return (frameIndex * MaxProfileScopes + scopeIndex) * 2u +
        (end ? 1u : 0u);
}

UINT64 TrPerformanceMonitor::GetReadbackOffset(
    UINT frameIndex,
    UINT scopeIndex) const
{
    return static_cast<UINT64>(GetQueryIndex(frameIndex, scopeIndex, false)) *
        sizeof(UINT64);
}

void TrPerformanceMonitor::UpdateModuleTiming(
    TrModuleTiming& timing,
    const TrFrameProfileData& frame,
    UINT scopeIndex,
    UINT frameIndex)
{
    const bool cpuValid = frame.CpuActive.test(scopeIndex);
    if(cpuValid)
    {
        const float sample = static_cast<float>(
            frame.CpuMilliseconds[scopeIndex]);
        timing.CpuMilliseconds = timing.CpuValid
            ? timing.CpuMilliseconds +
                (sample - timing.CpuMilliseconds) * TimingSmoothingFactor
            : sample;
    }
    timing.CpuValid = cpuValid;

    const bool gpuValid = frame.GpuActive.test(scopeIndex);
    if(gpuValid)
    {
        const UINT queryIndex = GetQueryIndex(frameIndex, scopeIndex, false);
        const UINT64 beginTimestamp = mMappedTimestamps[queryIndex];
        const UINT64 endTimestamp = mMappedTimestamps[queryIndex + 1u];
        if(endTimestamp >= beginTimestamp)
        {
            const float sample = static_cast<float>(
                static_cast<double>(endTimestamp - beginTimestamp) * 1000.0 /
                static_cast<double>(mTimestampFrequency));
            timing.GpuMilliseconds = timing.GpuValid
                ? timing.GpuMilliseconds +
                    (sample - timing.GpuMilliseconds) * TimingSmoothingFactor
                : sample;
            timing.GpuValid = true;
        }
        else
        {
            timing.GpuValid = false;
        }
    }
    else
    {
        timing.GpuValid = false;
    }
}
