#include "TrResourceBarrier.h"

#include <stdexcept>

bool TrResourceBarrier::Transition(
    ID3D12GraphicsCommandList* commandList,
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES beforeState,
    D3D12_RESOURCE_STATES afterState,
    UINT subresource)
{
    if(commandList == nullptr || resource == nullptr)
    {
        throw std::invalid_argument("Resource transition requires a command list and resource.");
    }

    if(beforeState == afterState)
    {
        return false;
    }

    const CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        resource,
        beforeState,
        afterState,
        subresource);
    commandList->ResourceBarrier(1, &barrier);
    return true;
}

void TrResourceBarrier::Uav(
    ID3D12GraphicsCommandList* commandList,
    ID3D12Resource* resource)
{
    if(commandList == nullptr)
    {
        throw std::invalid_argument("UAV barrier requires a command list.");
    }

    const CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::UAV(resource);
    commandList->ResourceBarrier(1, &barrier);
}
