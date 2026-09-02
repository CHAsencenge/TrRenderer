#include "TrRuntimeScene.h"

#include "Backend/TrUploadContext.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <deque>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace
{
    DirectX::XMFLOAT4X4 CopyMatrix(const std::array<float, 16>& source)
    {
        DirectX::XMFLOAT4X4 result;
        static_assert(sizeof(result) == sizeof(source));
        std::memcpy(&result, source.data(), sizeof(result));
        return result;
    }

    DirectX::XMFLOAT3 CopyPosition(const std::array<float, 3>& source)
    {
        return {source[0], source[1], source[2]};
    }

    TrAxisAlignedBounds TransformBounds(
        const TrAxisAlignedBounds& localBounds,
        const DirectX::XMFLOAT4X4& worldTransform)
    {
        TrAxisAlignedBounds result;
        if(!localBounds.IsValid())
        {
            return result;
        }

        using namespace DirectX;
        const XMMATRIX world = XMLoadFloat4x4(&worldTransform);
        for(std::uint32_t corner = 0; corner < 8; ++corner)
        {
            const XMFLOAT3 localPoint(
                (corner & 1u) != 0 ? localBounds.Maximum.x : localBounds.Minimum.x,
                (corner & 2u) != 0 ? localBounds.Maximum.y : localBounds.Minimum.y,
                (corner & 4u) != 0 ? localBounds.Maximum.z : localBounds.Minimum.z);
            XMFLOAT3 worldPoint;
            XMStoreFloat3(
                &worldPoint,
                XMVector3TransformCoord(XMLoadFloat3(&localPoint), world));
            result.Expand(worldPoint);
        }
        return result;
    }

    bool IsFiniteMatrix(const DirectX::XMFLOAT4X4& matrix)
    {
        const float* values = &matrix._11;
        for(std::size_t index = 0; index < 16; ++index)
        {
            if(!std::isfinite(values[index]))
            {
                return false;
            }
        }
        return true;
    }
}

TrAxisAlignedBounds::TrAxisAlignedBounds()
{
    Reset();
}

void TrAxisAlignedBounds::Reset()
{
    const float maximum = std::numeric_limits<float>::max();
    const float minimum = std::numeric_limits<float>::lowest();
    Minimum = {maximum, maximum, maximum};
    Maximum = {minimum, minimum, minimum};
}

void TrAxisAlignedBounds::Expand(const DirectX::XMFLOAT3& point)
{
    Minimum.x = std::min(Minimum.x, point.x);
    Minimum.y = std::min(Minimum.y, point.y);
    Minimum.z = std::min(Minimum.z, point.z);
    Maximum.x = std::max(Maximum.x, point.x);
    Maximum.y = std::max(Maximum.y, point.y);
    Maximum.z = std::max(Maximum.z, point.z);
}

void TrAxisAlignedBounds::Expand(const TrAxisAlignedBounds& bounds)
{
    if(bounds.IsValid())
    {
        Expand(bounds.Minimum);
        Expand(bounds.Maximum);
    }
}

bool TrAxisAlignedBounds::IsValid() const
{
    return Minimum.x <= Maximum.x &&
        Minimum.y <= Maximum.y &&
        Minimum.z <= Maximum.z;
}

DirectX::XMFLOAT3 TrAxisAlignedBounds::GetCenter() const
{
    if(!IsValid())
    {
        return {};
    }
    return
    {
        (Minimum.x + Maximum.x) * 0.5f,
        (Minimum.y + Maximum.y) * 0.5f,
        (Minimum.z + Maximum.z) * 0.5f
    };
}

float TrAxisAlignedBounds::GetRadius() const
{
    if(!IsValid())
    {
        return 0.0f;
    }
    const float x = (Maximum.x - Minimum.x) * 0.5f;
    const float y = (Maximum.y - Minimum.y) * 0.5f;
    const float z = (Maximum.z - Minimum.z) * 0.5f;
    return std::sqrt(x * x + y * y + z * z);
}

