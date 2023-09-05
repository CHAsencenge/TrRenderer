#pragma once
#include "TrVulkanUtil.h"


class TrVulkanRendererBase
{
public:

    TrVulkanRendererBase();
    TrVulkanRendererBase(uint32_t width, uint32_t height, const char* title);

    void Run()
    {
        OnInitWindow();
        OnInitVulkan();
        OnRender();
        OnCleanup();
    }

private:
    GLFWwindow* mWindow;

    void OnInitWindow();

    void OnInitVulkan();
    
    void OnRender();
    
    void OnCleanup();

    void CreateInstance();

    void CheckExtensionSupport();
   

private:
    uint32_t mWidth = 1920;
    uint32_t mHeight = 1080;
    const char* mTitle = "Vulkan";

    VkInstance mInstance;
};
