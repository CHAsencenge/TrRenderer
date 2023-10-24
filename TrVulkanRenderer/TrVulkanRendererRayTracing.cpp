#include "TrVulkanRendererRayTracing.h"



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

    // swap chain

    // depth buffer

    // render pass

    // frame buffers

    // imgui using sub pass 0

    // load model

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



