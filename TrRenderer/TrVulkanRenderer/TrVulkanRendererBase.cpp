#include "TrVulkanRendererBase.h"

#include <csignal>
#include <map>
#include <set>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "TrVulkanImGuiConfig.h"

TrVulkanRendererBase::TrVulkanRendererBase()
{
}

TrVulkanRendererBase::TrVulkanRendererBase(uint32_t width, uint32_t height, const char* title) :
	mWidth(width),
	mHeight(height),
	mTitle(title)
{
}

void TrVulkanRendererBase::Run()
{
	OnInitWindow();
	OnInitVulkan();
	OnRender();
	OnCleanup();
}

void TrVulkanRendererBase::OnInitWindow()
{
	glfwInit();

	// prevent it from automatically creating OpenGL context
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	mWindow = glfwCreateWindow(mWidth, mHeight, mTitle, nullptr, nullptr);
}

void TrVulkanRendererBase::OnInitVulkan()
{
	CheckExtensionSupport();
	CreateInstance();
	SetupDebugMessenger();
	CreateSurface();
	PickHighestWeightScorePhysicalDevice(VK_QUEUE_GRAPHICS_BIT); // or can PickFirstValidPhysicalDevice()
	CreateLogicalDevice();
	CreateSwapChain();
	CreateSwapChainImageViews();
	// CreateTestRenderPassImGuiOnly();
	CreateRenderPass();
	// before pipeline
	CreateDescriptorSetLayout();
	CreateGraphicsPipeline();
	CreateCommandPool();
	// before create frame buffers
	CreateDepthResources();
	CreateFrameBuffers();
	// LoadModels(std::vector<std::string>(1, "chalet"));
	LoadModels(std::vector<std::string>(1, "Tree"));
	// after create command pool
	CreateTextureImage();
	CreateTextureImageView();
	CreateTextureSampler();
	OnSetupImGui();
	
	// before create command buffers
	CreateVertexBuffer();
	CreateIndexBuffer();
	CreateUniformBuffers();
	
	CreateDescriptorPool();
	CreateDescriptorSets();

	InitImGuiBufferNum();
	CreateImGuiDescriptorSetLayout();
	CreateImGuiGraphicsPipeline();
	CreateImGuiDescriptorPool();
	CreateImGuiDescriptorSets();

	
	ImGuiSetFontTexId();
	
	CreateCommandBuffers();
	CreateSyncObjects();

	
}

void TrVulkanRendererBase::OnSetupImGui()
{
	// imgui
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();

	ImGui_ImplGlfw_InitForVulkan(mWindow, true);
	
	// Upload Fonts
	{
		VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
		{
			ImGuiCreateFontsTexture(commandBuffer);
		}
			
		EndSingleTimeCommands(commandBuffer);
	}
	
}

void TrVulkanRendererBase::OnRenderImGui()
{
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	// Start the Dear ImGui frame
	// ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	TrVulkanImGuiConfig::ShowTrVulkanConfig(&TrVulkanGlobal::bShowTrVulkanConfigWindow);
	
	/*if (TrVulkanGlobal::bShowDemoWindow)
		ImGui::ShowDemoWindow(&TrVulkanGlobal::bShowDemoWindow);*/

	// Rendering
	ImGui::Render();
}

void TrVulkanRendererBase::OnRender()
{
	// if the window close button is pressed
	while (!glfwWindowShouldClose(mWindow))
	{
		glfwPollEvents();
		
		DrawFrame();
	}

	// because draw frame is an async operation
	vkDeviceWaitIdle(mDevice);
}

// often vkCreateXXX needs vkDestroyXXX
void TrVulkanRendererBase::OnCleanup()
{
	if(mbEnableValidationLayers)
	{
		DestroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger, nullptr);
	}

	CleanupSwapChain();
	
	for(uint32_t i = 0; i < TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT; i++)
	{
		vkDestroySemaphore(mDevice, mImageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(mDevice, mRenderFinishedSemaphores[i], nullptr);
		vkDestroyFence(mDevice, mInFlightFences[i], nullptr);
	}

	vkDestroyDescriptorSetLayout(mDevice, mDescriptorSetLayout, nullptr);
	
	vkDestroyPipeline(mDevice, mGraphicsPipeline, nullptr);

	vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);

	vkDestroyDescriptorSetLayout(mDevice, mImGuiDescriptorSetLayout, nullptr);
	
	vkDestroyPipeline(mDevice, mImGuiGraphicsPipeline, nullptr);

	vkDestroyPipelineLayout(mDevice, mImGuiPipelineLayout, nullptr);

	vkDestroyRenderPass(mDevice, mRenderPass, nullptr);

	vkDestroySampler(mDevice, mTextureSampler, nullptr);
	vkDestroySampler(mDevice, mFontSampler, nullptr);

	vkDestroyImageView(mDevice, mTextureImageView, nullptr);
	vkDestroyImage(mDevice, mTextureImage, nullptr);
	vkFreeMemory(mDevice, mTextureImageMemory, nullptr);

	vkDestroyBuffer(mDevice, mVertexBuffer, nullptr);
	vkFreeMemory(mDevice, mVertexBufferMemory, nullptr);

	vkDestroyBuffer(mDevice, mIndexBuffer, nullptr);
	vkFreeMemory(mDevice, mIndexBufferMemory, nullptr);

	for(size_t i = 0; i < mUniformBuffers.size(); i++)
	{
		vkDestroyBuffer(mDevice, mUniformBuffers[i], nullptr);
		vkFreeMemory(mDevice, mUniformBuffersMemory[i], nullptr);
	}

	vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);
	vkDestroyDescriptorPool(mDevice, mImGuiDescriptorPool, nullptr);

	// should before destroy device, because of the firs param
	vkDestroyCommandPool(mDevice, mCommandPool, nullptr);
	
	// manually destroy logical device
	vkDestroyDevice(mDevice, nullptr);

	// manually destroy window surface
	vkDestroySurfaceKHR(mInstance, mSurface, nullptr);

	

	// manually destroy vulkan instance
	vkDestroyInstance(mInstance, nullptr);
	
	glfwDestroyWindow(mWindow);
	glfwTerminate();
}

void TrVulkanRendererBase::CreateInstance()
{
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "TrVulkanRendererBase";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 3, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 3, 0);
	appInfo.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	// extensions needed (for glfw, debugging......)
	std::vector<const char*> allExtensions;
	allExtensions = GetRequiredExtensions();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(allExtensions.size());
	createInfo.ppEnabledExtensionNames = allExtensions.data();

	// validation layers
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
	if(mbEnableValidationLayers)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(TrVulkanGlobal::validationLayers.size());
		createInfo.ppEnabledLayerNames = TrVulkanGlobal::validationLayers.data();
		PopulateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
	}
	else
	{
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = nullptr;
	}

	// return VkResult
	if(vkCreateInstance(&createInfo, nullptr, &mInstance) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::CREATE_INSTANCE_FAILED]);
	}
	
	
}

void TrVulkanRendererBase::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& messengerCreateInfo)
{
	messengerCreateInfo = {};
	messengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	// VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
	messengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	messengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	messengerCreateInfo.pfnUserCallback = DebugCallBack;
	messengerCreateInfo.pUserData = nullptr; // callback function parameters
}

void TrVulkanRendererBase::SetupDebugMessenger()
{
	if(!mbEnableValidationLayers)
	{
		return;
	}
	VkDebugUtilsMessengerCreateInfoEXT messengerCreateInfo;
	PopulateDebugMessengerCreateInfo(messengerCreateInfo);

	// vkCreateDebugUtilsMessengerEXT is an extension function, which will not be loaded by Vulkan lib automatically 
	if(CreateDebugUtilsMessengerEXT(mInstance, &messengerCreateInfo, nullptr, &mDebugMessenger) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::SETUP_DEBUG_MESSENGER_FAILED]);
	}
}

VkResult TrVulkanRendererBase::CreateDebugUtilsMessengerEXT(VkInstance instance,
	const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator,
	VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	// PFN means "pointer to function"
	// cast function pointer returned by vkGetInstanceProcAddr to PFN_vkCreateDebugUtilsMessengerEXT type
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if(func != nullptr)
	{
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void TrVulkanRendererBase::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
	const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if(func != nullptr)
	{
		func(instance, debugMessenger, pAllocator);
	}
}

void TrVulkanRendererBase::PickFirstValidPhysicalDevice(VkQueueFlagBits queueFlag)
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(mInstance, &deviceCount, nullptr);
	if(deviceCount == 0)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::NO_VALID_DEVICE]);
	}
	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data());
	for(const VkPhysicalDevice& device : devices)
	{
		if(IsDeviceSuitable(device, queueFlag))
		{
			mPhysicalDevice = device;
			break;
		}
	}
	if(mPhysicalDevice == VK_NULL_HANDLE)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::NO_SUITABLE_DEVICE]);
	}
}

void TrVulkanRendererBase::PickHighestWeightScorePhysicalDevice(VkQueueFlagBits queueFlag)
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(mInstance, &deviceCount, nullptr);
	if(deviceCount == 0)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::NO_VALID_DEVICE]);
	}
	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data());

	std::multimap<int, VkPhysicalDevice> candidates;
	
	for(const VkPhysicalDevice& device : devices)
	{
		uint32_t score = CalcDeviceSuitabilityScore(device, queueFlag);
		candidates.insert(std::make_pair(score, device));
	}
	// rbegin(): reverted iterator
	// multimap default sort rule: operator <
	if(candidates.rbegin()->first > 0)
	{
		mPhysicalDevice = candidates.rbegin()->second;
	}
	else
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::NO_SUITABLE_DEVICE]);
	}
}

bool TrVulkanRendererBase::IsDeviceSuitable(VkPhysicalDevice device, VkQueueFlagBits queueFlag)
{
	// basic device properties: name, type, supported vulkan version
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);

	// support for texture compression, 64 bits float for shader, multiple viewports...
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
	
	TrVulkanQueueFamilyIndices indices = FindQueueFamilies(device, queueFlag);

	bool bDeviceExtensionSupported = CheckDeviceExtensionSupport(device);

	bool bSwapChainAdequate = false;
	if(bDeviceExtensionSupported) // contains swap chain extension
	{
		// surface support detail
		TrVulkanSwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
		bSwapChainAdequate = !swapChainSupport.mFormats.empty() && !swapChainSupport.mPresentModes.empty();
	}
	
	return deviceFeatures.geometryShader && indices.IsComplete() && bDeviceExtensionSupported && bSwapChainAdequate && deviceFeatures.samplerAnisotropy;
}

uint32_t TrVulkanRendererBase::CalcDeviceSuitabilityScore(VkPhysicalDevice device, VkQueueFlagBits queueFlag)
{

	if(!IsDeviceSuitable(device, queueFlag))
	{
		return 0;
	}
	
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);

	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
	
	uint32_t score = 0;

	if(deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
	{
		score += 1000;
	}

	score += deviceProperties.limits.maxImageDimension2D;
	
	return score;
}

