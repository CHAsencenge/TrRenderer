#include "TrCornellBoxScene.h"

#include <limits>
#include <stdexcept>

TrCornellBoxMeshData CreateCornellBoxSphereScene()
{
    TrCornellBoxMeshData meshData;

    const DirectX::XMFLOAT3 white(0.73f, 0.73f, 0.73f);
    const DirectX::XMFLOAT3 red(0.63f, 0.065f, 0.05f);
    const DirectX::XMFLOAT3 green(0.14f, 0.45f, 0.091f);

    auto addQuad = [&meshData](
        const DirectX::XMFLOAT3& p0,
        const DirectX::XMFLOAT3& p1,
        const DirectX::XMFLOAT3& p2,
        const DirectX::XMFLOAT3& p3,
        const DirectX::XMFLOAT3& normal,
        const DirectX::XMFLOAT3& albedo)
    {
        if(meshData.Vertices.size() + 4 > std::numeric_limits<std::uint16_t>::max())
        {
            throw std::overflow_error("Cornell Box exceeds 16-bit index capacity.");
        }

        const std::uint16_t baseIndex = static_cast<std::uint16_t>(meshData.Vertices.size());
        meshData.Vertices.push_back({p0, normal, albedo});
        meshData.Vertices.push_back({p1, normal, albedo});
        meshData.Vertices.push_back({p2, normal, albedo});
        meshData.Vertices.push_back({p3, normal, albedo});
        meshData.Indices.insert(meshData.Indices.end(), {
            static_cast<std::uint16_t>(baseIndex + 0),
            static_cast<std::uint16_t>(baseIndex + 1),
            static_cast<std::uint16_t>(baseIndex + 2),
            static_cast<std::uint16_t>(baseIndex + 0),
            static_cast<std::uint16_t>(baseIndex + 2),
            static_cast<std::uint16_t>(baseIndex + 3)});
    };

    // Open-front room: floor, ceiling, back wall, red left wall and green right wall.
    addQuad({-1.0f, 0.0f, 0.0f}, { 1.0f, 0.0f, 0.0f}, { 1.0f, 0.0f, 2.0f}, {-1.0f, 0.0f, 2.0f}, { 0.0f, 1.0f, 0.0f}, white);
    addQuad({-1.0f, 2.0f, 2.0f}, { 1.0f, 2.0f, 2.0f}, { 1.0f, 2.0f, 0.0f}, {-1.0f, 2.0f, 0.0f}, { 0.0f,-1.0f, 0.0f}, white);
    addQuad({-1.0f, 0.0f, 2.0f}, { 1.0f, 0.0f, 2.0f}, { 1.0f, 2.0f, 2.0f}, {-1.0f, 2.0f, 2.0f}, { 0.0f, 0.0f,-1.0f}, white);
    addQuad({-1.0f, 0.0f, 2.0f}, {-1.0f, 0.0f, 0.0f}, {-1.0f, 2.0f, 0.0f}, {-1.0f, 2.0f, 2.0f}, { 1.0f, 0.0f, 0.0f}, red);
    addQuad({ 1.0f, 0.0f, 0.0f}, { 1.0f, 0.0f, 2.0f}, { 1.0f, 2.0f, 2.0f}, { 1.0f, 2.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, green);
    addQuad({-0.32f, 1.99f, 0.70f}, {0.32f, 1.99f, 0.70f}, {0.32f, 1.99f, 1.30f}, {-0.32f, 1.99f, 1.30f}, {0.0f,-1.0f, 0.0f}, {4.0f, 4.0f, 4.0f});

    auto addUvSphere = [&meshData](
        const DirectX::XMFLOAT3& center,
        float radius,
        std::uint32_t sliceCount,
        std::uint32_t stackCount,
        const DirectX::XMFLOAT3& albedo)
    {
        const std::uint32_t requiredVertexCount = (sliceCount + 1) * (stackCount + 1);
        if(meshData.Vertices.size() + requiredVertexCount > std::numeric_limits<std::uint16_t>::max())
        {
            throw std::overflow_error("Cornell Box exceeds 16-bit index capacity.");
        }

        const std::uint32_t baseIndex = static_cast<std::uint32_t>(meshData.Vertices.size());
        for(std::uint32_t stack = 0; stack <= stackCount; ++stack)
        {
            const float phi = DirectX::XM_PI * static_cast<float>(stack) / static_cast<float>(stackCount);
            float sinPhi = 0.0f;
            float cosPhi = 1.0f;
            DirectX::XMScalarSinCos(&sinPhi, &cosPhi, phi);

            for(std::uint32_t slice = 0; slice <= sliceCount; ++slice)
            {
                const float theta = DirectX::XM_2PI * static_cast<float>(slice) / static_cast<float>(sliceCount);
                float sinTheta = 0.0f;
                float cosTheta = 1.0f;
                DirectX::XMScalarSinCos(&sinTheta, &cosTheta, theta);

                const DirectX::XMFLOAT3 normal(
                    sinPhi * cosTheta,
                    cosPhi,
                    sinPhi * sinTheta);
                const DirectX::XMFLOAT3 position(
                    center.x + radius * normal.x,
                    center.y + radius * normal.y,
                    center.z + radius * normal.z);
                meshData.Vertices.push_back({position, normal, albedo});
            }
        }

        const std::uint32_t rowVertexCount = sliceCount + 1;
        for(std::uint32_t stack = 0; stack < stackCount; ++stack)
        {
            for(std::uint32_t slice = 0; slice < sliceCount; ++slice)
            {
                const std::uint32_t topLeft = baseIndex + stack * rowVertexCount + slice;
                const std::uint32_t bottomLeft = topLeft + rowVertexCount;
                meshData.Indices.insert(meshData.Indices.end(), {
                    static_cast<std::uint16_t>(topLeft),
                    static_cast<std::uint16_t>(bottomLeft),
                    static_cast<std::uint16_t>(topLeft + 1),
                    static_cast<std::uint16_t>(topLeft + 1),
                    static_cast<std::uint16_t>(bottomLeft),
                    static_cast<std::uint16_t>(bottomLeft + 1)});
            }
        }
    };

    // Warm and cool albedos are placeholders for future metal and dielectric BSDFs.
    addUvSphere({-0.38f, 0.34f, 0.72f}, 0.34f, 32, 20, {0.78f, 0.61f, 0.32f});
    addUvSphere({ 0.38f, 0.46f, 1.28f}, 0.46f, 32, 20, {0.68f, 0.78f, 0.90f});

    return meshData;
}
