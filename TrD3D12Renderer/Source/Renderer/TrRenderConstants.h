#pragma once

#include <DirectXMath.h>
#include <cstdint>

// UE-style logical constant-data domains. A logical domain does not have to
// map to a standalone CBV: the draw domain is implemented as root constants.
namespace TrConstantRegister
{
    constexpr std::uint32_t Scene = 0;
    constexpr std::uint32_t View = 1;
    constexpr std::uint32_t Pass = 2;
    constexpr std::uint32_t Primitive = 3;
    constexpr std::uint32_t Material = 4;
    constexpr std::uint32_t Draw = 5;
}

namespace TrMaterialFlag
{
    constexpr std::uint32_t Unlit = 1u << 0;
    constexpr std::uint32_t DoubleSided = 1u << 1;
    constexpr std::uint32_t AlphaMask = 1u << 2;
    constexpr std::uint32_t AlphaBlend = 1u << 3;
}

// Controls material-independent geometry diagnostics in the GBuffer pass.
enum class TrGeometryVisualization : std::uint32_t
{
    Shaded = 0,
    Hierarchy = 1,
    PrimitiveDraw = 2
};

struct alignas(16) TrSceneConstants
{
    DirectX::XMFLOAT3 LightDirection = {0.0f, 1.0f, 0.0f};
    float LightIntensity = 1.0f;
    DirectX::XMFLOAT3 LightColor = {1.0f, 1.0f, 1.0f};
    float AmbientStrength = 0.22f;
};

struct alignas(16) TrViewConstants
{
    DirectX::XMFLOAT4X4 View;
    DirectX::XMFLOAT4X4 Projection;
    DirectX::XMFLOAT4X4 ViewProjection;
    DirectX::XMFLOAT4X4 InverseViewProjection;
    DirectX::XMFLOAT4X4 PreviousViewProjection;
    DirectX::XMFLOAT3 CameraPosition;
    float NearPlane = 0.1f;
    DirectX::XMFLOAT2 RenderSize;
    DirectX::XMFLOAT2 InverseRenderSize;
    DirectX::XMFLOAT2 TemporalJitter = {0.0f, 0.0f};
    DirectX::XMFLOAT2 PreviousTemporalJitter = {0.0f, 0.0f};
    std::uint32_t FrameNumber = 0;
    float FarPlane = 100.0f;
    DirectX::XMFLOAT2 Padding = {0.0f, 0.0f};
};

struct alignas(16) TrGBufferPassConstants
{
    float BaseColorScale = 1.0f;
    float RoughnessScale = 1.0f;
    float MetallicScale = 1.0f;
    TrGeometryVisualization Visualization = TrGeometryVisualization::Shaded;
};

struct alignas(16) TrPrimitiveConstants
{
    DirectX::XMFLOAT4X4 World;
    DirectX::XMFLOAT4X4 PreviousWorld;
    DirectX::XMFLOAT4X4 WorldInverseTranspose;
    DirectX::XMFLOAT3 BoundsCenter = {0.0f, 0.0f, 0.0f};
    float BoundsRadius = 0.0f;
    std::uint32_t InstanceId = 0;
    std::uint32_t MeshId = 0;
    std::uint32_t ParentNodeId = UINT32_MAX;
    std::uint32_t HierarchyDepth = 0;
};

struct alignas(16) TrMaterialConstants
{
    DirectX::XMFLOAT4 BaseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
    DirectX::XMFLOAT3 EmissiveFactor = {0.0f, 0.0f, 0.0f};
    float EmissiveStrength = 1.0f;
    float Roughness = 0.65f;
    float Metallic = 0.0f;
    float AlphaCutoff = 0.5f;
    std::uint32_t Flags = 0;

    struct alignas(16) TextureTransform
    {
        DirectX::XMFLOAT2 Offset = {0.0f, 0.0f};
        DirectX::XMFLOAT2 Scale = {1.0f, 1.0f};
        float Rotation = 0.0f;
        std::uint32_t TexCoord = 0;
        float Strength = 1.0f;
        float Padding = 0.0f;
    };

    TextureTransform BaseColorTexture;
    TextureTransform MetallicRoughnessTexture;
    TextureTransform NormalTexture;
    TextureTransform OcclusionTexture;
    TextureTransform EmissiveTexture;
};

struct alignas(16) TrDeferredLightingPassConstants
{
    float DirectLightingScale = 1.0f;
    float AmbientLightingScale = 1.0f;
    float IndirectLightingScale = 1.0f;
    float RelativeDepthThreshold = 0.02f;
    float MinimumDepthThreshold = 0.05f;
    float NormalWeightPower = 8.0f;
    DirectX::XMFLOAT2 Padding = {0.0f, 0.0f};
};

struct alignas(16) TrForwardTransparentPassConstants
{
    float DirectLightingScale = 1.0f;
    float AmbientLightingScale = 1.0f;
    DirectX::XMFLOAT2 Padding = {0.0f, 0.0f};
};

struct alignas(16) TrCompositePassConstants
{
    float Exposure = 1.0f;
    float Gamma = 2.2f;
    std::uint32_t VisualizationMode = 0;
    float DepthVisualizationRange = 10.0f;
    float NearPlane = 0.1f;
    float FarPlane = 100.0f;
    DirectX::XMFLOAT2 Padding = {0.0f, 0.0f};
    DirectX::XMFLOAT2 OutputSize = {1.0f, 1.0f};
    DirectX::XMFLOAT2 OutputPadding = {0.0f, 0.0f};
};

struct alignas(16) TrDrawConstants
{
    std::uint32_t PrimitiveId = 0;
    std::uint32_t MaterialId = 0;
    std::uint32_t LocalPrimitiveIndex = 0;
    std::uint32_t Flags = 0;
};

static_assert(sizeof(TrSceneConstants) == 32);
static_assert(sizeof(TrViewConstants) == 384);
static_assert(sizeof(TrGBufferPassConstants) == 16);
static_assert(sizeof(TrPrimitiveConstants) == 224);
static_assert(sizeof(TrMaterialConstants::TextureTransform) == 32);
static_assert(sizeof(TrMaterialConstants) == 208);
static_assert(sizeof(TrDeferredLightingPassConstants) == 32);
static_assert(sizeof(TrForwardTransparentPassConstants) == 16);
static_assert(sizeof(TrCompositePassConstants) == 48);
static_assert(sizeof(TrDrawConstants) == 16);
