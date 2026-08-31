#include "TrMesh.h"

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

    uploadContext.UploadStaticBuffer(
        vertices,
        vertexBufferSize,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
        mVertexBuffer);
    uploadContext.UploadStaticBuffer(
        indices,
        indexBufferSize,
        D3D12_RESOURCE_STATE_INDEX_BUFFER,
        mIndexBuffer);

    mVertexBufferView.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
    mVertexBufferView.SizeInBytes = static_cast<UINT>(vertexBufferSize);
    mVertexBufferView.StrideInBytes = vertexStride;
    mIndexBufferView.BufferLocation = mIndexBuffer->GetGPUVirtualAddress();
    mIndexBufferView.SizeInBytes = static_cast<UINT>(indexBufferSize);
    mIndexBufferView.Format = indexFormat;
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
    commandList->DrawIndexedInstanced(mIndexCount, instanceCount, 0, 0, 0);
}
