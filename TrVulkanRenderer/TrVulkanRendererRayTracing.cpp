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
    // extensions and layers

    // create application

    // surface

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
}

std::vector<const char*> TrVulkanRendererRayTracingBase::GetRequiredExtensions()
{
    std::vector<const char*> extensions = TrVulkanRendererBase::GetRequiredExtensions();
    // extension for debugging
    if(mbEnableValidationLayers)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);  // VK_EXT_debug_utils
    }
    
}

void TrVulkanRendererRayTracingBase::SetupCamera()
{
    CameraManip.setWindowSize(mWidth, mHeight);
    CameraManip.setLookat(mEye, mCenter, mUp);
    
}



