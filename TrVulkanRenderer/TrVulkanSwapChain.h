#pragma once
#include "TrVulkanUtil.h"


// basic surface features: swap chain min/max image number, min/max image width/height
// surface format: pixel format, color space
// present modes
struct TrVulkanSwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR mCapabilities;
    std::vector<VkSurfaceFormatKHR> mFormats;
    std::vector<VkPresentModeKHR> mPresentModes;
};