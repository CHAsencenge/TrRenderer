#pragma once
#include "TrVulkanVertex.h"
#include <tiny_obj_loader.h>
#include "nvvk/resourceallocator_vk.hpp"


class TrVulkanModelBase
{
public:
    TrVulkanModelBase(const std::string& filename, const std::string& modelPath, const std::string& texturePath);

    void LoadModelFromPath();

    inline void SetModelPath(const std::string& modelPath) { mModelPath = modelPath; }

    inline void SetTexturePath(const std::string& texturePath) { mTexturePath = texturePath; }

    std::string mFilename;
    std::string mModelPath;
    std::string mTexturePath;
    
    std::vector<TrVulkanVertex3DTex> mVertices;
    std::vector<uint32_t> mIndices;

    float mScale = 1.0f;
};

template<>
struct std::hash<TrVulkanVertex3DTex>
{
    size_t operator()(TrVulkanVertex3DTex const& vertex) const
    {
        return ((std::hash<glm::vec3>()(vertex.mPos) ^ (std::hash<glm::vec3>()(vertex.mColor) << 1)) >> 1) ^ (std::hash<glm::vec2>()(vertex.mTexCoord) << 1);
    }
};

// Model
struct TrObjModelRtBase
{
    uint32_t mNumIndices{0};
    uint32_t mNumVertices{0};
    nvvk::Buffer mVertexBuffer;
    nvvk::Buffer mIndexBuffer;
    nvvk::Buffer mMatColorBuffer;
    nvvk::Buffer mMatIndexBuffer;
};

struct TrObjInstanceRtBase
{
    glm::mat4 mTransform;
    uint32_t mObjIndex{0};
};


