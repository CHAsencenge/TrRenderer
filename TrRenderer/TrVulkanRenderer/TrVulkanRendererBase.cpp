#include "TrVulkanRendererBase.h"

TrVulkanRendererBase::TrVulkanRendererBase()
{
}

TrVulkanRendererBase::TrVulkanRendererBase(uint32_t width, uint32_t height, const char* title) :
	mWidth(width),
	mHeight(height),
	mTitle(title)
{
}