void TrVulkanRendererBase::CreateLogicalDevice()
{
	TrVulkanQueueFamilyIndices indices = FindQueueFamilies(mPhysicalDevice, VK_QUEUE_GRAPHICS_BIT);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies =
	{
		indices.mGraphicsFamily.value(),
		indices.mPresentFamily.value(),
	};

	float queuePriority = 1.0f;

	for (uint32_t queueFamily : uniqueQueueFamilies)
	{
		// populate VkDeviceQueueCreateInfo, which describes queues needed
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		// queue family properties are get from vkGetPhysicalDeviceQueueFamilyProperties, indices.mGraphicsFamily is the location in std::vector<VkQueueFamilyProperties> 
		queueCreateInfo.queueFamilyIndex = queueFamily;
		// for each queue family, rarely needs more than one queue (can create command buffers in threads, commit once in main thread)
		queueCreateInfo.queueCount = 1;
		// priority to execute command buffer
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}
	
	// specify features to use
	VkPhysicalDeviceFeatures deviceFeatures = {};
	deviceFeatures.samplerAnisotropy = VK_TRUE; // anisotropy feature
	
	// create logical device, like create instance
	// device level create info, compared to instance level create info
	VkDeviceCreateInfo logicalDeviceCreateInfo = {};
	logicalDeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	// create queue along with the logical device, destroyed automatically when destroy the logical device
	logicalDeviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	logicalDeviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	logicalDeviceCreateInfo.pEnabledFeatures = &deviceFeatures;
	// if not enable swapchain extension, mSwapChain will be null after creation, but return VK_SUCCESS! 
	logicalDeviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(TrVulkanGlobal::deviceExtensions.size());
	logicalDeviceCreateInfo.ppEnabledExtensionNames = TrVulkanGlobal::deviceExtensions.data();
	if(mbEnableValidationLayers)
	{
		logicalDeviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(TrVulkanGlobal::validationLayers.size());
		logicalDeviceCreateInfo.ppEnabledLayerNames = TrVulkanGlobal::validationLayers.data();
	}
	else
	{
		logicalDeviceCreateInfo.enabledLayerCount = 0;
	}
	// associate with the physical device
	if(vkCreateDevice(mPhysicalDevice, &logicalDeviceCreateInfo, nullptr, &mDevice) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::CREATE_LOGICAL_DEVICE_FAILED]);
	}

	// after this, we can use graphics card
	vkGetDeviceQueue(mDevice, indices.mGraphicsFamily.value(), 0, &mGraphicsQueue);
	vkGetDeviceQueue(mDevice, indices.mPresentFamily.value(), 0, &mPresentQueue);
	
}

TrVulkanQueueFamilyIndices TrVulkanRendererBase::FindQueueFamilies(VkPhysicalDevice device, VkQueueFlagBits queueFlag)
{
	TrVulkanQueueFamilyIndices indices;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
	
	int i = 0;
	for(const VkQueueFamilyProperties& queueFamily : queueFamilies)
	{
		// find a queue family which support VK_QUEUE_GRAPHICS_BIT
		if(queueFamily.queueCount > 0 && queueFamily.queueFlags & queueFlag)
		{
			indices.mGraphicsFamily = i;
		}

		// find a queue family which support present
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, mSurface, &presentSupport);
		if(presentSupport)
		{
			indices.mPresentFamily = i;
		}

		if(indices.IsComplete())
		{
			break;
		}
		
		i++;
	}

	// why "indices" rather than "index": multiple supports, multiple families
	return indices;
}

void TrVulkanRendererBase::CreateSurface()
{
	if(glfwCreateWindowSurface(mInstance, mWindow, nullptr, &mSurface) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::CREATE_WINDOW_SURFACE_FAILED]);
	}
}


TrVulkanSwapChainSupportDetails TrVulkanRendererBase::QuerySwapChainSupport(VkPhysicalDevice device)
{
	TrVulkanSwapChainSupportDetails details;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, mSurface, &details.mCapabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, mSurface, &formatCount, nullptr);
	if(formatCount != 0)
	{
		details.mFormats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, mSurface, &formatCount, details.mFormats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, mSurface, &presentModeCount, nullptr);
	if(presentModeCount != 0)
	{
		details.mPresentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, mSurface, &presentModeCount, details.mPresentModes.data());
	}

	return details;
}

// TODO: configuration parameterization
VkSurfaceFormatKHR TrVulkanRendererBase::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
	for(const VkSurfaceFormatKHR& availableFormat : availableFormats)
	{
		if(availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
		{
			return availableFormat;
		}
	}
	return availableFormats[0];
}

// vertical sync
// vertical retrace
// tearing
VkPresentModeKHR TrVulkanRendererBase::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
	for(const VkPresentModeKHR& availablePresentMode : availablePresentModes)
	{
		if(availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return availablePresentMode;
		}
	}
	return VK_PRESENT_MODE_FIFO_KHR;
}

// resolution
VkExtent2D TrVulkanRendererBase::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
	// currentExtent tells appropriate window extent
	// if currentExtent use max value, we should decide extent by ourself
	if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return capabilities.currentExtent;
	}
	else
	{
		VkExtent2D actualExtent = {mWidth, mHeight};
		actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
		actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));
		return actualExtent;
	}
}

void TrVulkanRendererBase::CreateSwapChain()
{
	TrVulkanSwapChainSupportDetails swapChainSupportDetails = QuerySwapChainSupport(mPhysicalDevice);
	VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupportDetails.mFormats);
	VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupportDetails.mPresentModes);
	VkExtent2D extent2D = ChooseSwapExtent(swapChainSupportDetails.mCapabilities);

	// what is "Triple Buffering", why minImageCount + 1
	uint32_t imageCount = swapChainSupportDetails.mCapabilities.minImageCount;
	if(swapChainSupportDetails.mCapabilities.maxImageCount > 0 && swapChainSupportDetails.mCapabilities.maxImageCount < imageCount)
	{
		imageCount = swapChainSupportDetails.mCapabilities.maxImageCount;
	}
	VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.surface = mSurface;
	// min count, vulkan may create more images
	swapChainCreateInfo.minImageCount = imageCount;
	swapChainCreateInfo.imageFormat = surfaceFormat.format;
	swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapChainCreateInfo.presentMode = presentMode;
	swapChainCreateInfo.imageExtent = extent2D;
	// layer count in each image, VR uses more than one 
	swapChainCreateInfo.imageArrayLayers = 1;
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	// graphics queue responsible for drawing
	// present queue responsible for displaying
	// access: concurrent or exclusive
	TrVulkanQueueFamilyIndices indices = FindQueueFamilies(mPhysicalDevice, VK_QUEUE_GRAPHICS_BIT);
	// for compatibility with parameter types: pQueueFamilyIndices
	uint32_t queueFamilyIndices[] = {indices.mGraphicsFamily.value(), indices.mPresentFamily.value()};

	if(indices.mGraphicsFamily != indices.mPresentFamily)
	{
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		// specify which families share image access ownership
		swapChainCreateInfo.queueFamilyIndexCount = 2;
		swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	// image rotate, flip
	swapChainCreateInfo.preTransform = swapChainSupportDetails.mCapabilities.currentTransform;
	// whether to use alpha channel for blending with other windows in window system
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	// don't care pixel color occupied by other window
	swapChainCreateInfo.clipped = VK_TRUE;
	// swap chain need to be rebuild after adjusting extent
	swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	if(vkCreateSwapchainKHR(mDevice, &swapChainCreateInfo, nullptr, &mSwapChain) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::CREATE_SWAPCHAIN_FAILED]);
	}

	// minImageCount in create info, vulkan may creates more images than minImageCount
	vkGetSwapchainImagesKHR(mDevice, mSwapChain, &imageCount, nullptr);
	mSwapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(mDevice, mSwapChain, &imageCount, mSwapChainImages.data());

	mSwapChainImageFormat = surfaceFormat.format;
	mSwapChainExtent = extent2D;
}

VkImageView TrVulkanRendererBase::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
	VkImageViewCreateInfo imageViewCreateInfo = {};
	imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.image = image;
	imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.format = format;
	// VkComponentSwizzle: remapping image's component
	// color channel mapping, for example, single color textures, mapping all channels to red
	imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY; // use component's origin component value, do not remapping
	imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	// image usage
	imageViewCreateInfo.subresourceRange.aspectMask = aspectFlags; // used as RT
	imageViewCreateInfo.subresourceRange.layerCount = 1; // if VR, multiple layers (left eye layer, right eye layer), multiple views for one image
	imageViewCreateInfo.subresourceRange.levelCount = 1; // no LOD
	imageViewCreateInfo.subresourceRange.baseMipLevel = 0;

	VkImageView imageView;
	
	if(vkCreateImageView(mDevice, &imageViewCreateInfo, nullptr, &imageView) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::CREATE_IMAGE_VIEW_FAILED]);
	}
	return imageView;
}

