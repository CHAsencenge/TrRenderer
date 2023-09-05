#pragma once
#include "TrVulkanUtil.h"


class TrVulkanRendererBase
{
public:

    TrVulkanRendererBase();
    TrVulkanRendererBase(uint32_t width, uint32_t height, const char* title);

    void Run();

    static void CheckExtensionSupport();

    static bool CheckValidationLayerSupport();

    // VKAPI_ATTR and VKAPI_CALL ensure the function can be called by Vulkan lib
    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallBack(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

private:
    GLFWwindow* mWindow;

    void OnInitWindow();

    void OnInitVulkan();
    
    void OnRender();
    
    void OnCleanup();

    void CreateInstance();

    void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

    void SetupDebugMessenger();

    // proxy function, manually load vkCreateDebugUtilsMessengerEXT
    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);

    // proxy function
    void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

    std::vector<const char*> GetRequiredExtensions();


private:
    uint32_t mWidth = 1920;
    uint32_t mHeight = 1080;
    const char* mTitle = "Vulkan";

    VkInstance mInstance;
    // save debug callback info
    VkDebugUtilsMessengerEXT mDebugMessenger;


    // validation layers switcher
#ifdef NDEBUG
    const bool mbEnableValidationLayers = false;
#else
    const bool mbEnableValidationLayers = true;
#endif
};