void TrRuntimeScene::Initialize(
    TrUploadContext& uploadContext,
    const TrScene& scene)
{
    scene.Validate();
    const std::vector<std::uint32_t> activeNodes = scene.GetActiveNodeIndices();

    mSourceScene = &scene;
    mMeshes.clear();
    mMeshes.resize(scene.Meshes.size());
    mNodes.clear();
    mNodes.resize(scene.Nodes.size());
    mInstances.clear();
    mNodeToInstance.assign(scene.Nodes.size(), TrInvalidRuntimeId);
    mWorldBounds.Reset();
    mUploadedMeshCount = 0;
    mDrawCount = 0;
    mTransformsDirty = false;

    std::vector<std::uint8_t> activeMask(scene.Nodes.size(), 0);
    std::vector<std::uint8_t> referencedMeshes(scene.Meshes.size(), 0);
    for(const std::uint32_t nodeIndex : activeNodes)
    {
        activeMask[nodeIndex] = 1;
        const TrSceneNode& sourceNode = scene.Nodes[nodeIndex];
        if(sourceNode.MeshIndex != TrInvalidSceneIndex)
        {
            referencedMeshes[sourceNode.MeshIndex] = 1;
        }
    }

    for(std::size_t nodeIndex = 0; nodeIndex < scene.Nodes.size(); ++nodeIndex)
    {
        const TrSceneNode& source = scene.Nodes[nodeIndex];
        TrRuntimeNode& destination = mNodes[nodeIndex];
        destination.Name = source.Name;
        destination.NodeId = static_cast<TrNodeId>(nodeIndex);
        destination.ParentNodeId = source.ParentIndex;
        destination.Children = source.Children;
        destination.MeshId = source.MeshIndex;
        destination.LocalTransform = CopyMatrix(source.LocalTransform);
        destination.CurrentWorldTransform = CopyMatrix(source.WorldTransform);
        destination.PreviousWorldTransform = destination.CurrentWorldTransform;
        destination.Active = activeMask[nodeIndex] != 0;
    }
    RecalculateWorldTransforms();
    for(TrRuntimeNode& node : mNodes)
    {
        node.PreviousWorldTransform = node.CurrentWorldTransform;
        node.DirtyFlags = TrRuntimeNodeDirtyFlags::None;
    }

    std::uint64_t nextPrimitiveId = 0;
    for(std::size_t meshIndex = 0; meshIndex < scene.Meshes.size(); ++meshIndex)
    {
        const TrSceneMesh& source = scene.Meshes[meshIndex];
        const std::uint64_t primitiveBase = nextPrimitiveId;
        nextPrimitiveId += source.Primitives.size();
        if(nextPrimitiveId > TrInvalidRuntimeId)
        {
            throw std::overflow_error("Scene has too many primitives for 32-bit IDs.");
        }

        TrRuntimeMesh& destination = mMeshes[meshIndex];
        destination.MeshId = static_cast<TrMeshId>(meshIndex);
        if(referencedMeshes[meshIndex] == 0)
        {
            continue;
        }
        if(source.Vertices.empty() || source.Indices.empty() || source.Primitives.empty())
        {
            continue;
        }
        if(source.Vertices.size() > std::numeric_limits<UINT>::max() ||
           source.Indices.size() > std::numeric_limits<UINT>::max())
        {
            throw std::overflow_error("Scene mesh exceeds the DX12 mesh count limit.");
        }

        std::vector<TrRuntimeVertex> vertices;
        vertices.reserve(source.Vertices.size());
        for(const TrSceneVertex& vertex : source.Vertices)
        {
            vertices.push_back(
            {
                {vertex.Position[0], vertex.Position[1], vertex.Position[2]},
                {vertex.Normal[0], vertex.Normal[1], vertex.Normal[2]},
                {vertex.Color[0], vertex.Color[1], vertex.Color[2]},
                {vertex.TexCoord0[0], vertex.TexCoord0[1]},
                {vertex.TexCoord1[0], vertex.TexCoord1[1]},
                {vertex.TexCoord2[0], vertex.TexCoord2[1]}
            });
        }

        destination.Geometry = std::make_unique<TrMesh>();
        destination.Geometry->Initialize(
            uploadContext,
            vertices.data(),
            static_cast<UINT>(vertices.size()),
            sizeof(TrRuntimeVertex),
            source.Indices.data(),
            static_cast<UINT>(source.Indices.size()),
            DXGI_FORMAT_R32_UINT);
        destination.Primitives.reserve(source.Primitives.size());
        for(std::size_t primitiveIndex = 0;
            primitiveIndex < source.Primitives.size();
            ++primitiveIndex)
        {
            const TrScenePrimitive& sourcePrimitive = source.Primitives[primitiveIndex];
            TrRuntimePrimitive primitive;
            primitive.PrimitiveId = static_cast<TrPrimitiveId>(
                primitiveBase + primitiveIndex);
            primitive.MeshId = destination.MeshId;
            primitive.LocalPrimitiveIndex = static_cast<std::uint32_t>(primitiveIndex);
            primitive.FirstVertex = sourcePrimitive.FirstVertex;
            primitive.VertexCount = sourcePrimitive.VertexCount;
            primitive.FirstIndex = sourcePrimitive.FirstIndex;
            primitive.IndexCount = sourcePrimitive.IndexCount;
            primitive.MaterialId = sourcePrimitive.MaterialIndex;
            for(std::uint32_t vertexOffset = 0;
                vertexOffset < sourcePrimitive.VertexCount;
                ++vertexOffset)
            {
                primitive.LocalBounds.Expand(CopyPosition(
                    source.Vertices[sourcePrimitive.FirstVertex + vertexOffset].Position));
            }
            destination.LocalBounds.Expand(primitive.LocalBounds);
            destination.Primitives.push_back(primitive);
        }
        ++mUploadedMeshCount;
    }

    mInstances.reserve(activeNodes.size());
    for(const std::uint32_t nodeIndex : activeNodes)
    {
        const TrRuntimeNode& node = mNodes[nodeIndex];
        if(node.MeshId == TrInvalidRuntimeId ||
           mMeshes[node.MeshId].Geometry == nullptr)
        {
            continue;
        }

        TrRuntimeInstance instance;
        instance.InstanceId = node.NodeId;
        instance.NodeId = node.NodeId;
        instance.MeshId = node.MeshId;
        instance.CurrentWorldTransform = node.CurrentWorldTransform;
        instance.PreviousWorldTransform = node.CurrentWorldTransform;
        instance.CurrentWorldBounds = TransformBounds(
            mMeshes[node.MeshId].LocalBounds,
            node.CurrentWorldTransform);
        instance.PreviousWorldBounds = instance.CurrentWorldBounds;
        mDrawCount += mMeshes[node.MeshId].Primitives.size();
        mNodeToInstance[node.NodeId] = static_cast<std::uint32_t>(mInstances.size());
        mInstances.push_back(instance);
    }
    RecalculateInstanceBounds();

    if(mInstances.empty() || mUploadedMeshCount == 0 || mDrawCount == 0 ||
       !mWorldBounds.IsValid())
    {
        throw std::runtime_error("Scene has no renderable mesh instances.");
    }
    Validate();
}

