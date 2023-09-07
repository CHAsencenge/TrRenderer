#include "TrVulkanRendererBase.h"
#include <map>
#include <set>

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
}

void TrVulkanRendererBase::OnRender()
{
	// if the window close button is pressed
	while (!glfwWindowShouldClose(mWindow))
	{
		glfwPollEvents();
	}
}

// often vkCreateXXX needs vkDestroyXXX
void TrVulkanRendererBase::OnCleanup()
{
	if(mbEnableValidationLayers)
	{
		DestroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger, nullptr);
	}

	// manually destroy swap chain, earlier than logical device
	vkDestroySwapchainKHR(mDevice, mSwapChain, nullptr);
	
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
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	// extensions needed (for glfw, debugging......)
	uint32_t glfwExtensionCount = 0;
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
	
	return deviceFeatures.geometryShader && indices.IsComplete() && bDeviceExtensionSupported && bSwapChainAdequate;
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
	// no preferred format
	if(availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED)
	{
		return {VK_FORMAT_B8G8R8A8_SRGB, VK_COLORSPACE_SRGB_NONLINEAR_KHR};
	}
	// have format list
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
	uint32_t imageCount = swapChainSupportDetails.mCapabilities.minImageCount + 1;
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

