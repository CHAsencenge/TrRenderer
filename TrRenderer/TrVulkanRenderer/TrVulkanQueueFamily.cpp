#include "TrVulkanQueueFamily.h"

bool TrVulkanQueueFamilyIndices::IsComplete() const
{
    return mGraphicsFamily >= 0;
}
