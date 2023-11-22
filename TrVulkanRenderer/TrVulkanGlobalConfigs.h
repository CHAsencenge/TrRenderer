#pragma once
#include <unordered_map>
#include <vector>
#include <nvmath/nvmath_types.h>

#include "TrVulkanVertex.h"


// if use namespace and make variables static
// can prevent from defining multiple times in each cpp including this header
// but "internal link" makes each file has a TrVulkanGlobal::xxx copy
class TrVulkanGlobal
{
public:
    inline static const std::vector<const char*> validationLayers =
    {
        "VK_LAYER_KHRONOS_validation"
    };

    // device extensions needed
    inline static const std::vector<const char*> deviceExtensions =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        // VK_KHR_MAINTENANCE_1_EXTENSION_NAME,
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
        CREATE_GRAPHICS_PIPELINE_FAILED,
        CREATE_FRAME_BUFFER_FAILED,
        CREATE_COMMAND_POOL_FAILED,
        ALLOCATE_COMMAND_BUFFER_FAILED,
        RECORD_COMMAND_BUFFER_BEGIN_FAILED,
        RECORD_COMMAND_BUFFER_END_FAILED,
        CREATE_SEMAPHORE_FAILED,
        SUBMIT_DRAW_COMMAND_BUFFER_FAILED,
        CREATE_FENCE_FAILED,
        CREATE_VERTEX_BUFFER_FAILED,
        CREATE_BUFFER_FAILED,
        FIND_MEMORY_TYPE_FAILED,
        ALLOCATE_VERTEX_BUFFER_MEMORY_FAILED,
        ALLOCATE_BUFFER_MEMORY_FAILED,
        CREATE_DESCRIPTOR_SET_LAYOUT_FAILED,
        CREATE_DESCRIPTOR_POOL_FAILED,
        ALLOCATE_DESCRIPTOR_SETS_FAILED,
        CREATE_IMAGE_FAILED,
        ALLOCATE_IMAGE_MEMORY_FAILED,
        CREATE_SAMPLER_FAILED,
        FIND_SUPPORTED_FORMAT_FAILED,

