#pragma once
#include <unordered_map>
#include <vector>

// make variables static, prevent from defining multiple times in each cpp including this header
namespace TrVulkanGlobal
{
    static const std::vector<const char*> validationLayers =
    {
        "VK_LAYER_KHRONOS_validation"
    };

    // device extensions needed
    static const std::vector<const char*> deviceExtensions =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    enum class RUNTIME_ERROR_ENUM
    {
        CREATE_INSTANCE_FAILED,
        SETUP_DEBUG_MESSENGER_FAILED,
        NO_VALID_DEVICE,
        NO_SUITABLE_DEVICE,
        CREATE_LOGICAL_DEVICE_FAILED,
        CREATE_WINDOW_SURFACE_FAILED,
        CREATE_SWAPCHAIN_FAILED,
        CREATE_IMAGE_VIEW_FAILED,
        CREATE_SHADER_MODULE_FAILED,
        CREATE_LAYOUT_FAILED,
        CREATE_RENDER_PASS_FAILED,

        OPEN_FILE_FAILED,
    };
    
    static std::unordered_map<RUNTIME_ERROR_ENUM, const char*> RUNTIME_ERROR_STRING =
    {
        {RUNTIME_ERROR_ENUM::CREATE_INSTANCE_FAILED, "failed to create instance!"},
        {RUNTIME_ERROR_ENUM::SETUP_DEBUG_MESSENGER_FAILED, "failed to setup debug messenger!"},
        {RUNTIME_ERROR_ENUM::NO_VALID_DEVICE, "failed to find GPUs with Vulkan support!"},
        {RUNTIME_ERROR_ENUM::NO_SUITABLE_DEVICE, "failed to find a suitable GPU!"},
        {RUNTIME_ERROR_ENUM::CREATE_LOGICAL_DEVICE_FAILED, "failed to create logical device!"},
        {RUNTIME_ERROR_ENUM::CREATE_WINDOW_SURFACE_FAILED, "failed to create window surface!"},
        {RUNTIME_ERROR_ENUM::CREATE_SWAPCHAIN_FAILED, "failed to create swap chain!"},
        {RUNTIME_ERROR_ENUM::CREATE_IMAGE_VIEW_FAILED, "failed to create image view!"},
        {RUNTIME_ERROR_ENUM::CREATE_SHADER_MODULE_FAILED, "failed to create shader module!"},
        {RUNTIME_ERROR_ENUM::CREATE_LAYOUT_FAILED, "failed to create pipeline layout!"},
        {RUNTIME_ERROR_ENUM::CREATE_RENDER_PASS_FAILED, "failed to create render pass!"},
        
        {RUNTIME_ERROR_ENUM::OPEN_FILE_FAILED, "failed to open file!"},
    };
}
