#pragma once

#include "TrDescriptorHeap.h"
#include "TrTexture.h"

// Non-owning contract for depth and world-space shading normal. The optional
// Depth/Normal prepass initializes these resources and GBuffer finalizes them
// before both become readable by pixel and compute shaders.
class TrDepthNormalView
{
public:
    TrDepthNormalView() = default;
    TrDepthNormalView(
        const TrTexture& depth,
        const TrDescriptorAllocation& depthSrv,
        const TrTexture& worldNormal,
        const TrDescriptorAllocation& worldNormalSrv);

    void ValidateForCompute() const;
    bool IsValid() const;

    const TrTexture& GetDepth() const;
    const TrTexture& GetWorldNormal() const;
    const TrDescriptorAllocation& GetDepthSrv() const;
    const TrDescriptorAllocation& GetWorldNormalSrv() const;
    UINT GetWidth() const;
    UINT GetHeight() const;

private:
    const TrTexture* mDepth = nullptr;
    const TrTexture* mWorldNormal = nullptr;
    TrDescriptorAllocation mDepthSrv;
    TrDescriptorAllocation mWorldNormalSrv;
};