// used when creating frame buffer
void TrVulkanRendererBase::CreateSwapChainImageViews()
{
	mSwapChainImageViews.resize(mSwapChainImages.size());
	for(size_t i = 0; i < mSwapChainImages.size(); i++)
	{
		mSwapChainImageViews[i] = CreateImageView(mSwapChainImages[i], mSwapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
	}
}

void TrVulkanRendererBase::CleanupSwapChain()
{
	vkDestroyImage(mDevice, mDepthImage, nullptr);
	vkDestroyImageView(mDevice, mDepthImageView, nullptr);
	vkFreeMemory(mDevice, mDepthImageMemory, nullptr);
	
	// before destroying render pass and image view 
	for(VkFramebuffer& framebuffer : mSwapChainFrameBuffers)
	{
		vkDestroyFramebuffer(mDevice, framebuffer, nullptr);
	}
	
	for(VkImageView& imageView : mSwapChainImageViews)
	{
		vkDestroyImageView(mDevice, imageView, nullptr);
	}

	// manually destroy swap chain, earlier than logical device
	vkDestroySwapchainKHR(mDevice, mSwapChain, nullptr);
}

// a framework, no detail data
void TrVulkanRendererBase::CreateRenderPass()
{
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = mSwapChainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // process approach to attachment before render
	// the contents generated during the render pass and within the render area are written to memory
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // process approach to attachment after render
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // process approach to stencil of attachment before render
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // process approach to stencil of attachment after render
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // attachment layout before render
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // attachment layout after render, image is used to present in swapchain

	// a render pass can contain multiple sub passes
	// a sub pass references to the frame buffer content processed by last phase
	// each sub pass can reference one or more attachments
	VkAttachmentReference colorAttachmentReference = {};
	colorAttachmentReference.attachment = 0;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // best image layout for using as color attachment


	VkAttachmentDescription depthAttachment = {};
	depthAttachment.format = FindDepthFormat();
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // process approach to attachment before render
	// VK_ATTACHMENT_STORE_OP_DONT_CARE: the contents of an attachment are not needed after a render pass completes
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // process approach to attachment after render
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // process approach to stencil of attachment before render
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // process approach to stencil of attachment after render
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // attachment layout before render
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // attachment layout after render, image is used to present in swapchain

	VkAttachmentReference depthAttachmentReference = {};
	depthAttachmentReference.attachment = 1;
	depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
	
	VkSubpassDescription subPassDescription0 = {};
	// pipeline type that want to bind
	subPassDescription0.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // means this is a graphics sub pass, rather than a compute sub pass
	subPassDescription0.colorAttachmentCount = 1;
	subPassDescription0.pColorAttachments = &colorAttachmentReference;
	subPassDescription0.pDepthStencilAttachment = &depthAttachmentReference;

	VkSubpassDescription subPassDescription1 = {};
	// pipeline type that want to bind
	subPassDescription1.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // means this is a graphics sub pass, rather than a compute sub pass
	subPassDescription1.colorAttachmentCount = 1;
	subPassDescription1.pColorAttachments = &colorAttachmentReference;
	
	subPassDescription1.pDepthStencilAttachment = &depthAttachmentReference; // do not use this for UI
	
	std::array<VkSubpassDescription, 2> subPassDescriptions = {subPassDescription0, subPassDescription1};
	// sync 
	// define dependencies between sub passes
	VkSubpassDependency subpassDependency0 = {};
	subpassDependency0.srcSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependency0.dstSubpass = 0;
	subpassDependency0.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	subpassDependency0.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	subpassDependency0.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	subpassDependency0.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	VkSubpassDependency subpassDependency1 = {};
	subpassDependency1.srcSubpass = 0;
	subpassDependency1.dstSubpass = 1;
	subpassDependency1.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	subpassDependency1.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	subpassDependency1.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	subpassDependency1.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	std::array<VkSubpassDependency, 2> subpassDependencies = {subpassDependency0, subpassDependency1};


	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = 2;
	renderPassCreateInfo.pAttachments = attachments.data();
	renderPassCreateInfo.subpassCount = 2;
	renderPassCreateInfo.pSubpasses = subPassDescriptions.data();

	renderPassCreateInfo.dependencyCount = 2;
	renderPassCreateInfo.pDependencies = subpassDependencies.data();

	

	if(vkCreateRenderPass(mDevice, &renderPassCreateInfo, nullptr, &mRenderPass) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::CREATE_RENDER_PASS_FAILED]);
	}
}

void TrVulkanRendererBase::CreateGraphicsPipeline()
{
	std::string shaderFilePrefix = std::string(TrVulkanGlobal::SHADER_FILE_STRING[TrVulkanGlobal::SHADER_FILE_ENUM::depth]);
	auto vertShaderCompiledCode = TrVulkanUtil::ReadFile(shaderFilePrefix + TrVulkanGlobal::vertSuffix);
	auto fragShaderCompiledCode = TrVulkanUtil::ReadFile(shaderFilePrefix + TrVulkanGlobal::fragSuffix);

	// is only a wrap of shader byte code
	// only used in this field, so should destroy before leaving this field
	VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCompiledCode);
	VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCompiledCode);

	VkPipelineShaderStageCreateInfo vertShaderStageCreateInfo = {};
	vertShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageCreateInfo.module = vertShaderModule;
	vertShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	// may define multiple shaders in a shader file, use which
	vertShaderStageCreateInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageCreateInfo = {};
	fragShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageCreateInfo.module = fragShaderModule;
	fragShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	// may define multiple shaders in a shader file, use which
	fragShaderStageCreateInfo.pName = "main";

	// array of shader stage create info
	VkPipelineShaderStageCreateInfo shaderStageCreateInfos[] =
	{
		vertShaderStageCreateInfo,
		fragShaderStageCreateInfo,
	};

	// vertex input (to vertex shader)
	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	// auto bindingDescription = TrVulkanVertex2DBase::GetBindingDescription();
	// auto attributeDescriptions = TrVulkanVertex2DBase::GetAttributeDescriptions();

	// auto bindingDescription = TrVulkanVertex2DTex::GetBindingDescription();
	// auto attributeDescriptions = TrVulkanVertex2DTex::GetAttributeDescriptions();

	auto bindingDescription = TrVulkanVertex3DTex::GetBindingDescription();
	auto attributeDescriptions = TrVulkanVertex3DTex::GetAttributeDescriptions();


	// binding: offset between vertices, per-vertex or per-instance
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = &bindingDescription;
	// attribute: attribute for shader variable
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = attributeDescriptions.size();
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	// input assembly: what kind of primitive topology does vertex data define, whether to use primitive restart
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {};
	inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	// list or strip, point line or triangle
	inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	// if enabled, use a special index to restart, index after this index reset to the first vert of primitive
	inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

	// viewport and scissor
	// viewport: image map to frame buffer
	// scissor: pixels in which region is actually saved in frame buffer
	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = (float) mSwapChainExtent.height;
	viewport.width = (float) mSwapChainExtent.width; // mSwapChainExtent may different from window
	viewport.height = -((float) mSwapChainExtent.height);
	
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	VkRect2D scissor = {};
	scissor.offset = {0, 0};
	scissor.extent = mSwapChainExtent;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pViewports = &viewport;
	viewportStateCreateInfo.pScissors = &scissor;

	// rasterization
	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	// should fragments outside near plane and far plane be clamped to planes, rather than discarded
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	// no fragments output to frame buffer
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateCreateInfo.lineWidth = 1.0f;
	// cull front, back, front and back
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
	
	// front face condition: clockwise or counter clockwise
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	// rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	// use for shadow
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	/*rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
	rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;*/

	// multisample
	VkPipelineMultisampleStateCreateInfo multiSampleCreateInfo = {};
	multiSampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multiSampleCreateInfo.sampleShadingEnable = VK_FALSE;
	multiSampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	/*multiSampleCreateInfo.minSampleShading = 1.0f;
	multiSampleCreateInfo.alphaToCoverageEnable = VK_FALSE;
	multiSampleCreateInfo.alphaToOneEnable = VK_FALSE;*/
	
	// color blend: fragment color returned by fragment shader should blend with color in frame buffer corresponding pixel
	// for each frame buffer 
	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
	colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachmentState.blendEnable = VK_FALSE;
	// for global color blend
	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
	colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	// bit operation blend
	colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendStateCreateInfo.attachmentCount = 1;
	colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;
	colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendStateCreateInfo.blendConstants[0] = 0.0f;
	colorBlendStateCreateInfo.blendConstants[1] = 0.0f;
	colorBlendStateCreateInfo.blendConstants[2] = 0.0f;
	colorBlendStateCreateInfo.blendConstants[3] = 0.0f;

	// few pipeline states can change dynamically without rebuild pipeline
	std::vector<VkDynamicState> dynamicStates =
	{
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};
	// need to re-specify values when draw
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

	// can make dynamic config to shader (uniform variables can be modified dynamically after creating pipeline)
	VkPipelineLayoutCreateInfo layoutCreateInfo = {};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCreateInfo.setLayoutCount = 1;
	// layoutCreateInfo.pushConstantRangeCount = 0;
	layoutCreateInfo.pSetLayouts = &mDescriptorSetLayout;
	// layoutCreateInfo.pPushConstantRanges = nullptr;
	if(vkCreatePipelineLayout(mDevice, &layoutCreateInfo, nullptr, &mPipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::CREATE_LAYOUT_FAILED]);
	}

	VkPipelineDepthStencilStateCreateInfo depthStencil = {};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS; // new frag can write into attachment when its depth less than depth in depth buffer
	depthStencil.minDepthBounds = 0.0f;
	depthStencil.maxDepthBounds = 1.0f;
	// stencil
	depthStencil.stencilTestEnable = VK_FALSE;
	depthStencil.front = {};
	depthStencil.back = {};
	
	// create graphics pipeline
	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = 2; // vert & frag, each has a code file
	pipelineCreateInfo.pStages = shaderStageCreateInfos;
	// fixed stages
	pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multiSampleCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo; // for reduce rebuild graphics pipeline
	pipelineCreateInfo.pDepthStencilState = &depthStencil;
	
	pipelineCreateInfo.layout = mPipelineLayout; // for uniform variables
	pipelineCreateInfo.renderPass = mRenderPass;
	pipelineCreateInfo.subpass = 0; // index
	// for creating a new graphics pipeline with a created pipeline
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	// pipelineCreateInfo.basePipelineIndex = -1;

	if(vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &mGraphicsPipeline) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::CREATE_GRAPHICS_PIPELINE_FAILED]);
	}

	vkDestroyShaderModule(mDevice, vertShaderModule, nullptr);
	vkDestroyShaderModule(mDevice, fragShaderModule, nullptr);
}

VkShaderModule TrVulkanRendererBase::CreateShaderModule(std::vector<char>& compiledCode)
{
	VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = compiledCode.size();
	shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(compiledCode.data());

	VkShaderModule shaderModule;
	if(vkCreateShaderModule(mDevice, &shaderModuleCreateInfo, nullptr, &shaderModule) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::CREATE_SHADER_MODULE_FAILED]);
	}
	return shaderModule;
}

// frame buffers are used to save image attachments in render pass
void TrVulkanRendererBase::CreateFrameBuffers()
{
	mSwapChainFrameBuffers.resize(mSwapChainImageViews.size()); // frame buffer number equals to image view number in swap chain 
	for(size_t i = 0; i < mSwapChainFrameBuffers.size(); i++)
	{
		// one frame buffer can have multiple ( image view ) attachments 
		std::array<VkImageView, 2> attachments =
		{
			mSwapChainImageViews[i],
			mDepthImageView,
		};
		/*std::array<VkImageView, 1> attachments =
		{
			mSwapChainImageViews[i],
		};*/
		VkFramebufferCreateInfo frameBufferCreateInfo = {};
		frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frameBufferCreateInfo.renderPass = mRenderPass;
		frameBufferCreateInfo.width = mSwapChainExtent.width;
		frameBufferCreateInfo.height = mSwapChainExtent.height;
		frameBufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size()); // one frame buffer can have multiple ( image view ) attachments 
		// a pointer to an array of VkImageView handles, each of which will be used as the corresponding attachment in a render pass instance
		frameBufferCreateInfo.pAttachments = attachments.data();
		frameBufferCreateInfo.layers = 1; // image layer count
		
		if(vkCreateFramebuffer(mDevice, &frameBufferCreateInfo, nullptr, &mSwapChainFrameBuffers[i]) != VK_SUCCESS)
		{
			throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::CREATE_FRAME_BUFFER_FAILED]);
		}
	}
}

