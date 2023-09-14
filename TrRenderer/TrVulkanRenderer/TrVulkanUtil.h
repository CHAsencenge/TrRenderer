#pragma once

// vulkan API function, struct, enum
// #include <vulkan/vulkan.h>

// glfw lib has included vulkan header
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// resource management
#include <functional>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include "TrVulkanGlobalConfigs.h"
#include <fstream>

class TrVulkanUtil
{
public:
    static std::vector<char> ReadFile(const std::string& filename);
};

