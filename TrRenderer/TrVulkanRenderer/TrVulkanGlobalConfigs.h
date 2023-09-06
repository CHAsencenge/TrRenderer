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

    enum class RUNTIME_ERROR_ENUM
    {
        CREATE_INSTANCE_FAILED,
        SETUP_DEBUG_MESSENGER_FAILED,
        NO_VALID_DEVICE,
        NO_SUITABLE_DEVICE,
    };
    
    static std::unordered_map<RUNTIME_ERROR_ENUM, const char*> RUNTIME_ERROR_STRING =
    {
        {RUNTIME_ERROR_ENUM::CREATE_INSTANCE_FAILED, "failed to create instance!"},
        {RUNTIME_ERROR_ENUM::SETUP_DEBUG_MESSENGER_FAILED, "failed to setup debug messenger!"},
        {RUNTIME_ERROR_ENUM::NO_VALID_DEVICE, "failed to find GPUs with Vulkan support!"},
        {RUNTIME_ERROR_ENUM::NO_SUITABLE_DEVICE, "failed to find a suitable GPU!"},
    };
}
