#include "TrScene.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace
{
    constexpr char SceneMagic[8] = {'T', 'R', 'S', 'C', 'E', 'N', 'E', '\0'};
    constexpr std::uint32_t MaximumElementCount = 100000000;
    constexpr std::uint64_t MaximumBlobSize = 8ull * 1024ull * 1024ull * 1024ull;
    constexpr std::uint32_t MaximumStringSize = 1024 * 1024;

    class BinaryWriter
    {
    public:
        explicit BinaryWriter(const std::filesystem::path& path) :
            mStream(path, std::ios::binary | std::ios::trunc)
        {
            if(!mStream)
            {
                throw std::runtime_error("Failed to open output Scene file.");
            }
        }

        template<typename T>
        void Value(const T& value)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            Bytes(&value, sizeof(T));
        }

        void Bytes(const void* data, std::size_t size)
        {
            if(size != 0)
            {
                mStream.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
            }
            if(!mStream)
            {
                throw std::runtime_error("Failed while writing Scene file.");
            }
        }

        void String(const std::string& value)
        {
            if(value.size() > MaximumStringSize)
            {
                throw std::length_error("Scene string is too large.");
            }
            const auto size = static_cast<std::uint32_t>(value.size());
            Value(size);
            Bytes(value.data(), value.size());
        }

        template<typename T>
        void PodVector(const std::vector<T>& values)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            if(values.size() > MaximumElementCount)
            {
                throw std::length_error("Scene vector is too large.");
            }
            const auto count = static_cast<std::uint32_t>(values.size());
            Value(count);
            Bytes(values.data(), values.size() * sizeof(T));
        }

    private:
        std::ofstream mStream;
    };

    class BinaryReader
    {
    public:
        explicit BinaryReader(const std::filesystem::path& path) :
            mStream(path, std::ios::binary)
        {
            if(!mStream)
            {
                throw std::runtime_error("Failed to open Scene file.");
            }
        }

        template<typename T>
        T Value()
        {
            static_assert(std::is_trivially_copyable_v<T>);
            T value = {};
            Bytes(&value, sizeof(T));
            return value;
        }

        void Bytes(void* data, std::size_t size)
        {
            if(size != 0)
            {
                mStream.read(static_cast<char*>(data), static_cast<std::streamsize>(size));
            }
            if(!mStream)
            {
                throw std::runtime_error("Scene file is truncated or corrupt.");
            }
        }

        std::string String()
        {
            const std::uint32_t size = Value<std::uint32_t>();
            if(size > MaximumStringSize)
            {
                throw std::runtime_error("Scene string exceeds the safety limit.");
            }
            std::string result(size, '\0');
            Bytes(result.data(), size);
            return result;
        }

        template<typename T>
        std::vector<T> PodVector()
        {
            static_assert(std::is_trivially_copyable_v<T>);
            const std::uint32_t count = Value<std::uint32_t>();
            if(count > MaximumElementCount ||
               static_cast<std::uint64_t>(count) * sizeof(T) > MaximumBlobSize)
            {
                throw std::runtime_error("Scene vector exceeds the safety limit.");
            }
            std::vector<T> result(count);
            Bytes(result.data(), result.size() * sizeof(T));
            return result;
        }

    private:
        std::ifstream mStream;
    };

    template<typename T>
    void WriteObjectVector(BinaryWriter& writer, const std::vector<T>& values, void(*write)(BinaryWriter&, const T&))
    {
        if(values.size() > MaximumElementCount)
        {
            throw std::length_error("Scene object vector is too large.");
        }
        writer.Value(static_cast<std::uint32_t>(values.size()));
        for(const T& value : values)
        {
            write(writer, value);
        }
    }

    template<typename T>
    std::vector<T> ReadObjectVector(BinaryReader& reader, T(*read)(BinaryReader&))
    {
        const std::uint32_t count = reader.Value<std::uint32_t>();
        if(count > MaximumElementCount)
        {
            throw std::runtime_error("Scene object count exceeds the safety limit.");
        }
        std::vector<T> result;
        result.reserve(count);
        for(std::uint32_t index = 0; index < count; ++index)
        {
            result.push_back(read(reader));
        }
        return result;
    }

    void WriteTextureBinding(BinaryWriter& writer, const TrSceneTextureBinding& binding)
    {
        writer.Value(binding.TextureIndex);
        writer.Value(binding.TexCoord);
        writer.Value(binding.Strength);
        writer.Value(binding.Offset);
        writer.Value(binding.Scale);
        writer.Value(binding.Rotation);
    }

    TrSceneTextureBinding ReadTextureBinding(BinaryReader& reader)
    {
        TrSceneTextureBinding binding;
        binding.TextureIndex = reader.Value<std::int32_t>();
        binding.TexCoord = reader.Value<std::int32_t>();
        binding.Strength = reader.Value<float>();
        binding.Offset = reader.Value<std::array<float, 2>>();
        binding.Scale = reader.Value<std::array<float, 2>>();
        binding.Rotation = reader.Value<float>();
        return binding;
    }

    void WriteMaterial(BinaryWriter& writer, const TrSceneMaterial& material)
    {
        writer.String(material.Name);
        writer.Value(material.BaseColorFactor);
        writer.Value(material.EmissiveFactor);
        writer.Value(material.MetallicFactor);
        writer.Value(material.RoughnessFactor);
        writer.Value(material.EmissiveStrength);
        writer.Value(material.AlphaCutoff);
        writer.Value(static_cast<std::uint32_t>(material.AlphaMode));
        writer.Value(static_cast<std::uint8_t>(material.DoubleSided));
        writer.Value(static_cast<std::uint8_t>(material.Unlit));
        WriteTextureBinding(writer, material.BaseColorTexture);
        WriteTextureBinding(writer, material.MetallicRoughnessTexture);
        WriteTextureBinding(writer, material.NormalTexture);
        WriteTextureBinding(writer, material.OcclusionTexture);
        WriteTextureBinding(writer, material.EmissiveTexture);
    }

    TrSceneMaterial ReadMaterial(BinaryReader& reader)
    {
        TrSceneMaterial material;
        material.Name = reader.String();
        material.BaseColorFactor = reader.Value<std::array<float, 4>>();
        material.EmissiveFactor = reader.Value<std::array<float, 3>>();
        material.MetallicFactor = reader.Value<float>();
        material.RoughnessFactor = reader.Value<float>();
        material.EmissiveStrength = reader.Value<float>();
        material.AlphaCutoff = reader.Value<float>();
        material.AlphaMode = static_cast<TrSceneAlphaMode>(reader.Value<std::uint32_t>());
        material.DoubleSided = reader.Value<std::uint8_t>() != 0;
        material.Unlit = reader.Value<std::uint8_t>() != 0;
        material.BaseColorTexture = ReadTextureBinding(reader);
        material.MetallicRoughnessTexture = ReadTextureBinding(reader);
        material.NormalTexture = ReadTextureBinding(reader);
        material.OcclusionTexture = ReadTextureBinding(reader);
        material.EmissiveTexture = ReadTextureBinding(reader);
        return material;
    }

    void WriteImage(BinaryWriter& writer, const TrSceneImage& image)
    {
        writer.String(image.Name);
        writer.String(image.MimeType);
        writer.PodVector(image.Data);
    }

    TrSceneImage ReadImage(BinaryReader& reader)
    {
        TrSceneImage image;
        image.Name = reader.String();
        image.MimeType = reader.String();
        image.Data = reader.PodVector<std::uint8_t>();
        return image;
    }

    void WriteSampler(BinaryWriter& writer, const TrSceneSampler& sampler)
    {
        writer.String(sampler.Name);
        writer.Value(sampler.MagFilter);
        writer.Value(sampler.MinFilter);
        writer.Value(sampler.WrapU);
        writer.Value(sampler.WrapV);
    }

    TrSceneSampler ReadSampler(BinaryReader& reader)
    {
        TrSceneSampler sampler;
        sampler.Name = reader.String();
        sampler.MagFilter = reader.Value<std::uint32_t>();
        sampler.MinFilter = reader.Value<std::uint32_t>();
        sampler.WrapU = reader.Value<std::uint32_t>();
        sampler.WrapV = reader.Value<std::uint32_t>();
        return sampler;
    }

    void WriteTexture(BinaryWriter& writer, const TrSceneTexture& texture)
    {
        writer.String(texture.Name);
        writer.Value(texture.ImageIndex);
        writer.Value(texture.SamplerIndex);
    }

    TrSceneTexture ReadTexture(BinaryReader& reader)
    {
        TrSceneTexture texture;
        texture.Name = reader.String();
        texture.ImageIndex = reader.Value<std::int32_t>();
        texture.SamplerIndex = reader.Value<std::int32_t>();
        return texture;
    }

    void WriteMesh(BinaryWriter& writer, const TrSceneMesh& mesh)
    {
        writer.String(mesh.Name);
        writer.PodVector(mesh.Vertices);
        writer.PodVector(mesh.Indices);
        writer.PodVector(mesh.Primitives);
    }

    TrSceneMesh ReadMesh(BinaryReader& reader)
    {
        TrSceneMesh mesh;
        mesh.Name = reader.String();
        mesh.Vertices = reader.PodVector<TrSceneVertex>();
        mesh.Indices = reader.PodVector<std::uint32_t>();
        mesh.Primitives = reader.PodVector<TrScenePrimitive>();
        return mesh;
    }

    void WriteLight(BinaryWriter& writer, const TrSceneLight& light)
    {
        writer.String(light.Name);
        writer.Value(static_cast<std::uint32_t>(light.Type));
        writer.Value(light.Color);
        writer.Value(light.Intensity);
        writer.Value(light.Range);
        writer.Value(light.InnerConeAngle);
        writer.Value(light.OuterConeAngle);
    }

    TrSceneLight ReadLight(BinaryReader& reader)
    {
        TrSceneLight light;
        light.Name = reader.String();
        light.Type = static_cast<TrSceneLightType>(reader.Value<std::uint32_t>());
        light.Color = reader.Value<std::array<float, 3>>();
        light.Intensity = reader.Value<float>();
        light.Range = reader.Value<float>();
        light.InnerConeAngle = reader.Value<float>();
        light.OuterConeAngle = reader.Value<float>();
        return light;
    }

    void WriteCamera(BinaryWriter& writer, const TrSceneCamera& camera)
    {
        writer.String(camera.Name);
        writer.Value(static_cast<std::uint32_t>(camera.Type));
        writer.Value(camera.AspectRatio);
        writer.Value(camera.VerticalFieldOfView);
        writer.Value(camera.NearPlane);
        writer.Value(camera.FarPlane);
        writer.Value(camera.HorizontalMagnification);
        writer.Value(camera.VerticalMagnification);
    }

    TrSceneCamera ReadCamera(BinaryReader& reader)
    {
        TrSceneCamera camera;
        camera.Name = reader.String();
        camera.Type = static_cast<TrSceneCameraType>(reader.Value<std::uint32_t>());
        camera.AspectRatio = reader.Value<float>();
        camera.VerticalFieldOfView = reader.Value<float>();
        camera.NearPlane = reader.Value<float>();
        camera.FarPlane = reader.Value<float>();
        camera.HorizontalMagnification = reader.Value<float>();
        camera.VerticalMagnification = reader.Value<float>();
        return camera;
    }

    void WriteNode(BinaryWriter& writer, const TrSceneNode& node)
    {
        writer.String(node.Name);
        writer.Value(node.ParentIndex);
        writer.PodVector(node.Children);
        writer.Value(node.MeshIndex);
        writer.Value(node.LightIndex);
        writer.Value(node.CameraIndex);
        writer.Value(node.LocalTransform);
        writer.Value(node.WorldTransform);
    }

    TrSceneNode ReadNode(BinaryReader& reader)
    {
        TrSceneNode node;
        node.Name = reader.String();
        node.ParentIndex = reader.Value<std::uint32_t>();
        node.Children = reader.PodVector<std::uint32_t>();
        node.MeshIndex = reader.Value<std::uint32_t>();
        node.LightIndex = reader.Value<std::uint32_t>();
        node.CameraIndex = reader.Value<std::uint32_t>();
        node.LocalTransform = reader.Value<std::array<float, 16>>();
        node.WorldTransform = reader.Value<std::array<float, 16>>();
        return node;
    }

    bool IsFinite(float value)
    {
        return std::isfinite(value);
    }

    std::array<float, 3> TransformPosition(
        const std::array<float, 3>& value,
        const std::array<float, 16>& matrix)
    {
        return
        {
            value[0] * matrix[0] + value[1] * matrix[4] + value[2] * matrix[8] + matrix[12],
            value[0] * matrix[1] + value[1] * matrix[5] + value[2] * matrix[9] + matrix[13],
            value[0] * matrix[2] + value[1] * matrix[6] + value[2] * matrix[10] + matrix[14]
        };
    }

    std::array<float, 16> MultiplyMatrix(
        const std::array<float, 16>& left,
        const std::array<float, 16>& right)
    {
        std::array<float, 16> result = {};
        for(std::size_t row = 0; row < 4; ++row)
        {
            for(std::size_t column = 0; column < 4; ++column)
            {
                for(std::size_t inner = 0; inner < 4; ++inner)
                {
                    result[row * 4 + column] +=
                        left[row * 4 + inner] * right[inner * 4 + column];
                }
            }
        }
        return result;
    }

}

