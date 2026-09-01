#pragma once

#include "TrDescriptorHeap.h"
#include "TrTexture.h"

#include <string>

class TrHistoryTexture
{
public:
    static constexpr UINT DescriptorCount = 4;

    void Initialize(
        ID3D12Device* device,
        UINT width,
        UINT height,
        DXGI_FORMAT format,
        TrDescriptorHeap& resourceHeap,
        const wchar_t* debugName = nullptr);
    void Resize(ID3D12Device* device, UINT width, UINT height);

    // Call after the current texture has been completely written. It becomes
    // the previous texture read by the next frame.
    void AdvanceFrame();
    void Invalidate();

    bool IsValid() const { return mValid; }
    TrTexture& GetCurrent();
    const TrTexture& GetCurrent() const;
    TrTexture& GetPrevious();
    const TrTexture& GetPrevious() const;
    const TrDescriptorAllocation& GetCurrentSrv() const;
    const TrDescriptorAllocation& GetPreviousSrv() const;
    const TrDescriptorAllocation& GetCurrentUav() const;
    const TrDescriptorAllocation& GetPreviousUav() const;

private:
    void CreateResources(ID3D12Device* device, UINT width, UINT height);
    void ValidateInitialized() const;
    UINT GetPreviousIndex() const { return 1u - mCurrentIndex; }

    TrTexture mTextures[2];
    TrDescriptorAllocation mSrvs[2];
    TrDescriptorAllocation mUavs[2];
    DXGI_FORMAT mFormat = DXGI_FORMAT_UNKNOWN;
    std::wstring mDebugName;
    UINT mCurrentIndex = 0;
    bool mValid = false;
    bool mInitialized = false;
};
