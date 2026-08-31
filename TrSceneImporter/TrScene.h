#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

constexpr std::uint32_t TrInvalidSceneIndex = UINT32_MAX;

enum class TrSceneAlphaMode : std::uint32_t
{
    Opaque,
    Mask,
    Blend
};

enum class TrSceneLightType : std::uint32_t
{
    Directional,
    Point,
    Spot
};

enum class TrSceneCameraType : std::uint32_t
{
    Perspective,
    Orthographic
};

struct TrSceneTextureBinding
{
    std::int32_t TextureIndex = -1;
    std::int32_t TexCoord = 0;
    float Strength = 1.0f;
    std::array<float, 2> Offset = {0.0f, 0.0f};
    std::array<float, 2> Scale = {1.0f, 1.0f};
    float Rotation = 0.0f;
};

struct TrSceneMaterial
{
    std::string Name;
    std::array<float, 4> BaseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
    std::array<float, 3> EmissiveFactor = {0.0f, 0.0f, 0.0f};
    float MetallicFactor = 1.0f;
    float RoughnessFactor = 1.0f;
    float EmissiveStrength = 1.0f;
    float AlphaCutoff = 0.5f;
    TrSceneAlphaMode AlphaMode = TrSceneAlphaMode::Opaque;
    bool DoubleSided = false;
    bool Unlit = false;
    TrSceneTextureBinding BaseColorTexture;
    TrSceneTextureBinding MetallicRoughnessTexture;
    TrSceneTextureBinding NormalTexture;
    TrSceneTextureBinding OcclusionTexture;
    TrSceneTextureBinding EmissiveTexture;
};

struct TrSceneImage
{
    std::string Name;
    std::string MimeType;
    std::vector<std::uint8_t> Data;
};

struct TrSceneSampler
{
    std::string Name;
    std::uint32_t MagFilter = 0;
    std::uint32_t MinFilter = 0;
    std::uint32_t WrapU = 10497;
    std::uint32_t WrapV = 10497;
};

struct TrSceneTexture
{
    std::string Name;
    std::int32_t ImageIndex = -1;
    std::int32_t SamplerIndex = -1;
};

struct TrSceneVertex
{
    std::array<float, 3> Position = {};
    std::array<float, 3> Normal = {0.0f, 1.0f, 0.0f};
    std::array<float, 4> Tangent = {1.0f, 0.0f, 0.0f, 1.0f};
    std::array<float, 2> TexCoord0 = {};
    std::array<float, 2> TexCoord1 = {};
    std::array<float, 4> Color = {1.0f, 1.0f, 1.0f, 1.0f};
};

struct TrScenePrimitive
{
    std::uint32_t FirstVertex = 0;
    std::uint32_t VertexCount = 0;
    std::uint32_t FirstIndex = 0;
    std::uint32_t IndexCount = 0;
    std::uint32_t MaterialIndex = TrInvalidSceneIndex;
};

struct TrSceneMesh
{
    std::string Name;
    std::vector<TrSceneVertex> Vertices;
    std::vector<std::uint32_t> Indices;
    std::vector<TrScenePrimitive> Primitives;
};

struct TrSceneLight
{
    std::string Name;
    TrSceneLightType Type = TrSceneLightType::Point;
    std::array<float, 3> Color = {1.0f, 1.0f, 1.0f};
    float Intensity = 1.0f;
    float Range = 0.0f;
    float InnerConeAngle = 0.0f;
    float OuterConeAngle = 0.785398163f;
};

struct TrSceneCamera
{
    std::string Name;
    TrSceneCameraType Type = TrSceneCameraType::Perspective;
    float AspectRatio = 0.0f;
    float VerticalFieldOfView = 0.785398163f;
    float NearPlane = 0.1f;
    float FarPlane = 0.0f;
    float HorizontalMagnification = 1.0f;
    float VerticalMagnification = 1.0f;
};

struct TrSceneNode
{
    std::string Name;
    std::uint32_t ParentIndex = TrInvalidSceneIndex;
    std::vector<std::uint32_t> Children;
    std::uint32_t MeshIndex = TrInvalidSceneIndex;
    std::uint32_t LightIndex = TrInvalidSceneIndex;
    std::uint32_t CameraIndex = TrInvalidSceneIndex;
    // Row-major matrices for row-vector math, already converted from glTF's
    // right-handed coordinate system to Tr's left-handed coordinate system.
    std::array<float, 16> LocalTransform =
    {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    std::array<float, 16> WorldTransform = LocalTransform;
};

// Vertex layout consumed by the current DX12 GBuffer preview path. Material
// indices remain on draw ranges so the renderer can bind textures per primitive.
struct TrSceneRenderVertex
{
    std::array<float, 3> Position = {};
    std::array<float, 3> Normal = {0.0f, 1.0f, 0.0f};
    std::array<float, 3> BaseColor = {1.0f, 1.0f, 1.0f};
    std::array<float, 2> TexCoord0 = {};
    std::array<float, 2> TexCoord1 = {};
};

static_assert(sizeof(TrSceneRenderVertex) == sizeof(float) * 13);

struct TrSceneRenderDraw
{
    std::uint32_t FirstIndex = 0;
    std::uint32_t IndexCount = 0;
    std::uint32_t MaterialIndex = TrInvalidSceneIndex;
};

struct TrSceneRenderMesh
{
    std::vector<TrSceneRenderVertex> Vertices;
    std::vector<std::uint32_t> Indices;
    std::vector<TrSceneRenderDraw> Draws;
    std::array<float, 3> BoundsCenter = {};
    float BoundsRadius = 1.0f;
};

class TrScene
{
public:
    static constexpr std::uint32_t FileVersion = 1;

    std::string Name;
    std::string SourceGenerator;
    std::vector<TrSceneMesh> Meshes;
    std::vector<TrSceneMaterial> Materials;
    std::vector<TrSceneImage> Images;
    std::vector<TrSceneSampler> Samplers;
    std::vector<TrSceneTexture> Textures;
    std::vector<TrSceneLight> Lights;
    std::vector<TrSceneCamera> Cameras;
    std::vector<TrSceneNode> Nodes;
    std::vector<std::uint32_t> RootNodes;

    void Validate() const;
    void Save(const std::filesystem::path& path) const;
    static TrScene Load(const std::filesystem::path& path);
    TrSceneRenderMesh BuildStaticRenderMesh() const;
};