void TrScene::Validate() const
{
    auto validateIndex = [](std::uint32_t index, std::size_t size, const char* message)
    {
        if(index != TrInvalidSceneIndex && index >= size)
        {
            throw std::runtime_error(message);
        }
    };
    auto validateTexture = [this](const TrSceneTextureBinding& binding)
    {
        if(binding.TextureIndex < -1 ||
           (binding.TextureIndex >= 0 && static_cast<std::size_t>(binding.TextureIndex) >= Textures.size()))
        {
            throw std::runtime_error("Scene material references an invalid texture.");
        }
    };

    for(const TrSceneMaterial& material : Materials)
    {
        validateTexture(material.BaseColorTexture);
        validateTexture(material.MetallicRoughnessTexture);
        validateTexture(material.NormalTexture);
        validateTexture(material.OcclusionTexture);
        validateTexture(material.EmissiveTexture);
    }
    for(const TrSceneTexture& texture : Textures)
    {
        if(texture.ImageIndex < -1 ||
           (texture.ImageIndex >= 0 && static_cast<std::size_t>(texture.ImageIndex) >= Images.size()) ||
           texture.SamplerIndex < -1 ||
           (texture.SamplerIndex >= 0 && static_cast<std::size_t>(texture.SamplerIndex) >= Samplers.size()))
        {
            throw std::runtime_error("Scene texture references an invalid image or sampler.");
        }
    }
    for(const TrSceneMesh& mesh : Meshes)
    {
        for(const TrSceneVertex& vertex : mesh.Vertices)
        {
            for(float value : vertex.Position)
            {
                if(!IsFinite(value))
                {
                    throw std::runtime_error("Scene mesh contains a non-finite position.");
                }
            }
        }
        for(std::uint32_t index : mesh.Indices)
        {
            if(index >= mesh.Vertices.size())
            {
                throw std::runtime_error("Scene mesh contains an invalid index.");
            }
        }
        for(const TrScenePrimitive& primitive : mesh.Primitives)
        {
            if(static_cast<std::uint64_t>(primitive.FirstVertex) + primitive.VertexCount > mesh.Vertices.size() ||
               static_cast<std::uint64_t>(primitive.FirstIndex) + primitive.IndexCount > mesh.Indices.size() ||
               primitive.VertexCount == 0 || primitive.IndexCount == 0 ||
               primitive.IndexCount % 3 != 0)
            {
                throw std::runtime_error("Scene primitive range is invalid.");
            }
            for(std::uint32_t indexOffset = 0;
                indexOffset < primitive.IndexCount;
                ++indexOffset)
            {
                const std::uint64_t vertexIndex =
                    mesh.Indices[primitive.FirstIndex + indexOffset];
                if(vertexIndex < primitive.FirstVertex ||
                   vertexIndex >= static_cast<std::uint64_t>(primitive.FirstVertex) +
                       primitive.VertexCount)
                {
                    throw std::runtime_error(
                        "Scene primitive index escapes its vertex range.");
                }
            }
            validateIndex(primitive.MaterialIndex, Materials.size(), "Scene primitive material is invalid.");
        }
    }
    for(std::size_t nodeIndex = 0; nodeIndex < Nodes.size(); ++nodeIndex)
    {
        const TrSceneNode& node = Nodes[nodeIndex];
        validateIndex(node.ParentIndex, Nodes.size(), "Scene node parent is invalid.");
        validateIndex(node.MeshIndex, Meshes.size(), "Scene node mesh is invalid.");
        validateIndex(node.LightIndex, Lights.size(), "Scene node light is invalid.");
        validateIndex(node.CameraIndex, Cameras.size(), "Scene node camera is invalid.");
        if(node.ParentIndex == nodeIndex)
        {
            throw std::runtime_error("Scene node cannot parent itself.");
        }
        for(float value : node.LocalTransform)
        {
            if(!IsFinite(value))
            {
                throw std::runtime_error("Scene node contains a non-finite local transform.");
            }
        }
        for(float value : node.WorldTransform)
        {
            if(!IsFinite(value))
            {
                throw std::runtime_error("Scene node contains a non-finite world transform.");
            }
        }
        std::vector<std::uint32_t> uniqueChildren = node.Children;
        std::sort(uniqueChildren.begin(), uniqueChildren.end());
        if(std::adjacent_find(uniqueChildren.begin(), uniqueChildren.end()) !=
           uniqueChildren.end())
        {
            throw std::runtime_error("Scene node lists the same child more than once.");
        }
        for(std::uint32_t child : node.Children)
        {
            if(child == TrInvalidSceneIndex || child >= Nodes.size())
            {
                throw std::runtime_error("Scene node child is invalid.");
            }
            if(Nodes[child].ParentIndex != nodeIndex)
            {
                throw std::runtime_error("Scene node parent/child relationship is inconsistent.");
            }
        }
    }

    for(std::size_t nodeIndex = 0; nodeIndex < Nodes.size(); ++nodeIndex)
    {
        const std::uint32_t parentIndex = Nodes[nodeIndex].ParentIndex;
        if(parentIndex == TrInvalidSceneIndex)
        {
            continue;
        }
        const std::vector<std::uint32_t>& siblings = Nodes[parentIndex].Children;
        if(std::count(siblings.begin(), siblings.end(), static_cast<std::uint32_t>(nodeIndex)) != 1)
        {
            throw std::runtime_error("Scene child is missing from its declared parent.");
        }
    }

    std::vector<std::uint8_t> hierarchyState(Nodes.size(), 0);
    for(std::size_t startNode = 0; startNode < Nodes.size(); ++startNode)
    {
        std::uint32_t nodeIndex = static_cast<std::uint32_t>(startNode);
        std::vector<std::uint32_t> chain;
        while(nodeIndex != TrInvalidSceneIndex && hierarchyState[nodeIndex] == 0)
        {
            hierarchyState[nodeIndex] = 1;
            chain.push_back(nodeIndex);
            nodeIndex = Nodes[nodeIndex].ParentIndex;
        }
        if(nodeIndex != TrInvalidSceneIndex && hierarchyState[nodeIndex] == 1)
        {
            throw std::runtime_error("Scene node hierarchy contains a cycle.");
        }
        for(const std::uint32_t chainNode : chain)
        {
            hierarchyState[chainNode] = 2;
        }
    }

    std::vector<std::uint32_t> uniqueRoots = RootNodes;
    std::sort(uniqueRoots.begin(), uniqueRoots.end());
    if(std::adjacent_find(uniqueRoots.begin(), uniqueRoots.end()) != uniqueRoots.end())
    {
        throw std::runtime_error("Scene root node is listed more than once.");
    }
    for(std::uint32_t root : RootNodes)
    {
        if(root == TrInvalidSceneIndex || root >= Nodes.size())
        {
            throw std::runtime_error("Scene root node is invalid.");
        }
        if(Nodes[root].ParentIndex != TrInvalidSceneIndex)
        {
            throw std::runtime_error("Scene root node cannot have a parent.");
        }
    }
}