// command pool: manage command buffer, define rules of allocating and releasing of command buffers
void TrVulkanRendererBase::CreateCommandPool()
{
	TrVulkanQueueFamilyIndices queueFamilyIndices = FindQueueFamilies(mPhysicalDevice, VK_QUEUE_GRAPHICS_BIT);
	VkCommandPoolCreateInfo commandPoolCreateInfo = {};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	// each command buffer object allocated by command pool can only be committed to a certain type of queue
	commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndices.mGraphicsFamily.value(); // for draw command
	// optimizing
	// VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: specifies that command buffers allocated from the pool will be short-lived, meaning that they will be reset or freed in a relatively short timeframe.
	// This flag may be used by the implementation to control memory allocation behavior within the pool.
	// VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT: allows any command buffer allocated from a pool to be individually reset to the initial state
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // Attempt to reset VkCommandBuffer created from VkCommandPool that does NOT have the VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT bit set

	if(vkCreateCommandPool(mDevice, &commandPoolCreateInfo, nullptr, &mCommandPool) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::CREATE_COMMAND_POOL_FAILED]);
	}
}

void TrVulkanRendererBase::CreateCommandBuffers()
{
	mCommandBuffers.resize(TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT);
	
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.commandPool = mCommandPool;
	// VK_COMMAND_BUFFER_LEVEL_PRIMARY: can be committed to queue and executed, can not be invoked by other command buffer objects
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT;

	if(vkAllocateCommandBuffers(mDevice, &commandBufferAllocateInfo, mCommandBuffers.data()) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::ALLOCATE_COMMAND_BUFFER_FAILED]);
	}
}

void TrVulkanRendererBase::RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	OnRenderImGui();
	
	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	/*commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	commandBufferBeginInfo.pInheritanceInfo = nullptr;*/

	// begin to record command buffer
	if(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::RECORD_COMMAND_BUFFER_BEGIN_FAILED]);
	}

	// begin render pass
	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = mRenderPass;
	renderPassBeginInfo.framebuffer = mSwapChainFrameBuffers[imageIndex];
	renderPassBeginInfo.renderArea.offset = {0 ,0};
	renderPassBeginInfo.renderArea.extent = mSwapChainExtent;

	std::array<VkClearValue, 2> clearValues = {};
	clearValues[0] = {0.0f, 0.0f, 0.0f, 1.0f};
	clearValues[1] = {1.0f, 1.0f, 1.0f, 1.0f};
	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassBeginInfo.pClearValues = clearValues.data();

	// VK_SUBPASS_CONTENTS_INLINE: all commands are in main command buffer
	vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	{
		// bind graphics pipeline to the command buffer
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mGraphicsPipeline);

		// Cmds
		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = (float) mSwapChainExtent.height;
		viewport.width = (float) mSwapChainExtent.width;
		viewport.height = -((float) mSwapChainExtent.height);
		
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkRect2D scissor = {};
		scissor.offset = {0, 0};
		scissor.extent = mSwapChainExtent;
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		// bind vertex buffers
		VkBuffer vertexBuffers[] = {mVertexBuffer};
		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
		
		vkCmdBindIndexBuffer(commandBuffer, mIndexBuffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescriptorSets[mCurrentFrame], 0, nullptr);

		// vkCmdDraw(commandBuffer, static_cast<uint32_t>(TrVulkanGlobal::Vertices.size()), 1, 0, 0);
		// vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(TrVulkanGlobal::Indices.size()), 1, 0, 0, 0);
		// vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(TrVulkanGlobal::TexIndices3D.size()), 1, 0, 0, 0);

		// the assembled primitives execute the bound graphics pipeline
		vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(mModels[0].mIndices.size()), 1, 0, 0, 0);

		
		// to transition to the next sub pass in the render pass instance after recording the commands for a sub pass
		vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
		
		ImDrawData* imGuiDrawData = ImGui::GetDrawData();
		const bool bMinimized = (imGuiDrawData->DisplaySize.x <= 0.0f || imGuiDrawData->DisplaySize.y <= 0.0f);
		if (!bMinimized)
		{
			/*FrameRender(wd, draw_data);
			FramePresent(wd);*/

			// expected frame buffer width and height of imgui draw data
			int fbWidth = static_cast<int>(imGuiDrawData->DisplaySize.x * imGuiDrawData->FramebufferScale.x);
			int fbHeight = static_cast<int>(imGuiDrawData->DisplaySize.y * imGuiDrawData->FramebufferScale.y);
			if (fbWidth <= 0 || fbHeight <= 0)
				return;
			
			// data to vertex and index buffer
			if(imGuiDrawData->TotalVtxCount > 0)
			{
				size_t imGuiVertexSize = imGuiDrawData->TotalVtxCount * sizeof(ImDrawVert);
				size_t imGuiIndexSize = imGuiDrawData->TotalIdxCount * sizeof(ImDrawIdx);

				VkDeviceSize alignedVertexBufferSize = 0;
				VkDeviceSize alignedIndexBufferSize = 0;
				CreateOrReCreateBuffer(imGuiVertexSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, alignedVertexBufferSize, mImGuiVertexBuffers[mCurrentFrame], mImGuiVertexBufferMemories[mCurrentFrame]);
				CreateOrReCreateBuffer(imGuiIndexSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, alignedIndexBufferSize, mImGuiIndexBuffers[mCurrentFrame], mImGuiIndexBufferMemories[mCurrentFrame]);

				ImDrawVert* imGuiVertexDst = nullptr;
				ImDrawIdx* imGuiIndexDst = nullptr;

				VkPhysicalDeviceProperties deviceProperties;
				vkGetPhysicalDeviceProperties(mPhysicalDevice, &deviceProperties);
				// deviceProperties.limits.nonCoherentAtomSize / 2 - imGuiVertexSize % deviceProperties.limits.nonCoherentAtomSize / 2
				vkMapMemory(mDevice, mImGuiVertexBufferMemories[mCurrentFrame], 0, alignedVertexBufferSize, 0, (void**)&imGuiVertexDst);
				vkMapMemory(mDevice, mImGuiIndexBufferMemories[mCurrentFrame], 0, alignedIndexBufferSize, 0, (void**)&imGuiIndexDst);
				

				for (int n = 0; n < imGuiDrawData->CmdListsCount; n++)
				{
					const ImDrawList* imGuiCmdList = imGuiDrawData->CmdLists[n];
					memcpy(imGuiVertexDst, imGuiCmdList->VtxBuffer.Data, imGuiCmdList->VtxBuffer.Size * sizeof(ImDrawVert));
					memcpy(imGuiIndexDst, imGuiCmdList->IdxBuffer.Data, imGuiCmdList->IdxBuffer.Size * sizeof(ImDrawIdx));
					imGuiVertexDst += imGuiCmdList->VtxBuffer.Size;
					imGuiIndexDst += imGuiCmdList->IdxBuffer.Size;
				}

				VkMappedMemoryRange range[2] = {};
				range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
				range[0].memory = mImGuiVertexBufferMemories[mCurrentFrame];
				range[0].size = VK_WHOLE_SIZE;
				range[1].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
				range[1].memory = mImGuiIndexBufferMemories[mCurrentFrame];
				range[1].size = VK_WHOLE_SIZE;
				vkFlushMappedMemoryRanges(mDevice, 2, range);
				vkUnmapMemory(mDevice, mImGuiVertexBufferMemories[mCurrentFrame]);
				vkUnmapMemory(mDevice, mImGuiIndexBufferMemories[mCurrentFrame]);
			}

			// setup render state
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mImGuiGraphicsPipeline);

			if(imGuiDrawData->TotalVtxCount > 0)
			{
				VkBuffer imGuivertexBuffers[] = {mImGuiVertexBuffers[mCurrentFrame]};
				VkDeviceSize imGuiVertexOffset[] = {0};
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, imGuivertexBuffers, imGuiVertexOffset);
				vkCmdBindIndexBuffer(commandBuffer, mImGuiIndexBuffers[mCurrentFrame], 0, sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
			}

			VkViewport imGuiViewport;
			imGuiViewport.x = 0;
			imGuiViewport.y = 0;
			imGuiViewport.width = (float)fbWidth;
			imGuiViewport.height = (float)fbHeight;
			imGuiViewport.minDepth = 0.0f;
			imGuiViewport.maxDepth = 1.0f;
			vkCmdSetViewport(commandBuffer, 0, 1, &imGuiViewport);

			// scale and translation
			float scale[2];
			scale[0] = 2.0f / imGuiDrawData->DisplaySize.x;
			scale[1] = 2.0f / imGuiDrawData->DisplaySize.y;
			float translate[2];
			translate[0] = -1.0f - imGuiDrawData->DisplayPos.x * scale[0];
			translate[1] = -1.0f - imGuiDrawData->DisplayPos.y * scale[1];
			vkCmdPushConstants(commandBuffer, mImGuiPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 0, sizeof(float) * 2, scale);
			vkCmdPushConstants(commandBuffer, mImGuiPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 2, sizeof(float) * 2, translate);
			
			// scissor
			ImVec2 clip_off = imGuiDrawData->DisplayPos;         // (0,0) unless using multi-viewports
			ImVec2 clip_scale = imGuiDrawData->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

			int global_vtx_offset = 0;
			int global_idx_offset = 0;

			for (int n = 0; n < imGuiDrawData->CmdListsCount; n++)
			{
				const ImDrawList* cmd_list = imGuiDrawData->CmdLists[n];
				// for (int cmd_i = cmd_list->CmdBuffer.Size -1; cmd_i >= 0; cmd_i--)
				for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
				{
					const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
					
					// Project scissor/clipping rectangles into framebuffer space
					ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
					ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);

					// Clamp to viewport as vkCmdSetScissor() won't accept values that are off bounds
					if (clip_min.x < 0.0f) { clip_min.x = 0.0f; }
					if (clip_min.y < 0.0f) { clip_min.y = 0.0f; }
					if (clip_max.x > fbWidth) { clip_max.x = (float)fbWidth; }
					if (clip_max.y > fbHeight) { clip_max.y = (float)fbHeight; }
					if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
						continue;

					// Apply scissor/clipping rectangle
					VkRect2D imGuiScissor;
					imGuiScissor.offset.x = (int32_t)(clip_min.x);
					imGuiScissor.offset.y = (int32_t)(clip_min.y);
					imGuiScissor.extent.width = (uint32_t)(clip_max.x - clip_min.x);
					imGuiScissor.extent.height = (uint32_t)(clip_max.y - clip_min.y);
					vkCmdSetScissor(commandBuffer, 0, 1, &imGuiScissor);

					
					// bind descriptor set with font texture
					VkDescriptorSet desc_set[1] = { (VkDescriptorSet)pcmd->TextureId };
					
					IM_ASSERT(pcmd->TextureId == (ImTextureID)mImGuiDescriptorSets[mCurrentFrame]);
					desc_set[0] = mImGuiDescriptorSets[mCurrentFrame];
					ImGuiSetFontTexId();
					if (sizeof(ImTextureID) < sizeof(ImU64))
					{
						// We don't support texture switches if ImTextureID hasn't been redefined to be 64-bit. Do a flaky check that other textures haven't been used.
						IM_ASSERT(pcmd->TextureId == (ImTextureID)mImGuiDescriptorSets[mCurrentFrame]);
						desc_set[0] = mImGuiDescriptorSets[mCurrentFrame];
					}
					// bind one or more descriptor sets to a command buffer
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mImGuiPipelineLayout, 0, 1, desc_set, 0, nullptr);

					// draw
					vkCmdDrawIndexed(commandBuffer, pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
				}
				global_idx_offset += cmd_list->IdxBuffer.Size;
				global_vtx_offset += cmd_list->VtxBuffer.Size;
			}
			VkRect2D scissor1 = { { 0, 0 }, { (uint32_t)fbWidth, (uint32_t)fbHeight } };
			vkCmdSetScissor(commandBuffer, 0, 1, &scissor1);
		}
	}
	
	vkCmdEndRenderPass(commandBuffer);
	
	if(vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::RECORD_COMMAND_BUFFER_END_FAILED]);
	}
}

