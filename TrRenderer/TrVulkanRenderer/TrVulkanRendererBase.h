#pragma once
#include "TrVulkanUtil.h"


class TrVulkanRendererBase
{
public:

    TrVulkanRendererBase();
    TrVulkanRendererBase(uint32_t width, uint32_t height, const char* title);

    void Run()
    {
        OnInitWindow();
        OnInitVulkan();
        OnRender();
        OnCleanup();
    }

private:
    GLFWwindow* window;

    void OnInitWindow()
    {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(mWidth, mHeight, mTitle, nullptr, nullptr);
    }

    void OnInitVulkan()
    {
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::cout << extensionCount << " extension supported" << std::endl;
    }

    void OnRender()
    {
        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
        }
    }

    void OnCleanup()
    {
        glfwDestroyWindow(window);

        glfwTerminate();
    }

private:
    uint32_t mWidth = 1920;
    uint32_t mHeight = 1080;
    const char* mTitle = "Vulkan";
};
