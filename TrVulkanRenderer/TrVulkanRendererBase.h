#pragma once
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "TrVulkanUtil.h"
#define GLFW_INCLUDE_NONE


class TrVulkanRendererBase
{
public:

    TrVulkanRendererBase();
    TrVulkanRendererBase(uint32_t width, uint32_t height, const char* title);

    virtual void Run() = 0;

    virtual void OnInitWindow();

    virtual void OnInitVulkan() = 0;

    virtual void OnRender() = 0;

    virtual void OnCleanup() = 0;

    virtual void CreateInstance() = 0;

    virtual std::vector<const char*> GetRequiredExtensions();

protected:
    GLFWwindow* mWindow;
    
    uint32_t mWidth = 1920;
    
    uint32_t mHeight = 1080;
    
    const char* mTitle = "Vulkan";

    

    // validation layers switcher
#ifdef NODEBUG
    const bool mbEnableValidationLayers = false;
#else
    const bool mbEnableValidationLayers = true;
#endif
    
};