void TrVulkanRendererBase::CreateOrReCreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
	VkMemoryPropertyFlags propertyFlags, VkDeviceSize& alignedBufferSize, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
	if(buffer != VK_NULL_HANDLE)
	{
		vkDestroyBuffer(mDevice, buffer, nullptr);
	}
	if(bufferMemory != VK_NULL_HANDLE)
	{
		vkFreeMemory(mDevice, bufferMemory, nullptr);
	}

	alignedBufferSize = ((size - 1) / mBufferMemoryAlignment + 1 ) * mBufferMemoryAlignment;
	
	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = alignedBufferSize;
	bufferCreateInfo.usage = usage;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // because only use one queue, do not need to share between queue families

	if(vkCreateBuffer(mDevice, &bufferCreateInfo, nullptr, &buffer) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::CREATE_BUFFER_FAILED]);
	}
	
	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(mDevice, buffer, &memoryRequirements);

	mBufferMemoryAlignment = mBufferMemoryAlignment > memoryRequirements.alignment ? mBufferMemoryAlignment : memoryRequirements.alignment;

	VkMemoryAllocateInfo memoryAllocateInfo = {}; // do not forget {} initialize
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	// VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT: write data from cpu
	memoryAllocateInfo.memoryTypeIndex = FindMemoryType(memoryRequirements.memoryTypeBits, propertyFlags);

	// manually allocate memory to buffer
	// TODO: implement memory allocator, because physical device have memory allocation limit - maxMemoryAllocationCount
	// TODO: so need to request a large memory once, and then use custom allocator to allocate by offset
	if(vkAllocateMemory(mDevice, &memoryAllocateInfo, nullptr, &bufferMemory) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::ALLOCATE_BUFFER_MEMORY_FAILED]);
	}

	// need to free memory manually (vkFreeMemory)
	vkBindBufferMemory(mDevice, buffer, bufferMemory, 0);
}

void TrVulkanRendererBase::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	// command buffer needs allocate, not create
	// allocate do not need run time error check
	VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

	{
		VkBufferCopy bufferCopyRegion = {};
		bufferCopyRegion.size = size;
		// all copy commands are treated as "transfer" operations
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &bufferCopyRegion);
	}

	EndSingleTimeCommands(commandBuffer);
}

// use cpu visible buffer as staging buffer
// use buffer that read by graphics card faster as vertex buffer
void TrVulkanRendererBase::CreateVertexBuffer()
{
	VkBuffer stagingBuffer = VK_NULL_HANDLE;
	VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;
	
	// VkDeviceSize bufferSize = sizeof(TrVulkanGlobal::Vertices[0]) * TrVulkanGlobal::Vertices.size();
	// VkDeviceSize bufferSize = sizeof(TrVulkanGlobal::TexVertices[0]) * TrVulkanGlobal::TexVertices.size();
	// VkDeviceSize bufferSize = sizeof(TrVulkanGlobal::TexVertices3D[0]) * TrVulkanGlobal::TexVertices3D.size();
	VkDeviceSize bufferSize = sizeof(mModels[0].mVertices[0]) * mModels[0].mVertices.size();

	
	// VK_BUFFER_USAGE_TRANSFER_SRC_BIT: buffer can be used as the source of a transfer command
	VkBufferUsageFlags usageStaging = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	// VK_MEMORY_PROPERTY_HOST_COHERENT_BIT: the host cache management commands are not needed to flush host writes to the device or make device writes visible to the host
	// host writes are visible to the device, device writes are also visible to the host 
	VkMemoryPropertyFlags propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	VkDeviceSize alignedBufferSize = 0;
	CreateOrReCreateBuffer(bufferSize, usageStaging, propertyFlags, alignedBufferSize, stagingBuffer, stagingBufferMemory);
	
	void* dst;
	vkMapMemory(mDevice, stagingBufferMemory, 0, alignedBufferSize, 0, &dst);
	// but may not immediately copied to the memory
	// so use VK_MEMORY_PROPERTY_HOST_COHERENT_BIT to ensure memory visible consistency
	// or invoke vkFlushMappedMemoryRanges after writing to memory, invoke vkInvalidateMappedMemoryRanges before reading from memory

	{
		// memcpy(dst, TrVulkanGlobal::Vertices.data(), (size_t) bufferSize);
		// memcpy(dst, TrVulkanGlobal::TexVertices.data(), (size_t) bufferSize);
		// memcpy(dst, TrVulkanGlobal::TexVertices3D.data(), (size_t) bufferSize);
		memcpy(dst, mModels[0].mVertices.data(), (size_t) alignedBufferSize);
	}
	
	vkUnmapMemory(mDevice, stagingBufferMemory);

	// VK_BUFFER_USAGE_TRANSFER_DST_BIT: buffer can be used as the destination of a transfer command
	// VK_BUFFER_USAGE_VERTEX_BUFFER_BIT: buffer is suitable for pass as an element of the pBuffers array to vkCmdBindVertexBuffers
	VkBufferUsageFlags usageVertex = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	VkDeviceSize alignedVertexBufferSize = 0;
	CreateOrReCreateBuffer(bufferSize, usageVertex, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, alignedVertexBufferSize, mVertexBuffer, mVertexBufferMemory);

	CopyBuffer(stagingBuffer, mVertexBuffer, alignedVertexBufferSize);
	
	vkDestroyBuffer(mDevice, stagingBuffer, nullptr);
	vkFreeMemory(mDevice, stagingBufferMemory, nullptr);
}

void TrVulkanRendererBase::CreateIndexBuffer()
{
	VkBuffer stagingBuffer = VK_NULL_HANDLE;
	VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;
	
	// VkDeviceSize bufferSize = sizeof(TrVulkanGlobal::Indices[0]) * TrVulkanGlobal::Indices.size();
	// VkDeviceSize bufferSize = sizeof(TrVulkanGlobal::TexIndices3D[0]) * TrVulkanGlobal::TexIndices3D.size();
	VkDeviceSize bufferSize = sizeof(mModels[0].mIndices[0]) * mModels[0].mIndices.size();
	
	VkBufferUsageFlags usageStaging = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	VkMemoryPropertyFlags propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	VkDeviceSize alignedBufferSize = 0;
	CreateOrReCreateBuffer(bufferSize, usageStaging, propertyFlags, alignedBufferSize, stagingBuffer, stagingBufferMemory);

	// data from memory to device memory
	void* dst;
	vkMapMemory(mDevice, stagingBufferMemory, 0, alignedBufferSize, 0, &dst);

	{
		// memcpy(dst, TrVulkanGlobal::Indices.data(), (size_t) bufferSize);
		// memcpy(dst, TrVulkanGlobal::TexIndices3D.data(), (size_t) bufferSize); 
		memcpy(dst, mModels[0].mIndices.data(), (size_t) alignedBufferSize); 
	}
	
	vkUnmapMemory(mDevice, stagingBufferMemory);

	VkBufferUsageFlags usageIndex = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	VkDeviceSize alignedIndexBufferSize = 0;
	CreateOrReCreateBuffer(bufferSize, usageIndex, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, alignedIndexBufferSize, mIndexBuffer, mIndexBufferMemory);

	// data from host-visible device memory to fast-read-write-access device memory
	CopyBuffer(stagingBuffer, mIndexBuffer, alignedIndexBufferSize);

	vkDestroyBuffer(mDevice, stagingBuffer, nullptr);
	vkFreeMemory(mDevice, stagingBufferMemory, nullptr);
}

void TrVulkanRendererBase::CreateUniformBuffers()
{
	VkDeviceSize bufferSize = sizeof(TrVulkanTransformUBO);

	mUniformBuffers.resize(TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT);
	mUniformBuffersMemory.resize(TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT);

	for(auto i = 0; i < TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT; i++)
	{
		mUniformBuffers[i] = VK_NULL_HANDLE;
		mUniformBuffersMemory[i] = VK_NULL_HANDLE;
	}

	for(size_t i = 0; i < TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		VkMemoryPropertyFlags propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		VkDeviceSize alignedBufferSize = 0;
		CreateOrReCreateBuffer(bufferSize, usageFlags, propertyFlags, alignedBufferSize, mUniformBuffers[i], mUniformBuffersMemory[i]);
	}
}

// memory type and properties
uint32_t TrVulkanRendererBase::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags propertyFlags)
{
	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &memoryProperties);

	for(uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
	{
		// correspond bit field equals 1
		if((typeFilter & (1 << i)) && memoryProperties.memoryTypes[i].propertyFlags & propertyFlags)
		{
			return i;
		}
	}
	throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::FIND_MEMORY_TYPE_FAILED]);
}

// helper allocate and begin command buffer
VkCommandBuffer TrVulkanRendererBase::BeginSingleTimeCommands()
{
	// allocate command buffer
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandPool = mCommandPool;
	commandBufferAllocateInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(mDevice, &commandBufferAllocateInfo, &commandBuffer);

	// begin command buffer
	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);

	return commandBuffer;
}

// helper end command buffer and submit to queue and wait queue and free command buffer
void TrVulkanRendererBase::EndSingleTimeCommands(VkCommandBuffer commandBuffer)
{
	// end command buffer
	vkEndCommandBuffer(commandBuffer);

	// submit to queue
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);

	// wait queue
	vkQueueWaitIdle(mGraphicsQueue);

	// free command buffer
	vkFreeCommandBuffers(mDevice, mCommandPool, 1, &commandBuffer);
}

// before and after vkCmdCopyBufferToImage, need to change image layout
void TrVulkanRendererBase::TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

	// change image layout by image memory barrier
	VkImageMemoryBarrier imageMemoryBarrier = {};
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.oldLayout = oldLayout; // if do not need to access old image data, can set to undefined
	imageMemoryBarrier.newLayout = newLayout;
	imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // if do not transmit queue family ownership
	imageMemoryBarrier.image = image;
	imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageMemoryBarrier.subresourceRange.levelCount = 1;
	imageMemoryBarrier.subresourceRange.layerCount = 1;
	imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	
	// pipeline barrier to sync resource access, also can change image layout
	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		imageMemoryBarrier.srcAccessMask = 0;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if(oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else
	{
		throw std::invalid_argument(TrVulkanGlobal::INVALID_ARGUMENT_STRING[TrVulkanGlobal::INVALID_ARGUMENT_ENUM::UNSUPPORTED_LAYOUT_TRANSITION]);
	}

	vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

	EndSingleTimeCommands(commandBuffer);
}

