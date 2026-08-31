#pragma once

#include <DirectXMath.h>
#include <cstdint>
#include <vector>

struct TrCornellBoxVertex
{
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT3 Albedo;
};

struct TrCornellBoxMeshData
{
    std::vector<TrCornellBoxVertex> Vertices;
    std::vector<std::uint16_t> Indices;
};

TrCornellBoxMeshData CreateCornellBoxSphereScene();
