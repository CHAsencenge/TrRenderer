#include "TrMesh.h"

#include "TrUploadContext.h"

#include <limits>
#include <stdexcept>

void TrMesh::Initialize(
    TrUploadContext& uploadContext,
    const void* vertices,
    UINT vertexCount,
    UINT vertexStride,
    const void* indices,
    UINT indexCount,
    DXGI_FORMAT indexFormat)
{
    if(vertices == nullptr || indices == nullptr || vertexCount == 0 ||
       vertexStride == 0 || indexCount == 0)
    {
        throw std::invalid_argument("Mesh requires non-empty vertex and index data.");
    }

    UINT indexStride = 0;
    if(indexFormat == DXGI_FORMAT_R16_UINT)
    {
        indexStride = sizeof(UINT16);
    }
    else if(indexFormat == DXGI_FORMAT_R32_UINT)
    {
        indexStride = sizeof(UINT32);
    }
    else
    {
        throw std::invalid_argument("Mesh indices must be R16_UINT or R32_UINT.");
    }

    const UINT64 vertexBufferSize = static_cast<UINT64>(vertexCount) * vertexStride;
    const UINT64 indexBufferSize = static_cast<UINT64>(indexCount) * indexStride;
    if(vertexBufferSize > std::numeric_limits<UINT>::max() ||
       indexBufferSize > std::numeric_limits<UINT>::max())
    {
        throw std::overflow_error("Mesh buffer exceeds the D3D12 view size limit.");
    }

    mVertexBuffer.InitializeStatic(
        uploadContext,
        vertices,
        vertexBufferSize,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
        D3D12_RESOURCE_FLAG_NONE,
        L"Mesh Vertex Buffer");
    mIndexBuffer.InitializeStatic(
        uploadContext,
        indices,
        indexBufferSize,
        D3D12_RESOURCE_STATE_INDEX_BUFFER,
        D3D12_RESOURCE_FLAG_NONE,
        L"Mesh Index Buffer");

    mVertexBufferView = mVertexBuffer.CreateVertexBufferView(vertexStride);
    mIndexBufferView = mIndexBuffer.CreateIndexBufferView(indexFormat);
    mIndexCount = indexCount;
}

void TrMesh::Bind(ID3D12GraphicsCommandList* commandList) const
{
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &mVertexBufferView);
    commandList->IASetIndexBuffer(&mIndexBufferView);
}

void TrMesh::Draw(ID3D12GraphicsCommandList* commandList, UINT instanceCount) const
{
    DrawRange(commandList, mIndexCount, 0, instanceCount);
}

void TrMesh::DrawRange(
    ID3D12GraphicsCommandList* commandList,
    UINT indexCount,
    UINT firstIndex,
    UINT instanceCount) const
{
    if(commandList == nullptr || indexCount == 0 ||
       static_cast<UINT64>(firstIndex) + indexCount > mIndexCount)
    {
        throw std::out_of_range("Mesh draw range is invalid.");
    }
    commandList->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, 0, 0);
}
