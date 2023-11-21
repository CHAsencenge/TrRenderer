#pragma once
#include "TrVulkanRendererBase.h"
#include "nvh/cameramanipulator.hpp"
#include "nvvkhl/appbase_vk.hpp"
#include "nvvk/context_vk.hpp"
#include "nvvk/debug_util_vk.hpp"
#include "TrVulkanModel.h"
#include "nvvk/descriptorsets_vk.hpp"
#include "shaders/VkRayTracing/host_device.h"
#include "backends/imgui_impl_glfw.h"
#include "imgui.h"
#include <imgui/imgui_helper.h>
#include "imgui/imgui_camera_widget.h"
#include <imgui/backends/imgui_impl_vulkan.h>
#include "TrVulkanModel.h"
#include <stb_image.h>
#include "obj_loader.h"
#include "TrVulkanShader.h"
#include "nvh/fileoperations.hpp"
#include "nvvk/buffers_vk.hpp"
#include "nvvk/commands_vk.hpp"
#include "nvvk/images_vk.hpp"
#include "nvvk/pipeline_vk.hpp"
#include "nvvk/renderpasses_vk.hpp"
#include "nvvk/raytraceKHR_vk.hpp"
#include "../TrSoftwareRenderer/Transform.h"
#include "nvh/alignment.hpp"
#include "nvvk/shaders_vk.hpp"

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

    void onResize(int, int) override;

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

    void RenderUI(nvmath::vec4f clearColor);

    // each frame to update the camera matrix
    void UpdateUniformBuffer(const VkCommandBuffer& cmdBuf);

    void Rasterize(const VkCommandBuffer& cmdBuf);

    void DrawPost(const VkCommandBuffer& cmdBuf);

    void DestroyResources();


#pragma endregion


#pragma region Simple

    void InitRayTracing();

    nvvk::RaytracingBuilderKHR::BlasInput ObjectToVkGeometryKHR(const TrObjModelRtBase& model);

    void CreateBLAS();

    void CreateTLAS();

    void CreateRtDescriptorSet();

    void UpdateRtDescriptorSet();

    void CreateRtPipeline();

    // shader binding table
    void CreateSBT();

    void RayTrace(const VkCommandBuffer& cmdBuf, const nvmath::vec4f& clearColor);
#pragma endregion 

protected:

#pragma region Gui
    
    nvmath::vec3f mEye = TrVulkanGlobalRT::camEye;
    
    nvmath::vec3f mCenter = TrVulkanGlobalRT::camCenter;
    
    nvmath::vec3f mUp = TrVulkanGlobalRT::camUp;

    bool mbUseRayTracer = true;

#pragma endregion 


#pragma region Shader
    
    // Information pushed at each draw call
    TrPushConstantRaster mPushConstantRaster{
          {1},                // Identity matrix
          {10.f, 15.f, 8.f},  // light position
          0,                  // instance Id
          100.f,              // light intensity
          0                   // light type
    };

    TrPushConstantRay mPushConstantRay{};

#pragma endregion Before
    
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

    VkDescriptorPool mDescPool;

    VkDescriptorSetLayout mDescSetLayout;

    // when drawing a scene using different materials, we can group objects by material and order draws by material used
    // A material's pipeline and descriptors only need to be bound when drawing objects of that material
    VkDescriptorSet mDescSet;

    nvvk::Buffer mBufferGlobals;

    nvvk::Buffer mBufferObjDesc;

    nvvk::DescriptorSetBindings mPostDescSetLayoutBindings;

    VkDescriptorSetLayout mPostDescSetLayout;

    VkDescriptorPool mPostDescPool;

    VkDescriptorSet mPostDescSet;

    VkPipelineLayout mPipelineLayout;

    VkPipeline mGraphicsPipeline;

    VkPipelineLayout mPostPipelineLayout;
    
    VkPipeline mPostGraphicsPipeline;

#pragma endregion


#pragma region Simple

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR mRtProperties {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};

    nvvk::RaytracingBuilderKHR mRtBuilder; // helper class as a container for one TLAS referencing an array of BLASes

    nvvk::DescriptorSetBindings mRtDescSetLayoutBindings;

    VkDescriptorPool mRtDescPool;

    VkDescriptorSetLayout mRtDescSetLayout;

    // use external resources referenced by a descriptor set
    // uses a single set of descriptor sets containing all the resources necessary to render the scene
    VkDescriptorSet mRtDescSet;

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> mRtShaderGroupCreateInfos;

    VkPipelineLayout mRtPipelineLayout;

    VkPipeline mRtPipeline;

    nvvk::Buffer mRtSbtBuffer;

    VkStridedDeviceAddressRegionKHR mRaygenRegion{};
    
    VkStridedDeviceAddressRegionKHR mMissRegion{};
    
    VkStridedDeviceAddressRegionKHR mClosestHitRegion{};
    
    VkStridedDeviceAddressRegionKHR mCallRegion{};

#pragma endregion 
    
};

