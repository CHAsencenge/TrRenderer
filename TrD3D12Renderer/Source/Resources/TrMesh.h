#pragma once

#include "TrBuffer.h"

class TrUploadContext;

class TrMesh
{
public:
    void Initialize(
        TrUploadContext& uploadContext,
        const void* vertices,
        UINT vertexCount,
        UINT vertexStride,
        const void* indices,
        UINT indexCount,
        DXGI_FORMAT indexFormat);

    void Bind(ID3D12GraphicsCommandList* commandList) const;
    void Draw(ID3D12GraphicsCommandList* commandList, UINT instanceCount = 1) const;
    void DrawRange(
        ID3D12GraphicsCommandList* commandList,
        UINT indexCount,
        UINT firstIndex,
        UINT instanceCount = 1) const;

    UINT GetIndexCount() const { return mIndexCount; }

private:
    TrBuffer mVertexBuffer;
    TrBuffer mIndexBuffer;
    D3D12_VERTEX_BUFFER_VIEW mVertexBufferView = {};
    D3D12_INDEX_BUFFER_VIEW mIndexBufferView = {};
    UINT mIndexCount = 0;
};
