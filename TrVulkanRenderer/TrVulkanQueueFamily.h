#pragma once
#include <optional>
#include "TrVulkanUtil.h"


struct TrVulkanQueueFamilyIndices
{
    // c++17 standard, config in C/C++ --- All options --- Additional options, add "/std:c++17" 
    // don't need to use "magic value", use has_value()
    std::optional<uint32_t> mGraphicsFamily;
    std::optional<uint32_t> mPresentFamily;

    bool IsComplete() const;
};