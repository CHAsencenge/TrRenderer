#pragma once

#include "TrVulkanUtil.h"

struct TrVulkanQueueFamilyIndices
{
    int mGraphicsFamily = -1;

    bool IsComplete() const;
};