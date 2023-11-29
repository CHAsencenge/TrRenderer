#pragma once
#include "vulkan/vulkan_core.h"

#ifdef __cplusplus
#include <glm/glm.hpp>
// GLSL Type
using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using mat4 = glm::mat4;
using uint = unsigned int;
#endif

struct TrTexture
{
    VkImage mImage = VK_NULL_HANDLE;
    VkDescriptorImageInfo mDescriptor{};
    VkDeviceMemory* mDeviceMemoryPtr = nullptr;
};

struct TrImage
{
    VkImage mImage = VK_NULL_HANDLE;
    VkDeviceMemory* mDeviceMemoryPtr = nullptr;
};

struct TrBuffer
{
    VkBuffer mBuffer = VK_NULL_HANDLE;
    VkDeviceMemory* mDeviceMemoryPtr = nullptr;
};