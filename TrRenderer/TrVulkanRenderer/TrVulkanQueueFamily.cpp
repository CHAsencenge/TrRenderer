#include "TrVulkanQueueFamily.h"

bool TrVulkanQueueFamilyIndices::IsComplete() const
{
    return mGraphicsFamily.has_value() && mPresentFamily.has_value();
}
