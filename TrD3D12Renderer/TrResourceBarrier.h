#pragma once

#include "TrUtil.h"

namespace TrResourceBarrier
{
    bool Transition(
        ID3D12GraphicsCommandList* commandList,
        ID3D12Resource* resource,
        D3D12_RESOURCE_STATES beforeState,
        D3D12_RESOURCE_STATES afterState,
        UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

    void Uav(
        ID3D12GraphicsCommandList* commandList,
        ID3D12Resource* resource);
}
