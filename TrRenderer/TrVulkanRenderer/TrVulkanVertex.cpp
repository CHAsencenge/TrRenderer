#include "TrVulkanVertex.h"

// to gpu, for loading vertex data correctly in graphics memory
VkVertexInputBindingDescription TrVulkanVertex2DBase::GetBindingDescription()
{
    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0; // the binding number that this structure describes
    bindingDescription.stride = sizeof(TrVulkanVertex2DBase);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
}

// std::array  fixed size
std::array<VkVertexInputAttributeDescription, 2> TrVulkanVertex2DBase::GetAttributeDescriptions()
{
    std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions = {};

    attributeDescriptions[0].binding = 0; // the binding number which this attribute takes its data from
    attributeDescriptions[0].location = 0; // shader input location number for this attribute
    attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(TrVulkanVertex2DBase, mPos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(TrVulkanVertex2DBase, mColor);

    return attributeDescriptions;
}

VkVertexInputBindingDescription TrVulkanVertex2DTex::GetBindingDescription()
{
    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0; // the binding number that this structure describes
    bindingDescription.stride = sizeof(TrVulkanVertex2DTex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription; 
}

std::array<VkVertexInputAttributeDescription, 3> TrVulkanVertex2DTex::GetAttributeDescriptions()
{
    std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = {};

    attributeDescriptions[0].binding = 0; // the binding number which this attribute takes its data from
    attributeDescriptions[0].location = 0; // shader input location number for this attribute
    attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(TrVulkanVertex2DTex, mPos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(TrVulkanVertex2DTex, mColor);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(TrVulkanVertex2DTex, mTexCoord);

    return attributeDescriptions;
}
