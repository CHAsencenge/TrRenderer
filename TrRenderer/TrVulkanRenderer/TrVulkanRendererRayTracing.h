#pragma once
#include "TrVulkanRendererBase.h"
#include "nvh/cameramanipulator.hpp"
#include "nvvkhl/appbase_vk.hpp"

class TrVulkanRendererRayTracingBase : public TrVulkanRendererBase, public nvvkhl::AppBaseVk
{
public:
    TrVulkanRendererRayTracingBase();

    TrVulkanRendererRayTracingBase(uint32_t width, uint32_t height, const char* title);
    
#pragma region Core
    
    void Run() override;

    void OnInitWindow() override;
    
    void OnInitVulkan() override;

    void OnRender() override;
    
    void OnCleanup() override;

    void CreateInstance() override;

#pragma endregion

#pragma region Camera

    // nvh singleton camera manipulator
    void SetupCamera();

#pragma endregion

protected:
    nvmath::vec3f mEye = TrVulkanGlobalRT::camEye;
    
    nvmath::vec3f mCenter = TrVulkanGlobalRT::camCenter;
    
    nvmath::vec3f mUp = TrVulkanGlobalRT::camUp;
};