void TrScene::Save(const std::filesystem::path& path) const
{
    Validate();
    BinaryWriter writer(path);
    writer.Bytes(SceneMagic, sizeof(SceneMagic));
    writer.Value(FileVersion);
    writer.String(Name);
    writer.String(SourceGenerator);
    WriteObjectVector(writer, Meshes, WriteMesh);
    WriteObjectVector(writer, Materials, WriteMaterial);
    WriteObjectVector(writer, Images, WriteImage);
    WriteObjectVector(writer, Samplers, WriteSampler);
    WriteObjectVector(writer, Textures, WriteTexture);
    WriteObjectVector(writer, Lights, WriteLight);
    WriteObjectVector(writer, Cameras, WriteCamera);
    WriteObjectVector(writer, Nodes, WriteNode);
    writer.PodVector(RootNodes);
}

TrScene TrScene::Load(const std::filesystem::path& path)
{
    BinaryReader reader(path);
    char magic[sizeof(SceneMagic)] = {};
    reader.Bytes(magic, sizeof(magic));
    if(std::memcmp(magic, SceneMagic, sizeof(SceneMagic)) != 0)
    {
        throw std::runtime_error("Input is not a Tr Scene file.");
    }
    const std::uint32_t version = reader.Value<std::uint32_t>();
    if(version != FileVersion)
    {
        throw std::runtime_error("Unsupported Tr Scene file version.");
    }

    TrScene scene;
    scene.Name = reader.String();
    scene.SourceGenerator = reader.String();
    scene.Meshes = ReadObjectVector(reader, ReadMesh);
    scene.Materials = ReadObjectVector(reader, ReadMaterial);
    scene.Images = ReadObjectVector(reader, ReadImage);
    scene.Samplers = ReadObjectVector(reader, ReadSampler);
    scene.Textures = ReadObjectVector(reader, ReadTexture);
    scene.Lights = ReadObjectVector(reader, ReadLight);
    scene.Cameras = ReadObjectVector(reader, ReadCamera);
    scene.Nodes = ReadObjectVector(reader, ReadNode);
    scene.RootNodes = reader.PodVector<std::uint32_t>();
    scene.Validate();
    return scene;
}

