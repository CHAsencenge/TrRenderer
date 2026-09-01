#pragma once

#include <DirectXMath.h>
#include <cstdint>
#include <string>
#include <vector>

class TrScene;

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
    struct Part
    {
        std::string Name;
        std::uint32_t FirstVertex = 0;
        std::uint32_t VertexCount = 0;
        std::uint32_t FirstIndex = 0;
        std::uint32_t IndexCount = 0;
    };

    std::vector<TrCornellBoxVertex> Vertices;
    std::vector<std::uint16_t> Indices;
    std::vector<Part> Parts;
};

TrCornellBoxMeshData CreateCornellBoxSphereScene();
TrScene CreateCornellBoxScene();
