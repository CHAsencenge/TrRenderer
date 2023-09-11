#pragma once
#include "TrVulkanUtil.h"
#include "TrVulkanQueueFamily.h"
#include "TrVulkanSwapChain.h"

class TrVulkanRendererBase
{
public:

    TrVulkanRendererBase();
    TrVulkanRendererBase(uint32_t width, uint32_t height, const char* title);

    void Run();

#pragma region Debug
    
    void CheckExtensionSupport();

    bool CheckValidationLayerSupport();

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

#pragma region Extension

    std::vector<const char*> GetRequiredExtensions();

    bool CheckDeviceExtensionSupport(VkPhysicalDevice device);

#pragma endregion 

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
    void PickFirstValidPhysicalDevice(VkQueueFlagBits queueFlag);

    // weighted scoring by feature
    void PickHighestWeightScorePhysicalDevice(VkQueueFlagBits queueFlag);

    bool IsDeviceSuitable(VkPhysicalDevice device, VkQueueFlagBits queueFlag);

    uint32_t CalcDeviceSuitabilityScore(VkPhysicalDevice device, VkQueueFlagBits queueFlag);

    void CreateLogicalDevice();

#pragma endregion

#pragma region Queue & Queue Families

    TrVulkanQueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkQueueFlagBits queueFlag);

#pragma endregion

#pragma region Surface

    void CreateSurface();


#pragma endregion

#pragma region SwapChain

    TrVulkanSwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);

    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    void CreateSwapChain();

    void CreateImageViews();

#pragma endregion

#pragma region Pipeline

    void CreateRenderPass();
    
    void CreateGraphicsPipeline();

#pragma endregion

#pragma region Shader

    VkShaderModule CreateShaderModule(std::vector<char>& compiledCode);

#pragma endregion

#pragma region Buffer

    void CreateFrameBuffers();

    void CreateCommandPool();

    void CreateCommandBuffer();

    void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

#pragma endregion

#pragma region Tick

    void DrawFrame();

#pragma endregion

#pragma region Sync

    void CreateSyncObjects();

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

    VkQueue mGraphicsQueue;

    VkQueue mPresentQueue;

    // platform independent
    // VK_KHR_surface is an instance level extension, has contained in the extension list get by glfwGetRequiredInstanceExtensions()
    // VK_KHR_win32_surface process interaction with window on win platform
    VkSurfaceKHR mSurface;

    VkSwapchainKHR mSwapChain;

    std::vector<VkImage> mSwapChainImages;

    VkFormat mSwapChainImageFormat;

    VkExtent2D mSwapChainExtent;

    // describe an image
    std::vector<VkImageView> mSwapChainImageViews;

    VkRenderPass mRenderPass;
    
    VkPipelineLayout mPipelineLayout;

    VkPipeline mGraphicsPipeline;

    std::vector<VkFramebuffer> mSwapChainFrameBuffers;

    // draw command, memory transfer command...
    VkCommandPool mCommandPool;

    VkCommandBuffer mCommandBuffer;

    // sync
    VkSemaphore mImageAvailableSemaphore; // can render

    VkSemaphore mRenderFinishedSemaphore; // can present

    VkFence mInFlightFence;
};
