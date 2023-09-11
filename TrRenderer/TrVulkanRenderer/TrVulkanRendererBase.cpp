#include "TrVulkanRendererBase.h"

#include <csignal>
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
	CreateImageViews();
	CreateRenderPass();
	CreateGraphicsPipeline();
	CreateFrameBuffers();
	CreateCommandPool();
	CreateCommandBuffer();
	CreateSyncObjects();
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

	vkDestroySemaphore(mDevice, mImageAvailableSemaphore, nullptr);
	vkDestroySemaphore(mDevice, mRenderFinishedSemaphore, nullptr);
	vkDestroyFence(mDevice, mInFlightFence, nullptr);

	

	vkDestroyPipeline(mDevice, mGraphicsPipeline, nullptr);

	vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);

	// before destroying render pass and image view 
	for(VkFramebuffer& framebuffer : mSwapChainFrameBuffers)
	{
		vkDestroyFramebuffer(mDevice, framebuffer, nullptr);
	}

	vkDestroyRenderPass(mDevice, mRenderPass, nullptr);

	for(VkImageView& imageView : mSwapChainImageViews)
	{
		vkDestroyImageView(mDevice, imageView, nullptr);
	}

	// manually destroy swap chain, earlier than logical device
	vkDestroySwapchainKHR(mDevice, mSwapChain, nullptr);

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

void TrVulkanRendererBase::CreateImageViews()
{
	mSwapChainImageViews.resize(mSwapChainImages.size());
	for(size_t i = 0; i < mSwapChainImages.size(); i++)
	{
		VkImageViewCreateInfo imageViewCreateInfo = {};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.image = mSwapChainImages[i];
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = mSwapChainImageFormat;
		// color channel mapping, for example, single color textures, mapping all channels to red
		imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		// image usage
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // used as RT
		imageViewCreateInfo.subresourceRange.layerCount = 1; // if VR, multiple layers (left eye layer, right eye layer), multiple views for one image
		imageViewCreateInfo.subresourceRange.levelCount = 1; // no LOD
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;

		if(vkCreateImageView(mDevice, &imageViewCreateInfo, nullptr, &mSwapChainImageViews[i]) != VK_SUCCESS)
		{
			throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::CREATE_IMAGE_VIEW_FAILED]);
		}
	}
	
}

void TrVulkanRendererBase::CreateRenderPass()
{
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = mSwapChainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // image is used to present in swapchain

	// a render pass can contain multiple sub passes
	// a sub pass references to the frame buffer content processed by last phase
	// each sub pass can reference one or more attachments
	VkAttachmentReference colorAttachmentReference = {};
	colorAttachmentReference.attachment = 0;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // ?

	VkSubpassDescription subPassDescription = {};
	subPassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // means this is a graphics sub pass, rather than a compute sub pass
	subPassDescription.colorAttachmentCount = 1;
	subPassDescription.pColorAttachments = &colorAttachmentReference;

	// sync
	VkSubpassDependency subpassDependency = {};
	subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependency.dstSubpass = 0;
	subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependency.srcAccessMask = 0;
	subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = 1;
	renderPassCreateInfo.pAttachments = &colorAttachment;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subPassDescription;

	renderPassCreateInfo.dependencyCount = 1;
	renderPassCreateInfo.pDependencies = &subpassDependency;

	if(vkCreateRenderPass(mDevice, &renderPassCreateInfo, nullptr, &mRenderPass) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::CREATE_RENDER_PASS_FAILED]);
	}
}

void TrVulkanRendererBase::CreateGraphicsPipeline()
{
	auto vertShaderCompiledCode = TrVulkanUtil::ReadFile("Shaders/vert.spv");
	auto fragShaderCompiledCode = TrVulkanUtil::ReadFile("Shaders/frag.spv");

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
	// binding: offset between vertices, per-vertex or per-instance
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = nullptr;
	// attribute: attribute for shader variable
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = nullptr;

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
	viewport.y = 0.0f;
	viewport.width = (float) mSwapChainExtent.width; // mSwapChainExtent may different from window
	viewport.height = (float) mSwapChainExtent.height;
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
	// front face condition: clockwise or counter clockwise
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
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
	dynamicStateCreateInfo.dynamicStateCount = 2;
	dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

	// can make dynamic config to shader (uniform variables can be modified dynamically after creating pipeline)
	VkPipelineLayoutCreateInfo layoutCreateInfo = {};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCreateInfo.setLayoutCount = 0;
	layoutCreateInfo.pushConstantRangeCount = 0;
	layoutCreateInfo.pSetLayouts = nullptr;
	layoutCreateInfo.pPushConstantRanges = nullptr;
	if(vkCreatePipelineLayout(mDevice, &layoutCreateInfo, nullptr, &mPipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::CREATE_LAYOUT_FAILED]);
	}
	
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

