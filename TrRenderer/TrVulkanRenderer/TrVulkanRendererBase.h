#pragma once
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE  // default glm perspective use opengl depth [-1, 1], now use vulkan depth [0, 1]
#include "TrVulkanUtil.h"
#include "TrVulkanQueueFamily.h"
#include "TrVulkanSwapChain.h"
#include "TrVulkanVertex.h"
#include "TrVulkanDescriptor.h"
#include <glm/gtc/matrix_transform.hpp> // rotate etc
#include <chrono> // time functions
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_glfw.h"
#define GLFW_INCLUDE_NONE


#include "TrVulkanModel.h"


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

    void OnSetupImGui();

    void OnRenderImGui();
    
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

    TrVulkanQueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkQueueFlagBits queueFlag = VK_QUEUE_GRAPHICS_BIT);

#pragma endregion

#pragma region Surface

    void CreateSurface();


#pragma endregion

#pragma region SwapChain

    TrVulkanSwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);

    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    void CreateSwapChain(); // auto create image

    VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

    void CreateImageViews();

    void CleanupSwapChain();

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

    void CreateCommandBuffers();

    void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    // Buffers in Vulkan can save arbitrary data that can be read by graphics memory
    void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags propertyFlags, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

    void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    
    void CreateVertexBuffer();

    void CreateIndexBuffer();

    void CreateUniformBuffers();

    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags propertyFlags);

    VkCommandBuffer BeginSingleTimeCommands();

    void EndSingleTimeCommands(VkCommandBuffer commandBuffer);

    void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

    void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    void CreateDepthResources();

    VkFormat FindDepthFormat();

    VkFormat FindSupportedFormat(const std::vector<VkFormat> candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

#pragma endregion

#pragma region Tick

    void DrawFrame();

    void UpdateUniformBuffer(uint32_t currentImage); 

#pragma endregion

#pragma region Sync

    void CreateSyncObjects();

#pragma endregion

#pragma region Descriptor

    void CreateDescriptorSetLayout();

    void CreateDescriptorPool();

    void CreateDescriptorSets();

#pragma endregion

#pragma region Texture

    void CreateTextureImage();

    void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);

    void CreateTextureImageView();

    void CreateTextureSampler();

#pragma endregion

#pragma region Load

    void LoadModels(std::vector<std::string> filenames);

#pragma endregion

#pragma region ImGui

    void ImGuiCreateFontsTexture(VkCommandBuffer commandBuffer);

    void ImGuiSetFontTexId();

    void ImGuiRecordCommandBuffer(VkCommandBuffer commandBuffer); 

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

    // vector for parallel rendering frame
    std::vector<VkCommandBuffer> mCommandBuffers;

    // sync
    // vector for parallel rendering frame
    std::vector<VkSemaphore> mImageAvailableSemaphores; // can render

    std::vector<VkSemaphore> mRenderFinishedSemaphores; // can present

    std::vector<VkFence> mInFlightFences;

    uint32_t mCurrentFrame = 0;

    VkBuffer mVertexBuffer;

    VkDeviceMemory mVertexBufferMemory;

    VkBuffer mIndexBuffer;

    VkDeviceMemory mIndexBufferMemory;

    // for parallel render
    std::vector<VkBuffer> mUniformBuffers;

    std::vector<VkDeviceMemory> mUniformBuffersMemory;

    std::vector<void*> mUniformBuffersMapped;

    VkDescriptorSetLayout mDescriptorSetLayout;

    VkDescriptorPool mDescriptorPool;

    std::vector<VkDescriptorSet> mDescriptorSets;

    VkImage mTextureImage;

    VkDeviceMemory mTextureImageMemory;

    VkImageView mTextureImageView;

    VkSampler mTextureSampler;

    VkImage mDepthImage;

    VkImageView mDepthImageView;

    VkDeviceMemory mDepthImageMemory;

    std::vector<TrVulkanModelBase> mModels;

    VkImage mFontImage;

    VkImageView mFontImageView;

    VkDeviceMemory mFontImageMemory;

    VkSampler mFontSampler;


};

