#pragma once
#include "TrVulkanRendererBase.h"
#include "nvh/cameramanipulator.hpp"
#include "nvvkhl/appbase_vk.hpp"
#include "nvvk/context_vk.hpp"
#include "nvvk/debug_util_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"
#include "TrVulkanModel.h"
#include "nvvk/descriptorsets_vk.hpp"

class TrVulkanRendererRayTracingBase : public TrVulkanRendererBase, public nvvkhl::AppBaseVk
{
public:
    TrVulkanRendererRayTracingBase();

    TrVulkanRendererRayTracingBase(uint32_t width, uint32_t height, const char* title);
    
#pragma region TrVulkanRendererBase
    
    void Run() override;

    void OnInitWindow() override;
    
    void OnInitVulkan() override;

    void OnRender() override;
    
    void OnCleanup() override;

    // member instance is in AppBaseVk 
    void CreateInstance() override; 

    std::vector<const char*> GetRequiredExtensions() override;

#pragma endregion


#pragma region AppBaseVk

    void setup(const VkInstance& instance, const VkDevice& device, const VkPhysicalDevice& physicalDevice, uint32_t graphicsQueueIndex) override;

#pragma endregion


#pragma region nvvk

    void IsSurfaceSupportPresent();

#pragma endregion 

#pragma region Camera

    // nvh singleton camera manipulator
    void SetupCamera();

#pragma endregion

#pragma region TrVulkanRendererRayTracingBase

    void LoadModel(const std::string& filename, nvmath::mat4f transform = nvmath::mat4f(1));

    void CreateTextureImages(const VkCommandBuffer cmdBuffer, const std::vector<std::string> textures);

    void CreateOffscreenRender();

    void CreateDescriptorSetLayout();

    void CreateGraphicsPipeline();

    void CreateUniformBuffer();

    void CreateObjDescriptionBuffer();

    void UpdateDescriptorSet();

    void CreatePostDescriptor();

    void CreatePostPipeline();

    void UpdatePostDescriptorSet();

#pragma endregion 

protected:
    nvmath::vec3f mEye = TrVulkanGlobalRT::camEye;
    
    nvmath::vec3f mCenter = TrVulkanGlobalRT::camCenter;
    
    nvmath::vec3f mUp = TrVulkanGlobalRT::camUp;
    

    nvvk::Context mNvContext = {};

    VkSurfaceKHR mSurface;

    // DMA means device memory allocator
    nvvk::ResourceAllocatorDma mResourceAllocDma;

    nvvk::DebugUtil mDebugger; // can set object name, give label

    std::vector<nvvk::Texture> mTextures;

    std::vector<TrObjModelRtBase> mObjModels;

    std::vector<TrObjDescRtBase> mObjDescs;

    std::vector<TrObjInstanceRtBase> mObjInstances;

    nvvk::Texture mOffscreenColorTex;

    nvvk::Texture mOffscreenDepthTex;

    VkRenderPass mOffscreenRenderPass{VK_NULL_HANDLE};

    VkFramebuffer mOffscreenFrameBuffer{VK_NULL_HANDLE};

    VkFormat mOffscreenColorFormat {VK_FORMAT_R32G32B32A32_SFLOAT};

    VkFormat mOffscreenDepthFormat {VK_FORMAT_X8_D24_UNORM_PACK32};

    nvvk::DescriptorSetBindings mDescSetLayoutBindings;

    nvvk::Buffer mBufferGlobals;

    nvvk::Buffer mBufferObjDesc;

    nvvk::DescriptorSetBindings mPostDescSetLayoutBindings;

    
    
};

