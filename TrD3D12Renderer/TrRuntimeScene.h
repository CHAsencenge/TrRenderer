#pragma once

#include "TrMesh.h"
#include "TrScene.h"

#include <DirectXMath.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class TrUploadContext;

using TrMeshId = std::uint32_t;
using TrPrimitiveId = std::uint32_t;
using TrInstanceId = std::uint32_t;
using TrMaterialId = std::uint32_t;
using TrNodeId = std::uint32_t;

constexpr std::uint32_t TrInvalidRuntimeId = TrInvalidSceneIndex;

// Local-space vertex layout shared by the raster and future ray-hit paths.
// Instance transforms are deliberately not baked into these vertices.
struct TrRuntimeVertex
{
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT3 Color;
    DirectX::XMFLOAT2 TexCoord0;
    DirectX::XMFLOAT2 TexCoord1;
};

static_assert(sizeof(TrRuntimeVertex) == sizeof(float) * 13);

struct TrAxisAlignedBounds
{
    DirectX::XMFLOAT3 Minimum;
    DirectX::XMFLOAT3 Maximum;

    TrAxisAlignedBounds();

    void Reset();
    void Expand(const DirectX::XMFLOAT3& point);
    void Expand(const TrAxisAlignedBounds& bounds);
    bool IsValid() const;
    DirectX::XMFLOAT3 GetCenter() const;
    float GetRadius() const;
};

struct TrRuntimePrimitive
{
    // Stable scene-wide identity: sum of primitive counts in preceding source
    // meshes plus LocalPrimitiveIndex. It is independent of draw order.
    TrPrimitiveId PrimitiveId = TrInvalidRuntimeId;
    TrMeshId MeshId = TrInvalidRuntimeId;
    std::uint32_t LocalPrimitiveIndex = 0;
    std::uint32_t FirstVertex = 0;
    std::uint32_t VertexCount = 0;
    std::uint32_t FirstIndex = 0;
    std::uint32_t IndexCount = 0;
    TrMaterialId MaterialId = TrInvalidRuntimeId;
    TrAxisAlignedBounds LocalBounds;
};

struct TrRuntimeMesh
{
    TrMeshId MeshId = TrInvalidRuntimeId;
    std::unique_ptr<TrMesh> Geometry;
    std::vector<TrRuntimePrimitive> Primitives;
    TrAxisAlignedBounds LocalBounds;
};

enum class TrRuntimeNodeDirtyFlags : std::uint32_t
{
    None = 0,
    Transform = 1u << 0
};

struct TrRuntimeNode
{
    std::string Name;
    TrNodeId NodeId = TrInvalidRuntimeId;
    TrNodeId ParentNodeId = TrInvalidRuntimeId;
    std::vector<TrNodeId> Children;
    TrMeshId MeshId = TrInvalidRuntimeId;
    std::uint32_t HierarchyDepth = 0;
    DirectX::XMFLOAT4X4 LocalTransform;
    DirectX::XMFLOAT4X4 CurrentWorldTransform;
    DirectX::XMFLOAT4X4 PreviousWorldTransform;
    TrRuntimeNodeDirtyFlags DirtyFlags = TrRuntimeNodeDirtyFlags::None;
    bool Active = false;
};

struct TrRuntimeInstance
{
    // One renderable node currently produces exactly one stable instance.
    // Using the source NodeId avoids renumbering when unrelated nodes change.
    TrInstanceId InstanceId = TrInvalidRuntimeId;
    TrNodeId NodeId = TrInvalidRuntimeId;
    TrMeshId MeshId = TrInvalidRuntimeId;
    DirectX::XMFLOAT4X4 CurrentWorldTransform;
    DirectX::XMFLOAT4X4 PreviousWorldTransform;
    TrAxisAlignedBounds CurrentWorldBounds;
    TrAxisAlignedBounds PreviousWorldBounds;
    TrRuntimeNodeDirtyFlags DirtyFlags = TrRuntimeNodeDirtyFlags::None;
};

class TrRuntimeScene
{
public:
    void Initialize(TrUploadContext& uploadContext, const TrScene& scene);

    // Temporal lifecycle: snapshot current state, edit local transforms, then
    // propagate once. Static scenes only need BeginFrame().
    void BeginFrame();
    void SetNodeLocalTransform(
        TrNodeId nodeId,
        const DirectX::XMFLOAT4X4& localTransform);
    bool UpdateWorldTransforms();
    void Validate() const;

    const TrScene& GetSourceScene() const;
    const TrRuntimeMesh& GetMesh(TrMeshId meshId) const;
    const TrRuntimeNode& GetNode(TrNodeId nodeId) const;
    const TrRuntimeInstance* FindInstanceByNode(TrNodeId nodeId) const;
    const std::vector<TrRuntimeNode>& GetNodes() const { return mNodes; }
    const std::vector<TrRuntimeInstance>& GetInstances() const { return mInstances; }
    const TrAxisAlignedBounds& GetWorldBounds() const { return mWorldBounds; }
    std::size_t GetUploadedMeshCount() const { return mUploadedMeshCount; }
    std::size_t GetDrawCount() const { return mDrawCount; }

private:
    void MarkNodeAndDescendantsTransformDirty(TrNodeId nodeId);
    void RecalculateWorldTransforms();
    void RecalculateInstanceBounds();

    const TrScene* mSourceScene = nullptr;
    std::vector<TrRuntimeMesh> mMeshes;
    std::vector<TrRuntimeNode> mNodes;
    std::vector<TrRuntimeInstance> mInstances;
    std::vector<std::uint32_t> mNodeToInstance;
    TrAxisAlignedBounds mWorldBounds;
    std::size_t mUploadedMeshCount = 0;
    std::size_t mDrawCount = 0;
    bool mTransformsDirty = false;
};
