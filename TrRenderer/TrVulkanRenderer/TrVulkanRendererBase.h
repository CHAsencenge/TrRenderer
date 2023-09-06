#pragma once
#include "TrVulkanUtil.h"
#include "TrVulkanQueueFamily.h"

class TrVulkanRendererBase
{
public:

    TrVulkanRendererBase();
    TrVulkanRendererBase(uint32_t width, uint32_t height, const char* title);

    void Run();

#pragma region Debug
    
    static void CheckExtensionSupport();

    static bool CheckValidationLayerSupport();

    // VKAPI_ATTR and VKAPI_CALL ensure the function can be called by Vulkan lib
    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallBack(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

#pragma endregion

private:
    
    void OnInitWindow();

    void OnInitVulkan();
    
    void OnRender();
    
    void OnCleanup();

    void CreateInstance();

    std::vector<const char*> GetRequiredExtensions();

#pragma region Debug
    
    void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

    void SetupDebugMessenger();

#pragma endregion

#pragma region Proxy Function
    
    // proxy function, manually load vkCreateDebugUtilsMessengerEXT
    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);

    // proxy function
    void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

#pragma endregion

#pragma region Device
    // graphics card
    void PickFirstValidPhysicalDevice();

    // weighted scoring by feature
    void PickHighestWeightScorePhysicalDevice();

    bool IsDeviceSuitable(VkPhysicalDevice device);

    uint32_t CalcDeviceSuitabilityScore(VkPhysicalDevice device, VkQueueFlagBits queueFlag);

    void CreateLogicalDevice();

#pragma endregion


#pragma region Queue & Queue Families

    TrVulkanQueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkQueueFlagBits queueFlag);

#pragma endregion


private:
    GLFWwindow* mWindow;
    
    uint32_t mWidth = 1920;
    
    uint32_t mHeight = 1080;
    
    const char* mTitle = "Vulkan";

    VkInstance mInstance;
    
    // validation layers switcher
#ifdef NDEBUG
    const bool mbEnableValidationLayers = false;
#else
    const bool mbEnableValidationLayers = true;
#endif

    // save debug callback info
    VkDebugUtilsMessengerEXT mDebugMessenger;

    // automatically clear when VkInstance is cleared
    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;

    // logical device, interactive interface to the physical device
    // need to specify which queue families used queues belong to 
    VkDevice mDevice;
};