void TrVulkanRendererBase::CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
	VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

	VkBufferImageCopy bufferImageCopy = {};
	bufferImageCopy.bufferOffset = 0;
	bufferImageCopy.imageExtent.width = width;
	bufferImageCopy.imageExtent.height = height;
	bufferImageCopy.imageExtent.depth = 1;
	bufferImageCopy.imageOffset.x = 0;
	bufferImageCopy.imageOffset.y = 0;
	bufferImageCopy.imageOffset.z = 0;
	bufferImageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	bufferImageCopy.imageSubresource.layerCount = 1;
	bufferImageCopy.imageSubresource.mipLevel = 0;
	bufferImageCopy.imageSubresource.baseArrayLayer = 0;

	vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageCopy);
	
	EndSingleTimeCommands(commandBuffer);
}

void TrVulkanRendererBase::CreateDepthResources()
{
	VkFormat depthFormat = FindDepthFormat();
	CreateImage(mSwapChainExtent.width, mSwapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mDepthImage, mDepthImageMemory);

	mDepthImageView = CreateImageView(mDepthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

VkFormat TrVulkanRendererBase::FindDepthFormat()
{
	std::vector<VkFormat> formats = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
	return FindSupportedFormat(formats, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

// VkImageTiling: tiling arrangement of the texel blocks in memory
// VK_IMAGE_TILING_LINEAR: texels are laid out in memory in row-major order, possibly with some padding on each row
// VK_IMAGE_TILING_OPTIMAL: optimal tiling, texels are laid out in an implementagion-dependent arrangement, for more efficient memory access
VkFormat TrVulkanRendererBase::FindSupportedFormat(const std::vector<VkFormat> candidates, VkImageTiling tiling,
	VkFormatFeatureFlags features)
{
	for(VkFormat candidate : candidates)
	{
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(mPhysicalDevice, candidate, &props);

		// (props.linearTilingFeatures & features) == features -- bit and, props.linearTilingFeatures contains features
		// why linear prior to optimal
		if(tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
		{
			return candidate;
		}
		else if(tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
		{
			return candidate;
		}

		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::FIND_SUPPORTED_FORMAT_FAILED]);
		
	}
	
}

// TODO: parallel rendering in multiple frames
void TrVulkanRendererBase::DrawFrame()
{
	// sync: fence vs semaphore
	// semaphore between graphics queue and present queue
	// can get fence state by vkWaitForFences, but can not get semaphore state

	// parallel, submit 0 then wait 1, submit 1 then wait 0
	vkWaitForFences(mDevice, 1, &mInFlightFences[mCurrentFrame], VK_TRUE, UINT64_MAX);

	// get an image from the swap chain
	uint32_t imageIndex; // use this index to commit the correspond command buffer
	vkAcquireNextImageKHR(mDevice, mSwapChain, UINT64_MAX, mImageAvailableSemaphores[mCurrentFrame] ,VK_NULL_HANDLE, &imageIndex);

	UpdateGlobalConfigs();
	
	// update transform per frame
	UpdateUniformBuffer(mCurrentFrame);
	
	vkResetFences(mDevice, 1, &mInFlightFences[mCurrentFrame]);

	// not parallel, submit 0 then wait 0, submit 1 then wait 1
	// vkWaitForFences(mDevice, 1, &mInFlightFences[(mCurrentFrame + 1) % TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT], VK_TRUE, UINT64_MAX);
	// vkResetFences(mDevice, 1, &mInFlightFences[(mCurrentFrame + 1) % TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT]);
	
	// execute commands in command buffer for frame buffer attachment
	vkResetCommandBuffer(mCommandBuffers[mCurrentFrame], 0);
	RecordCommandBuffer(mCommandBuffers[mCurrentFrame], imageIndex);
	
	// submit command buffers to command queue
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = {mImageAvailableSemaphores[mCurrentFrame]};
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}; // we want to write color into image, so wait to the stage can write color attachment
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &mCommandBuffers[mCurrentFrame]; // command buffers submitted and executed

	VkSemaphore signalSemaphores[] = {mRenderFinishedSemaphores[mCurrentFrame]}; // wait & signal
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores; // sync

	// fence: sync operation when command buffers has been executed
	if(vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, mInFlightFences[mCurrentFrame]) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::SUBMIT_DRAW_COMMAND_BUFFER_FAILED]);
	}

	// return rendered image to the swap chain then present
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores; // sync 

	// specify swap chain for presenting
	VkSwapchainKHR swapChains[] = {mSwapChain};
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;
	vkQueuePresentKHR(mPresentQueue, &presentInfo);

	// vkQueueWaitIdle(mPresentQueue);
	mCurrentFrame = (mCurrentFrame + 1) % TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT;
}

void TrVulkanRendererBase::UpdateUniformBuffer(uint32_t currentImage)
{
	static std::chrono::time_point startTime = std::chrono::high_resolution_clock::now();
	std::chrono::time_point currentTime = std::chrono::high_resolution_clock::now();

	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	TrVulkanTransformUBO ubo = {};
	// mat, angle, axis
	ubo.mModel = glm::rotate(glm::mat4(mModels[0].mScale), glm::radians(mMvpModelRadiansAngle), mMvpModelAngleAxis);
	// ubo.mModel = glm::rotate(glm::mat4(1.0f), glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

	// source, target, up 
	// ubo.mView = glm::lookAt(glm::vec3(2.0f, 2.0f, 1.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.mView = glm::lookAt(mMvpViewEye, mMvpViewCenter, mMvpViewUp);
	// fov vertical angle, aspect, near, far
	ubo.mProj = glm::perspective(glm::radians(mMvpProjRadiansFovy), mSwapChainExtent.width / (float) mSwapChainExtent.height, mMvpProjZNear, mMvpProjZFar);
	// glm ( for opengl ) clip coord y axis is opposite to vulkan ???
	// ubo.mProj[1][1] *= -1;

	void* dst;
	vkMapMemory(mDevice, mUniformBuffersMemory[currentImage], 0, sizeof(ubo), 0, &dst);
	memcpy(dst, &ubo, sizeof(ubo));
	vkUnmapMemory(mDevice, mUniformBuffersMemory[currentImage]);
}

// semaphore: syne between queues or between buffers in a queue
// fence: sync between cpu and gpu
void TrVulkanRendererBase::CreateSyncObjects()
{
	mImageAvailableSemaphores.resize(TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT);
	mRenderFinishedSemaphores.resize(TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT);
	mInFlightFences.resize(TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT);
	
	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	// if use VK_FENCE_CREATE_SIGNALED_BIT to create, "signaled" state for initial state, so will not block while waiting for the semaphore
	// when to use VK_FENCE_CREATE_SIGNALED_BIT: passes semaphore to the queue immediately
	semaphoreCreateInfo.flags = 0; // VK_FENCE_CREATE_SIGNALED_BIT: parameter pCreateInfo->flags must be 0
	for(int i = 0; i < TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT; i++)
	{
		if(vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, &mImageAvailableSemaphores[i]) != VK_SUCCESS ||
		vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, &mRenderFinishedSemaphores[i]) != VK_SUCCESS)
		{
			throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::CREATE_SEMAPHORE_FAILED]);
		}
	}

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // initial status is "signaled"
	for(int i = 0; i < TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT; i++)
	{
		if(vkCreateFence(mDevice, &fenceInfo, nullptr, &mInFlightFences[i]) != VK_SUCCESS)
		{
			throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::CREATE_FENCE_FAILED]);
		}
	}
	
}

// pipeline layout specifies the sequence of descriptor set layouts
// a descriptor set layout determines the arrangement of content
// descriptors are organized into descriptor sets
// a descriptor representing a shader resource (buffer, buffer view, image view, sampler, combined image sampler)
void TrVulkanRendererBase::CreateDescriptorSetLayout()
{
	// use VkDescriptorSetLayoutBinding to describe each binding
	VkDescriptorSetLayoutBinding uboDescriptorSetLayoutBinding = {};
	uboDescriptorSetLayoutBinding.binding = 0;
	uboDescriptorSetLayoutBinding.descriptorCount = 1;
	uboDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uboDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding samplerDescriptorSetLayoutBinding = {};
	samplerDescriptorSetLayoutBinding.binding = 1;
	samplerDescriptorSetLayoutBinding.descriptorCount = 1; // accessed in a shader as an array
	samplerDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings = {uboDescriptorSetLayoutBinding, samplerDescriptorSetLayoutBinding};

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.bindingCount = 2;
	descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings.data();

	if(vkCreateDescriptorSetLayout(mDevice, &descriptorSetLayoutCreateInfo, nullptr, &mDescriptorSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::CREATE_DESCRIPTOR_SET_LAYOUT_FAILED]);
	}
}

// allocate descriptor set
void TrVulkanRendererBase::CreateDescriptorPool()
{
	VkDescriptorPoolSize uboPoolSize = {};
	uboPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboPoolSize.descriptorCount = static_cast<uint32_t>(TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT);

	VkDescriptorPoolSize samplerPoolSize = {};
	samplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerPoolSize.descriptorCount = static_cast<uint32_t>(TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT);

	std::vector<VkDescriptorPoolSize> poolSizes = {uboPoolSize, samplerPoolSize};

	VkDescriptorPoolCreateInfo poolCreateInfo = {};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.poolSizeCount = 2;
	poolCreateInfo.pPoolSizes = poolSizes.data();
	poolCreateInfo.maxSets = static_cast<uint32_t>(TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT);

	if(vkCreateDescriptorPool(mDevice, &poolCreateInfo, nullptr, &mDescriptorPool) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::CREATE_DESCRIPTOR_POOL_FAILED]);
	}
}

void TrVulkanRendererBase::CreateDescriptorSets()
{
	// layout object number should match set object number
	std::vector<VkDescriptorSetLayout> setLayouts(TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT, mDescriptorSetLayout);
	
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
	descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocateInfo.descriptorPool = mDescriptorPool;
	descriptorSetAllocateInfo.descriptorSetCount = static_cast<uint32_t>(TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT);
	descriptorSetAllocateInfo.pSetLayouts = setLayouts.data(); // layout object number must equal to descriptorSetCount
	// descriptorSetAllocateInfo.pSetLayouts = &mDescriptorSetLayout; // wrong usage

	mDescriptorSets.resize(TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT);
	if(vkAllocateDescriptorSets(mDevice, &descriptorSetAllocateInfo, mDescriptorSets.data()) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::ALLOCATE_DESCRIPTOR_SETS_FAILED]);
	}

	// config buffers referenced by descriptor
	for(uint32_t i = 0; i < TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkDescriptorBufferInfo bufferInfo = {};
		bufferInfo.buffer = mUniformBuffers[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(TrVulkanTransformUBO);

		// update config need VkWriteDescriptorSet
		std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};
		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstSet = mDescriptorSets[i];
		descriptorWrites[0].dstArrayElement = 0; // if use an array as descriptor, specify first element index of the array 
		descriptorWrites[0].pBufferInfo = &bufferInfo;

		VkDescriptorImageInfo texImageInfo = {};
		texImageInfo.sampler = mTextureSampler;
		texImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		texImageInfo.imageView = mTextureImageView;

		std::array<VkDescriptorImageInfo, 1> imageInfos = {texImageInfo};

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstSet = mDescriptorSets[i];
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].pImageInfo = imageInfos.data();

		vkUpdateDescriptorSets(mDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}
}

