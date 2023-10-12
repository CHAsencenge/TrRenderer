#pragma once
#include "TrVulkanRendererBase.h"
#include "nvh/cameramanipulator.hpp"

class TrVulkanRendererRayTracing : public TrVulkanRendererBase
{
public:
    TrVulkanRendererRayTracing();

    TrVulkanRendererRayTracing(uint32_t width, uint32_t height, const char* title);
    
#pragma region Core
    
    void Run() override;
    
    void OnInitVulkan() override;

    void OnRender() override;
    
    void OnCleanup() override;

    void CreateInstance() override;

#pragma endregion

#pragma region Camera

    // nvh singleton camera manipulator
    void SetupCamera();

#pragma endregion 
    
};

