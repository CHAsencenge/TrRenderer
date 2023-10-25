#include "TrVulkanRendererRayTracing.h"

#include "obj_loader.h"
#include "TrVulkanModel.h"
#include "nvh/fileoperations.hpp"
#include "nvvk/commands_vk.hpp"
#include "nvvk/images_vk.hpp"
#include "nvvk/renderpasses_vk.hpp"


TrVulkanRendererRayTracingBase::TrVulkanRendererRayTracingBase()
{
}

TrVulkanRendererRayTracingBase::TrVulkanRendererRayTracingBase(uint32_t width, uint32_t height, const char* title) :
TrVulkanRendererBase(width, height, title)
{
}

void TrVulkanRendererRayTracingBase::Run()
{
    OnInitWindow();
    OnInitVulkan();
    OnRender();
    OnCleanup();
}

void TrVulkanRendererRayTracingBase::OnInitWindow()
{
    TrVulkanRendererBase::OnInitWindow();

    SetupCamera();
}

void TrVulkanRendererRayTracingBase::OnInitVulkan()
{
    // create application
    CreateInstance();

    // can GCT queue (graphics, compute, transter) present on the surface
    IsSurfaceSupportPresent();

    // resource allocator, debug util, depth format
    setup(mNvContext.m_instance, mNvContext.m_device, mNvContext.m_physicalDevice, mNvContext.m_queueGCT.familyIndex);
    
    // swap chain
    createSwapchain(mSurface, mWidth, mHeight);

    // depth buffer
    createDepthBuffer();

    // default simple render pass
    createRenderPass();

    // frame buffers, in which the image will be rendered
    createFrameBuffers();

    // imgui using sub pass 0
    initGUI(0);

    // load model
    LoadModel(nvh::findFile("media/scenes/cube_multi.obj", TrVulkanGlobalRT::defaultSearchPaths, true));

    // offscreen render

    // descriptor set layout

    // pipeline

    // uniform buffer

    // obj buffer

    // update descriptor set

    // post descriptor

    // post pipeline

    // update post descriptor set

    // imgui init for vulkan
    
}

void TrVulkanRendererRayTracingBase::OnRender()
{

    // imgui new frame

    // imgui style

    // prepare rendering scene

    // update uniform buffer

    // clear screen

    // offscreen render pass

    // post render pass
    
}

void TrVulkanRendererRayTracingBase::OnCleanup()
{
}

void TrVulkanRendererRayTracingBase::CreateInstance()
{
    // extensions and layers
    std::vector<const char*> extensions = GetRequiredExtensions();
    
    // input: extensions, layers, version, 
    nvvk::ContextCreateInfo contextCreateInfo;
    contextCreateInfo.setVersion(1, 2);
    
    for(const char* extension : extensions)
    {
        contextCreateInfo.addInstanceExtension(extension, true);
    }

    for(const char* extension : TrVulkanGlobalRT::deviceExtensions)
    {
        contextCreateInfo.addDeviceExtension(extension);
    }
    
    for(const char* layer : TrVulkanGlobalRT::layers)
    {
        contextCreateInfo.addInstanceLayer(layer, true);
    }

    // create vulkan instance
    mNvContext.initInstance(contextCreateInfo);

    // create device
    std::vector<uint32_t> deviceIndices = mNvContext.getCompatibleDevices(contextCreateInfo);
    assert(!deviceIndices.empty());
    mNvContext.initDevice(deviceIndices[0], contextCreateInfo);

    
}

std::vector<const char*> TrVulkanRendererRayTracingBase::GetRequiredExtensions()
{
    std::vector<const char*> extensions = TrVulkanRendererBase::GetRequiredExtensions();
    return extensions;
}

void TrVulkanRendererRayTracingBase::setup(const VkInstance& instance, const VkDevice& device,
    const VkPhysicalDevice& physicalDevice, uint32_t graphicsQueueIndex)
{
    // both AppBaseVk and nvvk::Context have these references
    AppBaseVk::setup(instance, device, physicalDevice, graphicsQueueIndex);
    mResourceAllocDma.init(instance, device, physicalDevice);
    mDebugger.setup(device);
    mOffscreenDepthFormat = nvvk::findDepthFormat(physicalDevice);
}

