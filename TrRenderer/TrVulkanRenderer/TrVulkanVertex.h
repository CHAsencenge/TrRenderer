#pragma once
#include <vulkan/vulkan_core.h>

#include "glm/glm.hpp"
#include <array>

class TrVulkanVertex2DBase
{
public:
    glm::vec2 mPos;
    glm::vec3 mColor;

public:
    static VkVertexInputBindingDescription GetBindingDescription(); 

    static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions();
};


class TrVulkanVertex2DTex : public TrVulkanVertex2DBase
{
public:
    glm::vec2 mTexCoord;

public:
    static VkVertexInputBindingDescription GetBindingDescription();

    static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions();
};


class TrVulkanVertex3DBase
{
public:
    glm::vec3 mPos;
    glm::vec3 mColor;

public:
    bool operator==(const TrVulkanVertex3DBase& other) const;

public:
    static VkVertexInputBindingDescription GetBindingDescription(); 

    static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions();
};


class TrVulkanVertex3DTex : public TrVulkanVertex3DBase
{
public:
    glm::vec2 mTexCoord;

public:
    bool operator==(const TrVulkanVertex3DTex& other) const;

public:
    static VkVertexInputBindingDescription GetBindingDescription();

    static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions();
};
