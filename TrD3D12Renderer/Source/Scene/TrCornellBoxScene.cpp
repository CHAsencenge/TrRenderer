#include "TrCornellBoxScene.h"
#include "TrScene.h"

#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

namespace
{
    std::array<float, 16> StoreMatrix(DirectX::FXMMATRIX matrix)
    {
        DirectX::XMFLOAT4X4 stored;
        DirectX::XMStoreFloat4x4(&stored, matrix);
        std::array<float, 16> result;
        static_assert(sizeof(result) == sizeof(stored));
        std::memcpy(result.data(), &stored, sizeof(result));
        return result;
    }

    DirectX::XMMATRIX LoadMatrix(const std::array<float, 16>& matrix)
    {
        DirectX::XMFLOAT4X4 stored;
        static_assert(sizeof(matrix) == sizeof(stored));
        std::memcpy(&stored, matrix.data(), sizeof(stored));
        return DirectX::XMLoadFloat4x4(&stored);
    }

    std::uint32_t AddNode(
        TrScene& scene,
        const char* name,
        std::uint32_t parentNodeIndex,
        std::uint32_t meshIndex,
        DirectX::FXMMATRIX localTransform)
    {
        TrSceneNode node;
        node.Name = name;
        node.ParentIndex = parentNodeIndex;
        node.MeshIndex = meshIndex;
        node.LocalTransform = StoreMatrix(localTransform);
        node.WorldTransform = parentNodeIndex == TrInvalidSceneIndex
            ? node.LocalTransform
            : StoreMatrix(localTransform * LoadMatrix(
                scene.Nodes[parentNodeIndex].WorldTransform));

        const std::uint32_t nodeIndex = static_cast<std::uint32_t>(scene.Nodes.size());
        scene.Nodes.push_back(node);
        if(parentNodeIndex == TrInvalidSceneIndex)
        {
            scene.RootNodes.push_back(nodeIndex);
        }
        else
        {
            scene.Nodes[parentNodeIndex].Children.push_back(nodeIndex);
        }
        return nodeIndex;
    }

    TrSceneMaterial MakeMaterial(
        const char* name,
        const std::array<float, 4>& baseColor,
        float roughness,
        float metallic = 0.0f)
    {
        TrSceneMaterial material;
        material.Name = name;
        material.BaseColorFactor = baseColor;
        material.RoughnessFactor = roughness;
        material.MetallicFactor = metallic;
        return material;
    }
}

