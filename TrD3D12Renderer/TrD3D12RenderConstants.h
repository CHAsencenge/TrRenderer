#pragma once

#include <DirectXMath.h>
#include <cstdint>

// UE-style logical constant-data domains. A logical domain does not have to
// map to a standalone CBV: the draw domain is implemented as root constants.
namespace TrD3D12ConstantRegister
{
    constexpr std::uint32_t Scene = 0;
    constexpr std::uint32_t View = 1;
    constexpr std::uint32_t Pass = 2;
    constexpr std::uint32_t Primitive = 3;
    constexpr std::uint32_t Material = 4;
    constexpr std::uint32_t Draw = 5;
}

struct alignas(16) TrD3D12SceneConstants
{
    DirectX::XMFLOAT3 LightDirection = {0.0f, 1.0f, 0.0f};
    float LightIntensity = 1.0f;
    DirectX::XMFLOAT3 LightColor = {1.0f, 1.0f, 1.0f};
    float AmbientStrength = 0.22f;
};

struct alignas(16) TrD3D12ViewConstants
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

struct alignas(16) TrD3D12GBufferPassConstants
{
    float BaseColorScale = 1.0f;
    float RoughnessScale = 1.0f;
    float MetallicScale = 1.0f;
    float Padding = 0.0f;
};

struct alignas(16) TrD3D12PrimitiveConstants
{
    DirectX::XMFLOAT4X4 World;
    DirectX::XMFLOAT4X4 PreviousWorld;
    DirectX::XMFLOAT4X4 WorldInverseTranspose;
    DirectX::XMFLOAT3 BoundsCenter = {0.0f, 0.0f, 0.0f};
    float BoundsRadius = 0.0f;
    std::uint32_t PrimitiveId = 0;
    std::uint32_t PrimitiveFlags = 0;
    DirectX::XMFLOAT2 Padding = {0.0f, 0.0f};
};

struct alignas(16) TrD3D12MaterialConstants
{
    DirectX::XMFLOAT3 BaseColorFactor = {1.0f, 1.0f, 1.0f};
    float Roughness = 0.65f;
    float Metallic = 0.0f;
    float EmissiveStrength = 0.0f;
    DirectX::XMFLOAT2 Padding = {0.0f, 0.0f};
};

struct alignas(16) TrD3D12DeferredLightingPassConstants
{
    float DirectLightingScale = 1.0f;
    float AmbientLightingScale = 1.0f;
    std::uint32_t DebugView = 0;
    float Padding = 0.0f;
};

struct alignas(16) TrD3D12CompositePassConstants
{
    float Exposure = 1.0f;
    float Gamma = 2.2f;
    std::uint32_t DebugView = 0;
    float Padding = 0.0f;
};

struct alignas(16) TrD3D12DrawConstants
{
    std::uint32_t PrimitiveIndex = 0;
    std::uint32_t MaterialIndex = 0;
    std::uint32_t InstanceOffset = 0;
    std::uint32_t Flags = 0;
};

static_assert(sizeof(TrD3D12SceneConstants) == 32);
static_assert(sizeof(TrD3D12ViewConstants) == 384);
static_assert(sizeof(TrD3D12GBufferPassConstants) == 16);
static_assert(sizeof(TrD3D12PrimitiveConstants) == 224);
static_assert(sizeof(TrD3D12MaterialConstants) == 32);
static_assert(sizeof(TrD3D12DeferredLightingPassConstants) == 16);
static_assert(sizeof(TrD3D12CompositePassConstants) == 16);
static_assert(sizeof(TrD3D12DrawConstants) == 16);
