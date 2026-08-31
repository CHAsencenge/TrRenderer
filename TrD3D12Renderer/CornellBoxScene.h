#pragma once

#include <DirectXMath.h>
#include <cstdint>
#include <vector>

struct CornellBoxVertex
{
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT3 Albedo;
};

struct CornellBoxMeshData
{
    std::vector<CornellBoxVertex> Vertices;
    std::vector<std::uint16_t> Indices;
};

CornellBoxMeshData CreateCornellBoxSphereScene();