TrCornellBoxMeshData CreateCornellBoxSphereScene()
{
    TrCornellBoxMeshData meshData;

    const DirectX::XMFLOAT3 white(0.73f, 0.73f, 0.73f);
    const DirectX::XMFLOAT3 red(0.63f, 0.065f, 0.05f);
    const DirectX::XMFLOAT3 green(0.14f, 0.45f, 0.091f);

    auto addQuad = [&meshData](
        const char* name,
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
        const std::uint32_t firstIndex = static_cast<std::uint32_t>(meshData.Indices.size());
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
        meshData.Parts.push_back(
        {
            name,
            baseIndex,
            4,
            firstIndex,
            6
        });
    };

    // Open-front room: floor, ceiling, back wall, red left wall and green right wall.
    addQuad("Floor", {-1.0f, 0.0f, 0.0f}, { 1.0f, 0.0f, 0.0f}, { 1.0f, 0.0f, 2.0f}, {-1.0f, 0.0f, 2.0f}, { 0.0f, 1.0f, 0.0f}, white);
    addQuad("Ceiling", {-1.0f, 2.0f, 2.0f}, { 1.0f, 2.0f, 2.0f}, { 1.0f, 2.0f, 0.0f}, {-1.0f, 2.0f, 0.0f}, { 0.0f,-1.0f, 0.0f}, white);
    addQuad("Back Wall", {-1.0f, 0.0f, 2.0f}, { 1.0f, 0.0f, 2.0f}, { 1.0f, 2.0f, 2.0f}, {-1.0f, 2.0f, 2.0f}, { 0.0f, 0.0f,-1.0f}, white);
    addQuad("Left Wall", {-1.0f, 0.0f, 2.0f}, {-1.0f, 0.0f, 0.0f}, {-1.0f, 2.0f, 0.0f}, {-1.0f, 2.0f, 2.0f}, { 1.0f, 0.0f, 0.0f}, red);
    addQuad("Right Wall", { 1.0f, 0.0f, 0.0f}, { 1.0f, 0.0f, 2.0f}, { 1.0f, 2.0f, 2.0f}, { 1.0f, 2.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, green);
    addQuad("Ceiling Light", {-0.32f, 1.99f, 0.70f}, {0.32f, 1.99f, 0.70f}, {0.32f, 1.99f, 1.30f}, {-0.32f, 1.99f, 1.30f}, {0.0f,-1.0f, 0.0f}, {4.0f, 4.0f, 4.0f});

    auto addUvSphere = [&meshData](
        const char* name,
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
        const std::uint32_t firstIndex = static_cast<std::uint32_t>(meshData.Indices.size());
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
        meshData.Parts.push_back(
        {
            name,
            baseIndex,
            requiredVertexCount,
            firstIndex,
            sliceCount * stackCount * 6
        });
    };

    // Warm and cool albedos are placeholders for future metal and dielectric BSDFs.
    addUvSphere("Left Sphere", {-0.38f, 0.34f, 0.72f}, 0.34f, 32, 20, {0.78f, 0.61f, 0.32f});
    addUvSphere("Right Sphere", { 0.38f, 0.46f, 1.28f}, 0.46f, 32, 20, {0.68f, 0.78f, 0.90f});

    return meshData;
}

TrScene CreateCornellBoxScene()
{
    const TrCornellBoxMeshData source = CreateCornellBoxSphereScene();

    TrScene result;
    result.Name = "Procedural Hierarchy Cornell Box";
    result.SourceGenerator = "TrCornellBoxScene/RuntimeSceneValidation";

    result.Materials.push_back(MakeMaterial(
        "Matte White", {0.73f, 0.73f, 0.73f, 1.0f}, 0.82f));
    result.Materials.push_back(MakeMaterial(
        "Matte Red", {0.63f, 0.065f, 0.05f, 1.0f}, 0.78f));
    result.Materials.push_back(MakeMaterial(
        "Matte Green", {0.14f, 0.45f, 0.091f, 1.0f}, 0.78f));
    TrSceneMaterial ceilingLight = MakeMaterial(
        "Ceiling Light", {1.0f, 1.0f, 1.0f, 1.0f}, 0.2f);
    ceilingLight.EmissiveFactor = {1.0f, 0.91f, 0.72f};
    ceilingLight.EmissiveStrength = 7.0f;
    result.Materials.push_back(ceilingLight);
    result.Materials.push_back(MakeMaterial(
        "Brushed Gold", {0.82f, 0.58f, 0.24f, 1.0f}, 0.24f, 0.72f));
    result.Materials.push_back(MakeMaterial(
        "Blue Ceramic", {0.12f, 0.30f, 0.72f, 1.0f}, 0.20f));
    TrSceneMaterial transparentCyan = MakeMaterial(
        "Transparent Cyan", {0.08f, 0.72f, 0.92f, 0.32f}, 0.16f);
    transparentCyan.AlphaMode = TrSceneAlphaMode::Blend;
    transparentCyan.DoubleSided = true;
    result.Materials.push_back(transparentCyan);

    // One mesh with six material primitives verifies that Primitive is a draw
    // range/material section rather than a scene object.
    TrSceneMesh roomMesh;
    roomMesh.Name = "Room Multi-Primitive Mesh";
    constexpr std::array<std::uint32_t, 6> roomMaterialIds = {0, 0, 0, 1, 2, 3};
    for(std::size_t partIndex = 0; partIndex < roomMaterialIds.size(); ++partIndex)
    {
        const TrCornellBoxMeshData::Part& part = source.Parts[partIndex];
        TrScenePrimitive primitive;
        primitive.FirstVertex = static_cast<std::uint32_t>(roomMesh.Vertices.size());
        primitive.VertexCount = part.VertexCount;
        primitive.FirstIndex = static_cast<std::uint32_t>(roomMesh.Indices.size());
        primitive.IndexCount = part.IndexCount;
        primitive.MaterialIndex = roomMaterialIds[partIndex];

        for(std::uint32_t vertexOffset = 0; vertexOffset < part.VertexCount; ++vertexOffset)
        {
            const TrCornellBoxVertex& sourceVertex =
                source.Vertices[part.FirstVertex + vertexOffset];
            TrSceneVertex vertex;
            vertex.Position =
            {
                sourceVertex.Position.x,
                sourceVertex.Position.y,
                sourceVertex.Position.z
            };
            vertex.Normal =
            {
                sourceVertex.Normal.x,
                sourceVertex.Normal.y,
                sourceVertex.Normal.z
            };
            vertex.Color = {1.0f, 1.0f, 1.0f, 1.0f};
            roomMesh.Vertices.push_back(vertex);
        }
        for(std::uint32_t indexOffset = 0; indexOffset < part.IndexCount; ++indexOffset)
        {
            roomMesh.Indices.push_back(
                primitive.FirstVertex +
                static_cast<std::uint32_t>(source.Indices[part.FirstIndex + indexOffset]) -
                part.FirstVertex);
        }
        roomMesh.Primitives.push_back(primitive);
    }
    result.Meshes.push_back(std::move(roomMesh));

    // Convert one legacy sphere into a unit local-space mesh. Multiple nodes
    // below share this single GPU mesh with different transforms.
    const TrCornellBoxMeshData::Part& sourceSphere = source.Parts[6];
    TrSceneMesh sphereMesh;
    sphereMesh.Name = "Shared Unit Sphere";
    sphereMesh.Vertices.reserve(sourceSphere.VertexCount);
    for(std::uint32_t vertexOffset = 0;
        vertexOffset < sourceSphere.VertexCount;
        ++vertexOffset)
    {
        const TrCornellBoxVertex& sourceVertex =
            source.Vertices[sourceSphere.FirstVertex + vertexOffset];
        TrSceneVertex vertex;
        vertex.Position =
        {
            (sourceVertex.Position.x + 0.38f) / 0.34f,
            (sourceVertex.Position.y - 0.34f) / 0.34f,
            (sourceVertex.Position.z - 0.72f) / 0.34f
        };
        vertex.Normal =
        {
            sourceVertex.Normal.x,
            sourceVertex.Normal.y,
            sourceVertex.Normal.z
        };
        vertex.Color = {1.0f, 1.0f, 1.0f, 1.0f};
        sphereMesh.Vertices.push_back(vertex);
    }
    sphereMesh.Indices.reserve(sourceSphere.IndexCount);
    for(std::uint32_t indexOffset = 0;
        indexOffset < sourceSphere.IndexCount;
        ++indexOffset)
    {
        sphereMesh.Indices.push_back(
            static_cast<std::uint32_t>(source.Indices[sourceSphere.FirstIndex + indexOffset]) -
            sourceSphere.FirstVertex);
    }
    TrScenePrimitive spherePrimitive;
    spherePrimitive.VertexCount = static_cast<std::uint32_t>(sphereMesh.Vertices.size());
    spherePrimitive.IndexCount = static_cast<std::uint32_t>(sphereMesh.Indices.size());
    spherePrimitive.MaterialIndex = 4;
    sphereMesh.Primitives.push_back(spherePrimitive);
    result.Meshes.push_back(std::move(sphereMesh));

    TrSceneMesh cubeMesh;
    cubeMesh.Name = "Shared Unit Cube";
    auto addCubeFace = [&cubeMesh](
        const std::array<float, 3>& p0,
        const std::array<float, 3>& p1,
        const std::array<float, 3>& p2,
        const std::array<float, 3>& p3,
        const std::array<float, 3>& normal)
    {
        const std::uint32_t firstVertex = static_cast<std::uint32_t>(cubeMesh.Vertices.size());
        for(const std::array<float, 3>& position : {p0, p1, p2, p3})
        {
            TrSceneVertex vertex;
            vertex.Position = position;
            vertex.Normal = normal;
            vertex.Color = {1.0f, 1.0f, 1.0f, 1.0f};
            cubeMesh.Vertices.push_back(vertex);
        }
        cubeMesh.Indices.insert(cubeMesh.Indices.end(),
        {
            firstVertex, firstVertex + 1, firstVertex + 2,
            firstVertex, firstVertex + 2, firstVertex + 3
        });
    };
    addCubeFace({-0.5f,-0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f}, { 0.0f, 0.0f,-1.0f});
    addCubeFace({ 0.5f,-0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, {-0.5f,-0.5f, 0.5f}, { 0.0f, 0.0f, 1.0f});
    addCubeFace({-0.5f,-0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f,-0.5f}, {-0.5f,-0.5f,-0.5f}, {-1.0f, 0.0f, 0.0f});
    addCubeFace({ 0.5f,-0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f}, { 1.0f, 0.0f, 0.0f});
    addCubeFace({-0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f,-0.5f}, { 0.0f, 1.0f, 0.0f});
    addCubeFace({-0.5f,-0.5f, 0.5f}, {-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f, 0.5f}, { 0.0f,-1.0f, 0.0f});
    TrScenePrimitive cubePrimitive;
    cubePrimitive.VertexCount = static_cast<std::uint32_t>(cubeMesh.Vertices.size());
    cubePrimitive.IndexCount = static_cast<std::uint32_t>(cubeMesh.Indices.size());
    cubePrimitive.MaterialIndex = 5;
    cubeMesh.Primitives.push_back(cubePrimitive);
    result.Meshes.push_back(std::move(cubeMesh));

    TrSceneMesh transparentPanelMesh;
    transparentPanelMesh.Name = "Transparent Validation Panel";
    constexpr std::array<std::array<float, 3>, 4> panelPositions =
    {{
        {-0.5f, -0.5f, 0.0f},
        {-0.5f,  0.5f, 0.0f},
        { 0.5f,  0.5f, 0.0f},
        { 0.5f, -0.5f, 0.0f}
    }};
    for(const std::array<float, 3>& position : panelPositions)
    {
        TrSceneVertex vertex;
        vertex.Position = position;
        vertex.Normal = {0.0f, 0.0f, -1.0f};
        vertex.Color = {1.0f, 1.0f, 1.0f, 1.0f};
        transparentPanelMesh.Vertices.push_back(vertex);
    }
    transparentPanelMesh.Indices = {0, 1, 2, 0, 2, 3};
    TrScenePrimitive transparentPanelPrimitive;
    transparentPanelPrimitive.VertexCount = 4;
    transparentPanelPrimitive.IndexCount = 6;
    transparentPanelPrimitive.MaterialIndex = 6;
    transparentPanelMesh.Primitives.push_back(transparentPanelPrimitive);
    result.Meshes.push_back(std::move(transparentPanelMesh));

    using namespace DirectX;
    const std::uint32_t root = AddNode(
        result, "Cornell Box Root", TrInvalidSceneIndex, TrInvalidSceneIndex,
        XMMatrixIdentity());
    AddNode(result, "Room", root, 0, XMMatrixIdentity());

    // The rig and its child groups exercise real parent-to-child propagation;
    // two source meshes are reused by several independently transformed nodes.
    const XMMATRIX rigTransform =
        XMMatrixTranslation(0.0f, 0.0f, -1.0f) *
        XMMatrixRotationY(XMConvertToRadians(4.0f)) *
        XMMatrixTranslation(0.0f, 0.0f, 1.0f);
    const std::uint32_t rig = AddNode(
        result, "Sculpture Rig", root, TrInvalidSceneIndex, rigTransform);

    const std::uint32_t lowPedestal = AddNode(
        result, "Low Pedestal Group", rig, TrInvalidSceneIndex,
        XMMatrixTranslation(-0.46f, 0.0f, 1.23f));
    AddNode(result, "Low Pedestal", lowPedestal, 2,
        XMMatrixScaling(0.58f, 0.46f, 0.58f) *
        XMMatrixTranslation(0.0f, 0.23f, 0.0f));
    AddNode(result, "Low Pedestal Orb", lowPedestal, 1,
        XMMatrixScaling(0.30f, 0.30f, 0.30f) *
        XMMatrixTranslation(0.0f, 0.78f, 0.0f));

    const std::uint32_t tallPedestal = AddNode(
        result, "Tall Pedestal Group", rig, TrInvalidSceneIndex,
        XMMatrixTranslation(0.43f, 0.0f, 0.75f));
    AddNode(result, "Tall Pedestal", tallPedestal, 2,
        XMMatrixScaling(0.42f, 0.82f, 0.42f) *
        XMMatrixTranslation(0.0f, 0.41f, 0.0f));
    AddNode(result, "Tall Pedestal Orb", tallPedestal, 1,
        XMMatrixScaling(0.36f, 0.36f, 0.36f) *
        XMMatrixTranslation(0.0f, 1.18f, 0.0f));

    AddNode(result, "Rotated Display Cube", rig, 2,
        XMMatrixScaling(0.30f, 0.30f, 0.30f) *
        XMMatrixRotationY(XMConvertToRadians(32.0f)) *
        XMMatrixTranslation(0.05f, 0.30f, 1.70f));
    AddNode(result, "Floor Orb", rig, 1,
        XMMatrixScaling(0.17f, 0.17f, 0.17f) *
        XMMatrixTranslation(0.08f, 0.17f, 0.24f));
    AddNode(result, "Transparent Validation Panel", root, 3,
        XMMatrixScaling(1.15f, 0.92f, 1.0f) *
        XMMatrixTranslation(0.0f, 0.82f, 0.52f));

    result.Validate();
    return result;
}
