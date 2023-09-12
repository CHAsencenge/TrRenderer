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