void TrVulkanRendererBase::CreateTextureImage()
{
	// load texture file
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = nullptr;
	for(auto texSuf : TrVulkanGlobal::texSuffix)
	{
		mModels[0].SetTexturePath(TrVulkanUtil::FilenameToTexFilename(mModels[0].mFilename, texSuf));
		pixels = stbi_load(mModels[0].mTexturePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		// mModels[0].SetTexturePath(TrVulkanUtil::FilenameToTexFilename("Tree", ".bmp"));
		// pixels = stbi_load(mModels[0].mTexturePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		if(pixels)
		{
			break;
		}
	}
	if(!pixels)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::LOAD_TEXTURE_IMAGE_FAILED]);
	}
	
	// map pixel data to staging buffer
	VkDeviceSize imageSize = texWidth * texHeight * 4;
	VkBuffer stagingBuffer = VK_NULL_HANDLE;
	VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;
	VkDeviceSize alignedBufferSize = 0;
	CreateOrReCreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, alignedBufferSize, stagingBuffer, stagingBufferMemory);

	void* dst;
	vkMapMemory(mDevice, stagingBufferMemory, 0, alignedBufferSize, 0, &dst);
	memcpy(dst, pixels, (size_t)alignedBufferSize);
	vkUnmapMemory(mDevice, stagingBufferMemory);

	// free pixel data
	stbi_image_free(pixels);

	// though shaders can access pixel data in buffer
	// VkImage allows getting color data by coord, pixel data in VkImage is "texel"

	// allocate memory to Image, similar with allocating memory to buffer
	CreateImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mTextureImage, mTextureImageMemory);
	
	// change image layout by image memory barrier and then copy buffer data to image
	TransitionImageLayout(mTextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	CopyBufferToImage(stagingBuffer, mTextureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
	TransitionImageLayout(mTextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	
	vkDestroyBuffer(mDevice, stagingBuffer, nullptr);
	vkFreeMemory(mDevice, stagingBufferMemory, nullptr);
}

void TrVulkanRendererBase::CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
	VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory)
{
	// populate create info
	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.format = format;
	imageCreateInfo.tiling = tiling;
	imageCreateInfo.usage = usage;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.extent.width = width;
	imageCreateInfo.extent.height = height;
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	// create image
	if(vkCreateImage(mDevice, &imageCreateInfo, nullptr, &image) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::CREATE_IMAGE_FAILED]);
	}

	// memory requirement
	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(mDevice, image, &memoryRequirements);

	// allocate memory
	VkMemoryAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.allocationSize = memoryRequirements.size;
	allocateInfo.memoryTypeIndex = FindMemoryType(memoryRequirements.memoryTypeBits, properties);
	if(vkAllocateMemory(mDevice, &allocateInfo, nullptr, &imageMemory) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::ALLOCATE_IMAGE_MEMORY_FAILED]);
	}

	// bind
	vkBindImageMemory(mDevice, image, imageMemory, 0);
}

// access image should use image view
void TrVulkanRendererBase::CreateTextureImageView()
{
	mTextureImageView = CreateImageView(mTextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
}

// sampler can filter and transform (address rules) texture data automatically
// oversampling: many fragments sample the same texel, to solve this using bilinear filtering
// undersampling: many texels in one fragment, to solve this using anisotropic filtering
void TrVulkanRendererBase::CreateTextureSampler()
{
	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.anisotropyEnable = true;
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR; // when magnify texture
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR; // when minify texture 
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT; // address rules
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.maxAnisotropy = 16; // limit sample number used to compute final color
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE; // use [0, 1] tex coord
	samplerCreateInfo.compareEnable = VK_FALSE; // compare to a fixed value for filtering
	samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = 0.0f;

	if(vkCreateSampler(mDevice, &samplerCreateInfo, nullptr, &mTextureSampler) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::CREATE_SAMPLER_FAILED]);
	}

	if(vkCreateSampler(mDevice, &samplerCreateInfo, nullptr, &mFontSampler) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::CREATE_SAMPLER_FAILED]);
	}
}

void TrVulkanRendererBase::LoadModels(std::vector<std::string> filenames)
{
	for(auto filename : filenames)
	{
		std::string modelFile = TrVulkanUtil::FilenameToModelFilename(filename);
		std::string texFile = TrVulkanUtil::FilenameToTexFilename(filename, ".jpg");
		TrVulkanModelBase model(filename, modelFile, texFile);
		mModels.push_back(model);
	}
}

// populate io.Fonts
void TrVulkanRendererBase::ImGuiCreateFontsTexture(VkCommandBuffer commandBuffer)
{
	ImGuiIO& io = ImGui::GetIO();
	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	size_t uploadSize = width * height * 4 * sizeof(char);

	CreateImage(width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mFontImage, mFontImageMemory);

	mFontImageView = CreateImageView(mFontImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
	
	VkBuffer stagingBuffer = VK_NULL_HANDLE;
	VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;
	VkDeviceSize alignedBufferSize = 0;
	CreateOrReCreateBuffer(uploadSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, alignedBufferSize, stagingBuffer, stagingBufferMemory);

	char* dst;
	vkMapMemory(mDevice, stagingBufferMemory, 0, alignedBufferSize, 0, (void**)(&dst));
	memcpy(dst, pixels, (size_t)alignedBufferSize);
	vkUnmapMemory(mDevice, stagingBufferMemory);

	TransitionImageLayout(mFontImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	CopyBufferToImage(stagingBuffer, mFontImage, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
	TransitionImageLayout(mFontImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	
}

void TrVulkanRendererBase::ImGuiSetFontTexId()
{
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	// Store our identifier
	io.Fonts->SetTexID((ImTextureID)mImGuiDescriptorSets[(mCurrentFrame + 1) % 2]);
}

void TrVulkanRendererBase::CreateImGuiGraphicsPipeline()
{
	std::string shaderFilePrefix = std::string(TrVulkanGlobal::SHADER_FILE_STRING[TrVulkanGlobal::SHADER_FILE_ENUM::imGuiBase]);
	auto vertShaderCompiledCode = TrVulkanUtil::ReadFile(shaderFilePrefix + TrVulkanGlobal::vertSuffix);
	auto fragShaderCompiledCode = TrVulkanUtil::ReadFile(shaderFilePrefix + TrVulkanGlobal::fragSuffix);
	
	VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCompiledCode);
	VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCompiledCode);

	VkPipelineShaderStageCreateInfo vertShaderStageCreateInfo = {};
	vertShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageCreateInfo.module = vertShaderModule;
	vertShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageCreateInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageCreateInfo = {};
	fragShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageCreateInfo.module = fragShaderModule;
	fragShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageCreateInfo.pName = "main";
	
	VkPipelineShaderStageCreateInfo shaderStageCreateInfos[] =
	{
		vertShaderStageCreateInfo,
		fragShaderStageCreateInfo,
	};
	
	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	std::array<VkVertexInputBindingDescription, 1> bindingDesc = {};
	bindingDesc[0].stride = sizeof(ImDrawVert);
	bindingDesc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	std::array<VkVertexInputAttributeDescription, 3> attributeDesc = {};
	attributeDesc[0].location = 0;
	attributeDesc[0].binding = bindingDesc[0].binding;
	attributeDesc[0].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDesc[0].offset = IM_OFFSETOF(ImDrawVert, pos);
	attributeDesc[1].location = 1;
	attributeDesc[1].binding = bindingDesc[0].binding;
	attributeDesc[1].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDesc[1].offset = IM_OFFSETOF(ImDrawVert, uv);
	attributeDesc[2].location = 2;
	attributeDesc[2].binding = bindingDesc[0].binding;
	attributeDesc[2].format = VK_FORMAT_R8G8B8A8_UNORM;
	attributeDesc[2].offset = IM_OFFSETOF(ImDrawVert, col);
	
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = bindingDesc.data();

	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = attributeDesc.size();
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = attributeDesc.data();

	// input assembly: what kind of primitive topology does vertex data define, whether to use primitive restart
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {};
	inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	// list or strip, point line or triangle
	inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	// if enabled, use a special index to restart, index after this index reset to the first vert of primitive
	inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

	// viewport and scissor
	// viewport: image map to frame buffer
	// scissor: pixels in which region is actually saved in frame buffer
	/*VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = (float) mSwapChainExtent.height;
	viewport.width = (float) mSwapChainExtent.width; // mSwapChainExtent may different from window
	viewport.height = -((float) mSwapChainExtent.height);
	
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	VkRect2D scissor = {};
	scissor.offset = {0, 0};
	scissor.extent = mSwapChainExtent;*/

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.scissorCount = 1;
	// viewportStateCreateInfo.pViewports = &viewport;
	// viewportStateCreateInfo.pScissors = &scissor;

	// rasterization
	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	// should fragments outside near plane and far plane be clamped to planes, rather than discarded
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	// no fragments output to frame buffer
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateCreateInfo.lineWidth = 1.0f;
	// cull front, back, front and back
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
	// front face condition: clockwise or counter clockwise
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	// multisample
	VkPipelineMultisampleStateCreateInfo multiSampleCreateInfo = {};
	multiSampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multiSampleCreateInfo.sampleShadingEnable = VK_FALSE;
	multiSampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	
	// color blend: fragment color returned by fragment shader should blend with color in frame buffer corresponding pixel
	// for each frame buffer 
	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
	colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	// colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
	colorBlendAttachmentState.blendEnable = VK_TRUE;
	// colorBlendAttachmentState.blendEnable = VK_FALSE;
	colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	// for global color blend
	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
	colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCreateInfo.attachmentCount = 1;
	colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;

	/*colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendStateCreateInfo.blendConstants[0] = 0.0f;
	colorBlendStateCreateInfo.blendConstants[1] = 0.0f;
	colorBlendStateCreateInfo.blendConstants[2] = 0.0f;
	colorBlendStateCreateInfo.blendConstants[3] = 0.0f;*/

	// few pipeline states can change dynamically without rebuild pipeline
	std::vector<VkDynamicState> dynamicStates =
	{
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};
	// need to re-specify values when draw
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

	// can make dynamic config to shader (uniform variables can be modified dynamically after creating pipeline)
	VkPipelineLayoutCreateInfo layoutCreateInfo = {};
	VkPushConstantRange pushConstants[1] = {};
	pushConstants[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushConstants[0].offset = sizeof(float) * 0;
	pushConstants[0].size = sizeof(float) * 4;
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCreateInfo.setLayoutCount = 1;
	layoutCreateInfo.pushConstantRangeCount = 1;
	layoutCreateInfo.pSetLayouts = &mImGuiDescriptorSetLayout;
	layoutCreateInfo.pPushConstantRanges = pushConstants;
	if(vkCreatePipelineLayout(mDevice, &layoutCreateInfo, nullptr, &mImGuiPipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::CREATE_LAYOUT_FAILED]);
	}

	VkPipelineDepthStencilStateCreateInfo depthStencil = {};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_FALSE;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_FALSE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL; // new frag can write into attachment when its depth less than depth in depth buffer
	depthStencil.minDepthBounds = 0.0f;
	depthStencil.maxDepthBounds = 1.0f;
	// stencil
	depthStencil.stencilTestEnable = VK_FALSE;
	depthStencil.front = {};
	depthStencil.back = {};
	
	// create graphics pipeline
	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = 2; // vert & frag, each has a code file
	pipelineCreateInfo.pStages = shaderStageCreateInfos;
	// fixed stages
	pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multiSampleCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo; // for reduce rebuild graphics pipeline
	pipelineCreateInfo.pDepthStencilState = &depthStencil;
	
	pipelineCreateInfo.layout = mImGuiPipelineLayout; // for uniform variables
	pipelineCreateInfo.renderPass = mRenderPass;
	pipelineCreateInfo.subpass = 1; // index
	// pipelineCreateInfo.subpass = 0;
	// for creating a new graphics pipeline with a created pipeline
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;

	if(vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &mImGuiGraphicsPipeline) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::CREATE_GRAPHICS_PIPELINE_FAILED]);
	}

	vkDestroyShaderModule(mDevice, vertShaderModule, nullptr);
	vkDestroyShaderModule(mDevice, fragShaderModule, nullptr);
}