void TrVulkanRendererRayTracingBase::IsSurfaceSupportPresent()
{
    mSurface = getVkSurface(mNvContext.m_instance, mWindow);
    // determine whether a queue family of a physical device supports presentation to a given surface
    bool bSupportPresent = mNvContext.setGCTQueueWithPresent(mSurface);
    assert(bSupportPresent);
}

void TrVulkanRendererRayTracingBase::SetupCamera()
{
    CameraManip.setWindowSize(mWidth, mHeight);
    CameraManip.setLookat(mEye, mCenter, mUp);
    
}

void TrVulkanRendererRayTracingBase::LoadModel(const std::string& filename, nvmath::mat4f transform)
{
    ObjLoader objLoader;
    objLoader.loadModel(filename);

    // material tone mapping, from srgb to linear
    for(MaterialObj mat : objLoader.m_materials)
    {
        mat.diffuse = nvmath::pow(mat.diffuse, 2.2f);
        mat.specular = nvmath::pow(mat.specular, 2.2f);
        mat.ambient= nvmath::pow(mat.ambient, 2.2f);
    }

    // create buffers for model vertices, indices, material colors, material indices
    nvvk::CommandPool cmdPool(m_device, m_graphicsQueueIndex);
    VkCommandBuffer cmdBuffer = cmdPool.createCommandBuffer();
    
    TrObjModelRtBase model;
    model.mNumIndices = static_cast<uint32_t>(objLoader.m_indices.size());
    model.mNumVertices = static_cast<uint32_t>(objLoader.m_vertices.size());

    // use vkGetBufferDeviceAddress can retrieve buffer, and can use that address to access buffer's memory from a shader
    VkBufferUsageFlags flag   = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    // VK_BUFFER_USAGE_VERTEX_BUFFER_BIT: buffer is suitable for passing to vkCmdBindVertexBuffers
    model.mVertexBuffer = mResourceAllocDma.createBuffer(cmdBuffer, objLoader.m_vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | flag);
    // VK_BUFFER_USAGE_INDEX_BUFFER_BIT: buffer is suitable for passing to vkCmdBindIndexBuffer
    model.mIndexBuffer = mResourceAllocDma.createBuffer(cmdBuffer, objLoader.m_indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | flag);
    // VK_BUFFER_USAGE_STORAGE_BUFFER_BIT: buffer can be used in VkDescriptorBufferInfo
    // suitable for occupying VK_DESCRIPTOR_TYPE_STORAGE_BUFFER or VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC VkDescriptorSet slot
    model.mMatColorBuffer = mResourceAllocDma.createBuffer(cmdBuffer, objLoader.m_materials, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | flag);
    model.mMatIndexBuffer = mResourceAllocDma.createBuffer(cmdBuffer, objLoader.m_matIndx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | flag);

    // create all textures (nvvk::Texture) for the model, if no textures, create a dummy texture
    auto texOffset = static_cast<uint32_t>(mTextures.size());
    CreateTextureImages(cmdBuffer, objLoader.m_textures);


    // submit and wait


    // staging memory release


    // debug obj name


    // set obj instance transform
}

void TrVulkanRendererRayTracingBase::CreateTextureImages(const VkCommandBuffer cmdBuffer, const std::vector<std::string> textures)
{
    // need: sampler create info, image create info
    VkSamplerCreateInfo samplerCreateInfo {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.maxLod = FLT_MAX;

    VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
    
    // if no textures are present, create a dummy texture to accomodate the pipeline layout
    if(textures.empty() && mTextures.empty())
    {
        nvvk::Texture texture;
        
        std::array<uint8_t, 4> color{255u, 255u, 255u, 255u}; // u means unsigned
        VkDeviceSize size = sizeof(color);
        auto imgExtent = VkExtent2D{1, 1};
        VkImageCreateInfo imgCreateInfo = nvvk::makeImage2DCreateInfo(imgExtent, format);

        nvvk::Image image = mResourceAllocDma.createImage(cmdBuffer, size, color.data(), imgCreateInfo);
        VkImageViewCreateInfo imgViewCreateInfo = nvvk::makeImageViewCreateInfo(image.image, imgCreateInfo);
        texture = mResourceAllocDma.createTexture(image, imgViewCreateInfo, samplerCreateInfo);
    }
    else
    {
        
    }
}



