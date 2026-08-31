#pragma once

#include <DirectXMath.h>
#include <cstdint>
#include <vector>

struct TrCornellBoxVertex
{
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT3 Albedo;
    DirectX::XMFLOAT2 TexCoord0 = {0.0f, 0.0f};
    DirectX::XMFLOAT2 TexCoord1 = {0.0f, 0.0f};
};

static_assert(sizeof(TrCornellBoxVertex) == sizeof(float) * 13);

struct TrCornellBoxMeshData
{
    std::vector<TrCornellBoxVertex> Vertices;
    std::vector<std::uint16_t> Indices;
};

TrCornellBoxMeshData CreateCornellBoxSphereScene();