void TrRuntimeScene::BeginFrame()
{
    if(mTransformsDirty)
    {
        throw std::logic_error(
            "UpdateWorldTransforms must be called before advancing the Runtime Scene frame.");
    }

    for(TrRuntimeNode& node : mNodes)
    {
        node.PreviousWorldTransform = node.CurrentWorldTransform;
        node.DirtyFlags = TrRuntimeNodeDirtyFlags::None;
    }
    for(TrRuntimeInstance& instance : mInstances)
    {
        instance.PreviousWorldTransform = instance.CurrentWorldTransform;
        instance.PreviousWorldBounds = instance.CurrentWorldBounds;
        instance.DirtyFlags = TrRuntimeNodeDirtyFlags::None;
    }
}

void TrRuntimeScene::SetNodeLocalTransform(
    TrNodeId nodeId,
    const DirectX::XMFLOAT4X4& localTransform)
{
    if(nodeId >= mNodes.size() || !mNodes[nodeId].Active)
    {
        throw std::out_of_range("Runtime Scene node ID is not active.");
    }
    if(!IsFiniteMatrix(localTransform))
    {
        throw std::invalid_argument("Runtime Scene node transform is not finite.");
    }

    mNodes[nodeId].LocalTransform = localTransform;
    MarkNodeAndDescendantsTransformDirty(nodeId);
    mTransformsDirty = true;
}

bool TrRuntimeScene::UpdateWorldTransforms()
{
    if(!mTransformsDirty)
    {
        return false;
    }

    RecalculateWorldTransforms();
    RecalculateInstanceBounds();
    mTransformsDirty = false;
    return true;
}