        OPEN_FILE_FAILED,
        LOAD_TEXTURE_IMAGE_FAILED,
    };
    
    inline static std::unordered_map<RUNTIME_ERROR_ENUM, const char*> RUNTIME_ERROR_STRING =
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
        {RUNTIME_ERROR_ENUM::CREATE_GRAPHICS_PIPELINE_FAILED, "failed to create graphics pipeline!"},
        {RUNTIME_ERROR_ENUM::CREATE_FRAME_BUFFER_FAILED, "failed to create frame buffer!"},
        {RUNTIME_ERROR_ENUM::CREATE_COMMAND_POOL_FAILED, "failed to create command pool!"},
        {RUNTIME_ERROR_ENUM::ALLOCATE_COMMAND_BUFFER_FAILED, "failed to allocate command buffer!"},
        {RUNTIME_ERROR_ENUM::RECORD_COMMAND_BUFFER_BEGIN_FAILED, "failed to begin record command buffer!"},
        {RUNTIME_ERROR_ENUM::RECORD_COMMAND_BUFFER_END_FAILED, "failed to end record command buffer!"},
        {RUNTIME_ERROR_ENUM::CREATE_SEMAPHORE_FAILED, "failed to create semaphore!"},
        {RUNTIME_ERROR_ENUM::SUBMIT_DRAW_COMMAND_BUFFER_FAILED, "failed to submit draw command buffer!"},
        {RUNTIME_ERROR_ENUM::CREATE_FENCE_FAILED, "failed to create fence!"},
        {RUNTIME_ERROR_ENUM::CREATE_VERTEX_BUFFER_FAILED, "failed to create vertex buffer!"},
        {RUNTIME_ERROR_ENUM::CREATE_BUFFER_FAILED, "failed to create buffer!"},
        {RUNTIME_ERROR_ENUM::FIND_MEMORY_TYPE_FAILED, "failed to find suitable memory type!"},
        {RUNTIME_ERROR_ENUM::ALLOCATE_VERTEX_BUFFER_MEMORY_FAILED, "failed to allocate vertex buffer memory!"},
        {RUNTIME_ERROR_ENUM::ALLOCATE_BUFFER_MEMORY_FAILED, "failed to allocate buffer memory!"},
        {RUNTIME_ERROR_ENUM::CREATE_DESCRIPTOR_SET_LAYOUT_FAILED, "failed to create descriptor set layout!"},
        {RUNTIME_ERROR_ENUM::CREATE_DESCRIPTOR_POOL_FAILED, "failed to create descriptor pool!"},
        {RUNTIME_ERROR_ENUM::ALLOCATE_DESCRIPTOR_SETS_FAILED, "failed to allocate descriptor sets!"},
        {RUNTIME_ERROR_ENUM::CREATE_IMAGE_FAILED, "failed to create image!"},
        {RUNTIME_ERROR_ENUM::ALLOCATE_IMAGE_MEMORY_FAILED, "failed to allocate image memory!"},
        {RUNTIME_ERROR_ENUM::CREATE_SAMPLER_FAILED, "failed to create sampler!"},
        {RUNTIME_ERROR_ENUM::FIND_SUPPORTED_FORMAT_FAILED, "failed to find supported format!"},
        
        
        {RUNTIME_ERROR_ENUM::OPEN_FILE_FAILED, "failed to open file!"},
        {RUNTIME_ERROR_ENUM::LOAD_TEXTURE_IMAGE_FAILED, "failed to load texture image!"},
    };

    enum class INVALID_ARGUMENT_ENUM
    {
        UNSUPPORTED_LAYOUT_TRANSITION,
    };

    inline static std::unordered_map<INVALID_ARGUMENT_ENUM, const char*> INVALID_ARGUMENT_STRING =
    {
        {INVALID_ARGUMENT_ENUM::UNSUPPORTED_LAYOUT_TRANSITION, "unsupported layout transition!"},
    };

    inline static uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    // interleaving vertex attributes
    inline static std::vector<TrVulkanVertex2DBase> Vertices =
    {
        {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
        {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
    };

    
    inline static std::vector<TrVulkanVertex2DTex> TexVertices =
    {
        {{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}}, {1.0f, 0.0f}},
        {{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}}, {0.0f, 0.0f}},
        {{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}, {0.0f, 1.0f}},
        {{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}, {1.0f, 1.0f}},
    };

    inline static std::vector<TrVulkanVertex3DTex> TexVertices3D =
    {
        {{{-0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}}, {0.0f, 0.0f}},
        {{{0.5f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}}, {1.0f, 0.0f}},
        {{{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}, {0.0f, 1.0f}},
        {{{0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}}, {1.0f, 1.0f}},

        {{{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}}, {0.0f, 0.0f}},
        {{{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}}, {1.0f, 0.0f}},
        {{{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}}, {0.0f, 1.0f}},
        {{{0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}}, {1.0f, 1.0f}},
    };

    inline static std::vector<uint16_t> Indices =
    {
        0, 1, 2,
        2, 3, 0
    };

    inline static std::vector<uint16_t> TexIndices3D =
    {
        0, 1, 2,
        1, 3, 2,
        
        4, 5, 6,
        5, 7, 6
    };

    

    enum class SHADER_FILE_ENUM
    {
        base,
        vertexbuffer,
        ubo,
        textures,
        depth,

        imGuiBase,
    };

    inline static std::unordered_map<SHADER_FILE_ENUM, const char*> SHADER_FILE_STRING =
    {
        {SHADER_FILE_ENUM::base, SHADER_DIR "VkRaster/shader"},
        {SHADER_FILE_ENUM::vertexbuffer, SHADER_DIR "VkRaster/shader_vertexbuffer"},
        {SHADER_FILE_ENUM::ubo, SHADER_DIR "VkRaster/shader_ubo"},
        {SHADER_FILE_ENUM::textures, SHADER_DIR "VkRaster/shader_textures"},
        {SHADER_FILE_ENUM::depth, SHADER_DIR "VkRaster/shader_depth"},
        
        {SHADER_FILE_ENUM::imGuiBase, SHADER_DIR "VkRaster/shader_imgui"},
    };

    inline static const char* vertSuffix = "_vert.spv";
    inline static const char* fragSuffix = "_frag.spv";

    inline static std::vector<std::string> texSuffix =
    {
        ".jpg",
        ".bmp",
        ".png",
        ".tga",
        
    };

    inline static bool bShowTrVulkanConfigWindow = true;
    inline static bool bShowDemoWindow = true;
    inline static bool bShowAnotherWindow = false;
    inline static glm::vec4 clearColor = {0.45f, 0.55f, 0.60f, 1.00f};

    inline static bool bTestBool = false;

    // MVP
    inline static float rModelScaleRate = 0.005f; // r means drag float rate
    inline static float modelScaleRate = 1.0f;

    inline static float rModelAngleAxis = 0.005f;
    inline static glm::vec3 modelAngleAxis = {0.0f, 0.0f, 1.0f};
    inline static float rModelRadiansAngle = 0.05f;
    inline static float modelRadiansAngle = 0.0f;

    inline static float rViewEye = 1.0f;
    inline static glm::vec3 viewEye = {0.0f, 1000.0f, 2000.0f};
    inline static float rViewCenter = 0.005f;
    inline static glm::vec3 viewCenter = {0.0f, 0.0f, 0.0f};
    inline static float rViewUp = 0.005f;
    inline static glm::vec3 viewUp = {0.0f, 0.0f, 1.0f};

    inline static float rProjRadiansFovy = 0.1f;
    inline static float projRadiansFovy = 45.0f;
    inline static float rProjZNear = 1.0f;
    inline static float projZNear = 1.0f;
    inline static float rProjZFar = 1.0f;
    inline static float projZFar = 5000.0f;
    
};

extern TrVulkanGlobal GTrVulkanGlobal;


class TrVulkanGlobalRT
{
public:
    
    // device extensions needed
    inline static const std::vector<const char*> deviceExtensions =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    inline static const std::vector<const char*> instanceExtensions =
    {
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    };

    inline static const std::vector<const char*> layers =
    {
        // "VK_LAYER_KHRONOS_validation", // already in create context
        "VK_LAYER_LUNARG_monitor", 
    };

    inline static float rCamEye = 1.0f;
    inline static float rCamCenter = 0.005f;
    inline static float rCamUp = 0.005f;
    inline static nvmath::vec3f camEye = {4.0f, 4.0f, 4.0f};
    inline static nvmath::vec3f camCenter = {0, 1, 0};
    inline static nvmath::vec3f camUp = {0, 1, 0};

    inline static std::vector<std::string> defaultSearchPaths =
    {
        SLN_ROOT_DIR,
        PROJECT_RELDIRECTORY,
    };
    
};

extern TrVulkanGlobalRT GTrVulkanGlobalRT;