void TrVulkanRendererBase::CreateImGuiDescriptorSetLayout()
{
	VkDescriptorSetLayoutBinding samplerDescriptorSetLayoutBinding = {};
	samplerDescriptorSetLayoutBinding.binding = 0;
	samplerDescriptorSetLayoutBinding.descriptorCount = 1; // accessed in a shader as an array
	samplerDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings = {samplerDescriptorSetLayoutBinding};

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.bindingCount = 1;
	descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings.data();

	if(vkCreateDescriptorSetLayout(mDevice, &descriptorSetLayoutCreateInfo, nullptr, &mImGuiDescriptorSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::CREATE_DESCRIPTOR_SET_LAYOUT_FAILED]);
	}
}

void TrVulkanRendererBase::CreateImGuiDescriptorPool()
{
	VkDescriptorPoolSize samplerPoolSize = {};
	samplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerPoolSize.descriptorCount = static_cast<uint32_t>(TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT);

	std::vector<VkDescriptorPoolSize> poolSizes = {samplerPoolSize};

	VkDescriptorPoolCreateInfo poolCreateInfo = {};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolCreateInfo.pPoolSizes = poolSizes.data();
	poolCreateInfo.maxSets = static_cast<uint32_t>(TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT);

	if(vkCreateDescriptorPool(mDevice, &poolCreateInfo, nullptr, &mImGuiDescriptorPool) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::CREATE_DESCRIPTOR_POOL_FAILED]);
	}
}

void TrVulkanRendererBase::CreateImGuiDescriptorSets()
{
	// layout object number should match set object number
	std::vector<VkDescriptorSetLayout> setLayouts(TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT, mImGuiDescriptorSetLayout);
	
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
	descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocateInfo.descriptorPool = mImGuiDescriptorPool;
	descriptorSetAllocateInfo.descriptorSetCount = static_cast<uint32_t>(TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT);
	descriptorSetAllocateInfo.pSetLayouts = setLayouts.data(); // layout object number must equal to descriptorSetCount
	// descriptorSetAllocateInfo.pSetLayouts = &mDescriptorSetLayout; // wrong usage

	mImGuiDescriptorSets.resize(TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT);
	if(vkAllocateDescriptorSets(mDevice, &descriptorSetAllocateInfo, mImGuiDescriptorSets.data()) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::ALLOCATE_DESCRIPTOR_SETS_FAILED]);
	}

	// config buffers referenced by descriptor
	for(uint32_t i = 0; i < TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkDescriptorImageInfo fontImageInfo = {};
		fontImageInfo.sampler = mFontSampler;
		fontImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		fontImageInfo.imageView = mFontImageView;

		std::vector<VkDescriptorImageInfo> imageInfos = {fontImageInfo};
		
		VkWriteDescriptorSet samplerDescriptorWrite = {};
		samplerDescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		samplerDescriptorWrite.descriptorCount = 1;
		samplerDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerDescriptorWrite.dstBinding = 0;
		samplerDescriptorWrite.dstSet = mImGuiDescriptorSets[i];
		samplerDescriptorWrite.dstArrayElement = 0;
		samplerDescriptorWrite.pImageInfo = imageInfos.data();
		
		// update config need VkWriteDescriptorSet
		std::vector<VkWriteDescriptorSet> descriptorWrites = {samplerDescriptorWrite};

		vkUpdateDescriptorSets(mDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}
}

void TrVulkanRendererBase::InitImGuiBufferNum()
{
	mImGuiVertexBuffers.resize(TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT);
	mImGuiIndexBuffers.resize(TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT);
	mImGuiVertexBufferMemories.resize(TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT);
	mImGuiIndexBufferMemories.resize(TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT);
	for(auto i = 0; i < TrVulkanGlobal::MAX_FRAMES_IN_FLIGHT; i++)
	{
		mImGuiVertexBuffers[i] = VK_NULL_HANDLE;
		mImGuiIndexBuffers[i] = VK_NULL_HANDLE;
	}
}

void TrVulkanRendererBase::CreateTestRenderPassImGuiOnly()
{
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = mSwapChainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // process approach to attachment before render
	// the contents generated during the render pass and within the render area are written to memory
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // process approach to attachment after render
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // process approach to stencil of attachment before render
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // process approach to stencil of attachment after render
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // attachment layout before render
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // attachment layout after render, image is used to present in swapchain

	// a render pass can contain multiple sub passes
	// a sub pass references to the frame buffer content processed by last phase
	// each sub pass can reference one or more attachments
	VkAttachmentReference colorAttachmentReference = {};
	colorAttachmentReference.attachment = 0;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // best image layout for using as color attachment


	

	std::array<VkAttachmentDescription, 2> attachments = {colorAttachment};

	VkSubpassDescription subPassDescription1 = {};
	// pipeline type that want to bind
	subPassDescription1.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // means this is a graphics sub pass, rather than a compute sub pass
	subPassDescription1.colorAttachmentCount = 1;
	subPassDescription1.pColorAttachments = &colorAttachmentReference;
	
	std::array<VkSubpassDescription, 2> subPassDescriptions = {subPassDescription1};
	// sync 
	// define dependencies between sub passes
	VkSubpassDependency subpassDependency1 = {};
	subpassDependency1.srcSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependency1.dstSubpass = 0;
	subpassDependency1.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	subpassDependency1.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	subpassDependency1.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	subpassDependency1.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	std::array<VkSubpassDependency, 2> subpassDependencies = {subpassDependency1};


	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = 1;
	renderPassCreateInfo.pAttachments = attachments.data();
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = subPassDescriptions.data();

	renderPassCreateInfo.dependencyCount = 1;
	renderPassCreateInfo.pDependencies = subpassDependencies.data();

	

	if(vkCreateRenderPass(mDevice, &renderPassCreateInfo, nullptr, &mRenderPass) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::CREATE_RENDER_PASS_FAILED]);
	}
}

void TrVulkanRendererBase::UpdateGlobalConfigs()
{
	// MVP Params
	// Model
	mModels[0].mScale = TrVulkanGlobal::modelScaleRate;
	
	mMvpModelAngleAxis = TrVulkanGlobal::modelAngleAxis;
	mMvpModelRadiansAngle = TrVulkanGlobal::modelRadiansAngle;
	// mMvpModelRadiansAngle = 45.0f;
	// View
	mMvpViewEye = TrVulkanGlobal::viewEye;
	mMvpViewCenter = TrVulkanGlobal::viewCenter;
	mMvpViewUp = TrVulkanGlobal::viewUp;
	// Proj
	mMvpProjRadiansFovy = TrVulkanGlobal::projRadiansFovy;
	mMvpProjZNear = TrVulkanGlobal::projZNear;
	mMvpProjZFar = TrVulkanGlobal::projZFar;
}

// all extensions needed by vulkan instance (for GLFW and for debugging)
std::vector<const char*> TrVulkanRendererBase::GetRequiredExtensions()
{
	// vulkan instance extensions required by GLFW
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	// extension for debugging
	if(mbEnableValidationLayers)
	{
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);  // VK_EXT_debug_utils
	}
	
	return extensions;
}


bool TrVulkanRendererBase::CheckDeviceExtensionSupport(VkPhysicalDevice device)
{
	uint32_t deviceExtensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &deviceExtensionCount, nullptr);

	std::vector<VkExtensionProperties> availableDeviceExtensions(deviceExtensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &deviceExtensionCount, availableDeviceExtensions.data());

	std::set<std::string> requiredExtensions(TrVulkanGlobal::deviceExtensions.begin(), TrVulkanGlobal::deviceExtensions.end());

	for(const VkExtensionProperties& extension : availableDeviceExtensions)
	{
		requiredExtensions.erase(extension.extensionName);
	}
	
	return requiredExtensions.empty();
}

// check before creating instance
// sometimes the requested extension is not necessary, the program can run without that extension
void TrVulkanRendererBase::CheckExtensionSupport()
{
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> extensions(extensionCount);
	// vector data(): pointer to the begin of data address
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
	std::cout << "Available extensions: " << std::endl;
	for(const VkExtensionProperties& extension : extensions)
	{
		std::cout << "\t" << extension.extensionName << std::endl;
	}
}

// validation layers needed are configured in TrVulkanGlobalConfigs.h 
bool TrVulkanRendererBase::CheckValidationLayerSupport()
{
	uint32_t layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
	for(const char* validationLayer: TrVulkanGlobal::validationLayers)
	{
		bool bLayerFound = false;
		for(const VkLayerProperties& availableLayer : availableLayers)
		{
			if(strcmp(validationLayer, availableLayer.layerName) == 0)
			{
				bLayerFound = true;
				break;
			}
		}
		if(!bLayerFound)
		{
			return false;
		}
	}
	return true;
}

// used for receiving debug info
// return value: if the Vulkan API processed by validation layers is interrupted
// usually return true when test validation layer itself
VkBool32 TrVulkanRendererBase::DebugCallBack(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;
	return VK_FALSE;
}