void TrRuntimeScene::Validate() const
{
    if(mSourceScene == nullptr || mNodes.size() != mSourceScene->Nodes.size() ||
       mMeshes.size() != mSourceScene->Meshes.size() ||
       mNodeToInstance.size() != mNodes.size())
    {
        throw std::logic_error("Runtime Scene storage does not match its source Scene.");
    }

    std::unordered_set<TrPrimitiveId> primitiveIds;
    std::size_t uploadedMeshCount = 0;
    for(std::size_t meshIndex = 0; meshIndex < mMeshes.size(); ++meshIndex)
    {
        const TrRuntimeMesh& mesh = mMeshes[meshIndex];
        if(mesh.MeshId != meshIndex)
        {
            throw std::logic_error("Runtime Scene MeshID is unstable.");
        }
        if(mesh.Geometry == nullptr)
        {
            continue;
        }
        ++uploadedMeshCount;
        if(!mesh.LocalBounds.IsValid())
        {
            throw std::logic_error("Runtime Scene mesh has no valid local AABB.");
        }
        for(std::size_t primitiveIndex = 0;
            primitiveIndex < mesh.Primitives.size();
            ++primitiveIndex)
        {
            const TrRuntimePrimitive& primitive = mesh.Primitives[primitiveIndex];
            if(primitive.PrimitiveId == TrInvalidRuntimeId ||
               !primitiveIds.insert(primitive.PrimitiveId).second ||
               primitive.MeshId != mesh.MeshId ||
               primitive.LocalPrimitiveIndex != primitiveIndex ||
               !primitive.LocalBounds.IsValid() ||
               (primitive.MaterialId != TrInvalidRuntimeId &&
                primitive.MaterialId >= mSourceScene->Materials.size()))
            {
                throw std::logic_error("Runtime Scene primitive contract is invalid.");
            }
        }
    }
    if(uploadedMeshCount != mUploadedMeshCount)
    {
        throw std::logic_error("Runtime Scene uploaded mesh count is inconsistent.");
    }

    for(std::size_t nodeIndex = 0; nodeIndex < mNodes.size(); ++nodeIndex)
    {
        const TrRuntimeNode& node = mNodes[nodeIndex];
        if(node.NodeId != nodeIndex || !IsFiniteMatrix(node.LocalTransform) ||
           !IsFiniteMatrix(node.CurrentWorldTransform) ||
           !IsFiniteMatrix(node.PreviousWorldTransform))
        {
            throw std::logic_error("Runtime Scene node contract is invalid.");
        }
        if(node.Active &&
           ((node.ParentNodeId == TrInvalidRuntimeId && node.HierarchyDepth != 0) ||
            (node.ParentNodeId != TrInvalidRuntimeId &&
             node.HierarchyDepth != mNodes[node.ParentNodeId].HierarchyDepth + 1)))
        {
            throw std::logic_error("Runtime Scene hierarchy depth is inconsistent.");
        }
    }

    std::size_t drawCount = 0;
    std::unordered_set<TrInstanceId> instanceIds;
    for(std::size_t instanceIndex = 0;
        instanceIndex < mInstances.size();
        ++instanceIndex)
    {
        const TrRuntimeInstance& instance = mInstances[instanceIndex];
        if(instance.InstanceId == TrInvalidRuntimeId ||
           !instanceIds.insert(instance.InstanceId).second ||
           instance.NodeId >= mNodes.size() ||
           instance.MeshId >= mMeshes.size() ||
           mMeshes[instance.MeshId].Geometry == nullptr ||
           !mNodes[instance.NodeId].Active ||
           mNodes[instance.NodeId].MeshId != instance.MeshId ||
           mNodeToInstance[instance.NodeId] != instanceIndex ||
           !instance.CurrentWorldBounds.IsValid() ||
           !instance.PreviousWorldBounds.IsValid())
        {
            throw std::logic_error("Runtime Scene instance contract is invalid.");
        }
        drawCount += mMeshes[instance.MeshId].Primitives.size();
    }
    if(drawCount != mDrawCount || !mWorldBounds.IsValid())
    {
        throw std::logic_error("Runtime Scene draw count or world AABB is inconsistent.");
    }
}

