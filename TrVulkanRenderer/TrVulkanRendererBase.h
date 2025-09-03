#pragma once
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "TrVulkanScene.h"
#include "TrVulkanUtil.h"
#include "nvh/fileoperations.hpp"
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

    virtual void LoadModel(const std::string& filename, uint32_t actorId, glm::mat4 transform = glm::mat4(1));

    void LoadScene();

    virtual void CreateTextureImages(const VkCommandBuffer cmdBuffer, const std::vector<std::string> textures) = 0;


    

protected:
    GLFWwindow* mWindow;
    
    uint32_t mWidth = 1920;
    
    uint32_t mHeight = 1080;
    
    const char* mTitle = "Vulkan";

#pragma region Scene

    TrScene* mScene;

#pragma endregion 

    

    // validation layers switcher
#ifdef NODEBUG
    const bool mbEnableValidationLayers = false;
#else
    const bool mbEnableValidationLayers = true;
#endif
    
};