std::vector<std::uint32_t> TrScene::GetActiveNodeIndices() const
{
    Validate();
    std::vector<std::uint8_t> activeNodes(Nodes.size(), 0);
    std::vector<std::uint32_t> pendingNodes = RootNodes;
    while(!pendingNodes.empty())
    {
        const std::uint32_t nodeIndex = pendingNodes.back();
        pendingNodes.pop_back();
        if(activeNodes[nodeIndex] != 0)
        {
            continue;
        }
        activeNodes[nodeIndex] = 1;
        const TrSceneNode& node = Nodes[nodeIndex];
        pendingNodes.insert(
            pendingNodes.end(),
            node.Children.begin(),
            node.Children.end());
    }

    std::vector<std::uint32_t> result;
    result.reserve(Nodes.size());
    for(std::uint32_t nodeIndex = 0; nodeIndex < Nodes.size(); ++nodeIndex)
    {
        if(activeNodes[nodeIndex] != 0)
        {
            result.push_back(nodeIndex);
        }
    }
    return result;
}

TrSceneBounds TrScene::CalculateWorldBounds() const
{
    const std::vector<std::uint32_t> activeNodes = GetActiveNodeIndices();
    std::vector<std::array<float, 16>> calculatedWorldTransforms(Nodes.size());
    std::vector<std::uint32_t> pendingNodes = RootNodes;
    while(!pendingNodes.empty())
    {
        const std::uint32_t nodeIndex = pendingNodes.back();
        pendingNodes.pop_back();
        const TrSceneNode& node = Nodes[nodeIndex];
        calculatedWorldTransforms[nodeIndex] = node.ParentIndex == TrInvalidSceneIndex
            ? node.LocalTransform
            : MultiplyMatrix(
                node.LocalTransform,
                calculatedWorldTransforms[node.ParentIndex]);
        pendingNodes.insert(
            pendingNodes.end(),
            node.Children.begin(),
            node.Children.end());
    }
    std::array<float, 3> minimum =
    {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()
    };
    std::array<float, 3> maximum =
    {
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()
    };
    bool hasGeometry = false;

    for(const std::uint32_t nodeIndex : activeNodes)
    {
        const TrSceneNode& node = Nodes[nodeIndex];
        if(node.MeshIndex == TrInvalidSceneIndex)
        {
            continue;
        }
        const TrSceneMesh& mesh = Meshes[node.MeshIndex];
        for(const TrScenePrimitive& primitive : mesh.Primitives)
        {
            for(std::uint32_t localVertex = 0;
                localVertex < primitive.VertexCount;
                ++localVertex)
            {
                const TrSceneVertex& vertex =
                    mesh.Vertices[primitive.FirstVertex + localVertex];
                const std::array<float, 3> worldPosition = TransformPosition(
                    vertex.Position,
                    calculatedWorldTransforms[nodeIndex]);
                for(std::size_t axis = 0; axis < 3; ++axis)
                {
                    minimum[axis] = std::min(minimum[axis], worldPosition[axis]);
                    maximum[axis] = std::max(maximum[axis], worldPosition[axis]);
                }
                hasGeometry = true;
            }
        }
    }

    if(!hasGeometry)
    {
        throw std::runtime_error("Scene has no renderable static triangle geometry.");
    }

    TrSceneBounds result;
    for(std::size_t axis = 0; axis < 3; ++axis)
    {
        result.Minimum[axis] = minimum[axis];
        result.Maximum[axis] = maximum[axis];
        result.BoundsCenter[axis] = (minimum[axis] + maximum[axis]) * 0.5f;
    }

    float radiusSquared = 0.0f;
    for(const std::uint32_t nodeIndex : activeNodes)
    {
        const TrSceneNode& node = Nodes[nodeIndex];
        if(node.MeshIndex == TrInvalidSceneIndex)
        {
            continue;
        }
        const TrSceneMesh& mesh = Meshes[node.MeshIndex];
        for(const TrScenePrimitive& primitive : mesh.Primitives)
        {
            for(std::uint32_t localVertex = 0;
                localVertex < primitive.VertexCount;
                ++localVertex)
            {
                const std::array<float, 3> worldPosition = TransformPosition(
                    mesh.Vertices[primitive.FirstVertex + localVertex].Position,
                    calculatedWorldTransforms[nodeIndex]);
                const float x = worldPosition[0] - result.BoundsCenter[0];
                const float y = worldPosition[1] - result.BoundsCenter[1];
                const float z = worldPosition[2] - result.BoundsCenter[2];
                radiusSquared = std::max(radiusSquared, x * x + y * y + z * z);
            }
        }
    }
    result.BoundsRadius = std::max(std::sqrt(radiusSquared), 0.001f);
    return result;
}
