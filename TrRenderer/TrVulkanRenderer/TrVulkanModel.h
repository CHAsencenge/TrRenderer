#pragma once
#include <vector>
#include "TrVulkanVertex.h"
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>



class TrVulkanModelBase
{
public:
    TrVulkanModelBase(const std::string& modelPath, const std::string& texturePath);

    void LoadModelFromPath();
    
    std::string mModelPath;
    std::string mTexturePath;
    
    std::vector<TrVulkanVertex3DTex> mVertices;
    std::vector<uint32_t> mIndices;
};

template<>
struct std::hash<TrVulkanVertex3DTex>
{
    size_t operator()(TrVulkanVertex3DTex const& vertex) const
    {
        return ((std::hash<glm::vec3>()(vertex.mPos) ^ (std::hash<glm::vec3>()(vertex.mColor) << 1)) >> 1) ^ (std::hash<glm::vec2>()(vertex.mTexCoord) << 1);
    }
};
