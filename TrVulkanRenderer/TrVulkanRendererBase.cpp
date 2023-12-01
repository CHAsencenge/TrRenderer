#include "TrVulkanRendererBase.h"

TrVulkanRendererBase::TrVulkanRendererBase()
{
}

TrVulkanRendererBase::TrVulkanRendererBase(uint32_t width, uint32_t height, const char* title) : 
mWidth(width),
mHeight(height),
mTitle(title)
{
    mScene = std::make_shared<TrScene>();
}

void TrVulkanRendererBase::Run()
{
}

void TrVulkanRendererBase::OnInitWindow()
{
    glfwInit();

    // prevent it from automatically creating OpenGL context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    mWindow = glfwCreateWindow(mWidth, mHeight, mTitle, nullptr, nullptr);
}

std::vector<const char*> TrVulkanRendererBase::GetRequiredExtensions()
{
    // vulkan instance extensions required by GLFW
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    return extensions;
}

void TrVulkanRendererBase::LoadModel(const std::string& filename, uint32_t actorId, glm::mat4 transform)
{
}

void TrVulkanRendererBase::LoadScene()
{
    for(const std::pair<uint32_t, TrActor*>& actor : mScene->mSceneActors)
    {
        LoadModel(nvh::findFile(actor.second->mModelReferencePath, TrVulkanGlobalRT::defaultSearchPaths, true), actor.first);
    }
}

