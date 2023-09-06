#include "TrVulkanRendererBase.h"
#include <map>

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
	PickHighestWeightScorePhysicalDevice(); // or can PickFirstValidPhysicalDevice()

}

void TrVulkanRendererBase::OnRender()
{
	// if the window close button is pressed
	while (!glfwWindowShouldClose(mWindow))
	{
		glfwPollEvents();
	}
}

void TrVulkanRendererBase::OnCleanup()
{
	if(mbEnableValidationLayers)
	{
		DestroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger, nullptr);
	}
	
	// manually destroy logical device
	vkDestroyDevice(mDevice, nullptr);

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

void TrVulkanRendererBase::PickFirstValidPhysicalDevice()
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
		if(IsDeviceSuitable(device))
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

void TrVulkanRendererBase::PickHighestWeightScorePhysicalDevice()
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
		uint32_t score = CalcDeviceSuitabilityScore(device, VK_QUEUE_GRAPHICS_BIT);
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

bool TrVulkanRendererBase::IsDeviceSuitable(VkPhysicalDevice device)
{
	// basic device properties: name, type, supported vulkan version
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);

	// support for texture compression, 64 bits float for shader, multiple viewports...
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);


	TrVulkanQueueFamilyIndices indices = FindQueueFamilies(device, VK_QUEUE_GRAPHICS_BIT);
	
	return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && deviceFeatures.geometryShader && indices.IsComplete();
}

uint32_t TrVulkanRendererBase::CalcDeviceSuitabilityScore(VkPhysicalDevice device, VkQueueFlagBits queueFlag)
{
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);

	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
	
	uint32_t score = 0;

	if(!deviceFeatures.geometryShader)
	{
		return 0;
	}

	// queues should responsible for graphics
	TrVulkanQueueFamilyIndices indices = FindQueueFamilies(device, queueFlag);
	if(!indices.IsComplete())
	{
		return 0;
	}

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

	// populate VkDeviceQueueCreateInfo, which describes queues needed
	VkDeviceQueueCreateInfo queueCreateInfo = {};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	// queue family properties are get from vkGetPhysicalDeviceQueueFamilyProperties, indices.mGraphicsFamily is the location in std::vector<VkQueueFamilyProperties> 
	queueCreateInfo.queueFamilyIndex = indices.mGraphicsFamily;
	// for each queue family, rarely needs more than one queue (can create command buffers in threads, commit once in main thread)
	queueCreateInfo.queueCount = 1;
	float queuePriority = 1.0f;
	// priority to execute command buffer
	queueCreateInfo.pQueuePriorities = &queuePriority;

	// specify features to use
	VkPhysicalDeviceFeatures deviceFeatures = {};

	// create logical device, like create instance
	// device level create info, compared to instance level create info
	VkDeviceCreateInfo logicalDeviceCreateInfo = {};
	logicalDeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	// create queue along with the logical device, destroyed automatically when destroy the logical device
	logicalDeviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	logicalDeviceCreateInfo.queueCreateInfoCount = 1;
	logicalDeviceCreateInfo.pEnabledFeatures = &deviceFeatures;
	logicalDeviceCreateInfo.enabledExtensionCount = 0;
	if(mbEnableValidationLayers)
	{
		logicalDeviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(TrVulkanGlobal::validationLayers.size());
		logicalDeviceCreateInfo.ppEnabledExtensionNames = TrVulkanGlobal::validationLayers.data();
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
	vkGetDeviceQueue(mDevice, indices.mGraphicsFamily, 0, &mGraphicsQueue);
	
}

TrVulkanQueueFamilyIndices TrVulkanRendererBase::FindQueueFamilies(VkPhysicalDevice device, VkQueueFlagBits queueFlag)
{
	TrVulkanQueueFamilyIndices indices;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	// find a queue family which support VK_QUEUE_GRAPHICS_BIT
	int i = 0;
	for(const VkQueueFamilyProperties& queueFamily : queueFamilies)
	{
		if(queueFamily.queueCount > 0 && queueFamily.queueFlags & queueFlag)
		{
			indices.mGraphicsFamily = i;
		}

		if(indices.IsComplete())
		{
			break;
		}
		
		i++;
	}

	return indices;
}

// all extensions needed (for GLFW and for debugging)
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

