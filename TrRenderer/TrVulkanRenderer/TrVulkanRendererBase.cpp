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

void TrVulkanRendererBase::Run()
{
}

void TrVulkanRendererBase::OnInitWindow()
{
    glfwInit();

    // prevent it from automatically creating OpenGL context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    mWindow = glfwCreateWindow(mWidth, mHeight, mTitle, nullptr, nullptr);
}