void TrVulkanRendererBase::CreateFrameBuffers()
{
	mSwapChainFrameBuffers.resize(mSwapChainImageViews.size());
	for(size_t i = 0; i < mSwapChainFrameBuffers.size(); i++)
	{
		VkImageView attachments[] =
		{
			mSwapChainImageViews[i]
		};
		VkFramebufferCreateInfo frameBufferCreateInfo = {};
		frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frameBufferCreateInfo.renderPass = mRenderPass;
		frameBufferCreateInfo.width = mSwapChainExtent.width;
		frameBufferCreateInfo.height = mSwapChainExtent.height;
		frameBufferCreateInfo.attachmentCount = 1;
		frameBufferCreateInfo.pAttachments = attachments;
		frameBufferCreateInfo.layers = 1; // image layer count
		
		if(vkCreateFramebuffer(mDevice, &frameBufferCreateInfo, nullptr, &mSwapChainFrameBuffers[i]) != VK_SUCCESS)
		{
			throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::CREATE_FRAME_BUFFER_FAILED]);
		}
	}
}

void TrVulkanRendererBase::CreateCommandPool()
{
	TrVulkanQueueFamilyIndices queueFamilyIndices = FindQueueFamilies(mPhysicalDevice, VK_QUEUE_GRAPHICS_BIT);
	VkCommandPoolCreateInfo commandPoolCreateInfo = {};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	// each command buffer object allocated by command pool can only be committed to a certain type of queue
	commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndices.mGraphicsFamily.value(); // for draw command
	// optimizing
	// VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
	// VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // Attempt to reset VkCommandBuffer created from VkCommandPool that does NOT have the VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT bit set

	if(vkCreateCommandPool(mDevice, &commandPoolCreateInfo, nullptr, &mCommandPool) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::CREATE_COMMAND_POOL_FAILED]);
	}
}

void TrVulkanRendererBase::CreateCommandBuffer()
{
	// because draw operations process on frame buffer, we need to allocate a command buffer for each image in swap chain
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.commandPool = mCommandPool;
	// VK_COMMAND_BUFFER_LEVEL_PRIMARY: can be committed to queue and executed, can not be invoked by other command buffer objects
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = 1;

	if(vkAllocateCommandBuffers(mDevice, &commandBufferAllocateInfo, &mCommandBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::ALLOCATE_COMMAND_BUFFER_FAILED]);
	}
}

void TrVulkanRendererBase::RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
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

	VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
	renderPassBeginInfo.clearValueCount = 1;
	renderPassBeginInfo.pClearValues = &clearColor;

	// VK_SUBPASS_CONTENTS_INLINE: all commands are in main command buffer
	vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	// bind graphics pipeline
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mGraphicsPipeline);

	// Cmds
	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float) mSwapChainExtent.width;
	viewport.height = (float) mSwapChainExtent.height;
	viewport.minDepth = 0;
	viewport.maxDepth = 1;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.offset = {0, 0};
	scissor.extent = mSwapChainExtent;
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	vkCmdDraw(commandBuffer, 3, 1, 0, 0);
	vkCmdEndRenderPass(commandBuffer);
	if(vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::RECORD_COMMAND_BUFFER_END_FAILED]);
	}
}

void TrVulkanRendererBase::DrawFrame()
{
	// sync: fence vs semaphore
	// can get fence state by vkWaitForFences, but can not get semaphore state

	vkWaitForFences(mDevice, 1, &mInFlightFence, VK_TRUE, UINT64_MAX);
	vkResetFences(mDevice, 1, &mInFlightFence);
	
	// get an image from the swap chain
	uint32_t imageIndex; // use this index to commit the correspond command buffer
	vkAcquireNextImageKHR(mDevice, mSwapChain, UINT64_MAX, mImageAvailableSemaphore ,VK_NULL_HANDLE, &imageIndex);

	// execute commands in command buffer for frame buffer attachment
	vkResetCommandBuffer(mCommandBuffer, 0);
	RecordCommandBuffer(mCommandBuffer, imageIndex);

	// submit command buffers to command queue
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = {mImageAvailableSemaphore};
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}; // we want to write color into image, so wait to the stage can write color attachment
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &mCommandBuffer; // command buffers submitted and executed

	VkSemaphore signalSemaphores[] = {mRenderFinishedSemaphore}; // wait & signal
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	// fence: sync operation when command buffers executed
	if(vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, mInFlightFence) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::SUBMIT_DRAW_COMMAND_BUFFER_FAILED]);
	}

	// return rendered image to the swap chain then present
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	// specify swap chain for presenting
	VkSwapchainKHR swapChains[] = {mSwapChain};
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;
	vkQueuePresentKHR(mPresentQueue, &presentInfo);

	// vkQueueWaitIdle(mPresentQueue);
}

void TrVulkanRendererBase::CreateSyncObjects()
{
	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreCreateInfo.flags = 0; // VK_FENCE_CREATE_SIGNALED_BIT: parameter pCreateInfo->flags must be 0
	if(vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, &mImageAvailableSemaphore) != VK_SUCCESS ||
		vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, &mRenderFinishedSemaphore) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::CREATE_SEMAPHORE_FAILED]);
	}

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // ?
	if(vkCreateFence(mDevice, &fenceInfo, nullptr, &mInFlightFence) != VK_SUCCESS)
	{
		throw std::runtime_error(TrVulkanGlobal::RUNTIME_ERROR_STRING[TrVulkanGlobal::RUNTIME_ERROR_ENUM::CREATE_FENCE_FAILED]);
	}
	
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