const TrScene& TrRuntimeScene::GetSourceScene() const
{
    if(mSourceScene == nullptr)
    {
        throw std::logic_error("Runtime Scene has not been initialized.");
    }
    return *mSourceScene;
}

const TrRuntimeMesh& TrRuntimeScene::GetMesh(TrMeshId meshId) const
{
    if(meshId >= mMeshes.size() || mMeshes[meshId].Geometry == nullptr)
    {
        throw std::out_of_range("Runtime Scene mesh ID is not renderable.");
    }
    return mMeshes[meshId];
}

const TrRuntimeNode& TrRuntimeScene::GetNode(TrNodeId nodeId) const
{
    if(nodeId >= mNodes.size())
    {
        throw std::out_of_range("Runtime Scene node ID is invalid.");
    }
    return mNodes[nodeId];
}

const TrRuntimeInstance* TrRuntimeScene::FindInstanceByNode(TrNodeId nodeId) const
{
    if(nodeId >= mNodeToInstance.size())
    {
        return nullptr;
    }
    const std::uint32_t instanceIndex = mNodeToInstance[nodeId];
    return instanceIndex == TrInvalidRuntimeId
        ? nullptr
        : &mInstances[instanceIndex];
}

void TrRuntimeScene::MarkNodeAndDescendantsTransformDirty(TrNodeId nodeId)
{
    std::vector<TrNodeId> pending = {nodeId};
    while(!pending.empty())
    {
        const TrNodeId currentId = pending.back();
        pending.pop_back();
        TrRuntimeNode& node = mNodes[currentId];
        if(node.DirtyFlags == TrRuntimeNodeDirtyFlags::Transform)
        {
            continue;
        }
        node.DirtyFlags = TrRuntimeNodeDirtyFlags::Transform;
        pending.insert(pending.end(), node.Children.begin(), node.Children.end());
    }
}

void TrRuntimeScene::RecalculateWorldTransforms()
{
    using namespace DirectX;
    std::deque<TrNodeId> pending;
    for(const std::uint32_t rootNodeId : mSourceScene->RootNodes)
    {
        if(mNodes[rootNodeId].Active)
        {
            pending.push_back(rootNodeId);
        }
    }

    std::size_t updatedNodeCount = 0;
    while(!pending.empty())
    {
        const TrNodeId nodeId = pending.front();
        pending.pop_front();
        TrRuntimeNode& node = mNodes[nodeId];
        const XMMATRIX local = XMLoadFloat4x4(&node.LocalTransform);
        if(node.ParentNodeId == TrInvalidRuntimeId)
        {
            node.HierarchyDepth = 0;
            XMStoreFloat4x4(&node.CurrentWorldTransform, local);
        }
        else
        {
            node.HierarchyDepth = mNodes[node.ParentNodeId].HierarchyDepth + 1;
            const XMMATRIX parentWorld = XMLoadFloat4x4(
                &mNodes[node.ParentNodeId].CurrentWorldTransform);
            XMStoreFloat4x4(
                &node.CurrentWorldTransform,
                XMMatrixMultiply(local, parentWorld));
        }
        ++updatedNodeCount;

        for(const TrNodeId childId : node.Children)
        {
            if(mNodes[childId].Active)
            {
                pending.push_back(childId);
            }
        }
    }

    const std::size_t activeNodeCount = static_cast<std::size_t>(std::count_if(
        mNodes.begin(),
        mNodes.end(),
        [](const TrRuntimeNode& node) { return node.Active; }));
    if(updatedNodeCount != activeNodeCount)
    {
        throw std::runtime_error(
            "Runtime Scene hierarchy is cyclic or disconnected from its declared roots.");
    }
}

void TrRuntimeScene::RecalculateInstanceBounds()
{
    mWorldBounds.Reset();
    for(TrRuntimeInstance& instance : mInstances)
    {
        const TrRuntimeNode& node = mNodes[instance.NodeId];
        instance.CurrentWorldTransform = node.CurrentWorldTransform;
        instance.CurrentWorldBounds = TransformBounds(
            mMeshes[instance.MeshId].LocalBounds,
            instance.CurrentWorldTransform);
        instance.DirtyFlags = node.DirtyFlags;
        mWorldBounds.Expand(instance.CurrentWorldBounds);
    }
}
