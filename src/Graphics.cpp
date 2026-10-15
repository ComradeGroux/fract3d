#include "Graphics.hpp"

#define VOLK_IMPLEMENTATION
#include <volk.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_log.h>

#include <array>
#include <filesystem>
#include <stdexcept>
#include <vector>
#include <fstream>
#include <iostream>

constexpr uint32_t	MAGIC_SPIRV = 0x07230203;
struct ShaderFile {
	const char*	source;
	const char*	binary;
};
constexpr std::array<ShaderFile, 2>	SHADERS = {{
	{ "shader/base.vert", "build/shaders-spv/base.vert.spv" },
	{ "shader/mandelbox.frag", "build/shaders-spv/mandelbox.frag.spv" }
}};

static inline void	chk(VkResult result)
{
	if (result != VK_SUCCESS)
	{
		std::cerr << "error code: " << result << std::endl;
		throw std::runtime_error("Vulkan failed");
	}
}

Graphics::Graphics(void)
{
	if (!SDL_Init(SDL_INIT_VIDEO))
		throw std::runtime_error(SDL_GetError());

	if (!SDL_Vulkan_LoadLibrary(NULL))
		throw std::runtime_error(SDL_GetError());
	volkInitializeCustom((PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr());

	_createVkApplicationInfo();
	_createVkInstance();
	_createVkDeviceAndQueue();
	_createAllocator();
	_createWindowAndSurface();
	_createSwapchain();
	_createSwapchainImages();
	_createSyncObjects();
	_prepareShaders();
	_createPipelines();
	_keyboardState = SDL_GetKeyboardState(nullptr);
	SDL_SetWindowRelativeMouseMode(_window, true);
}

Graphics::~Graphics(void)
{
	vkDeviceWaitIdle(_device);

	for (int i = 0; i < E_PIPELINE_COUNT; i++)
		vkDestroyPipeline(_device, _pipelines[i], nullptr);
	vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);
	for (uint32_t i = 0; i < _maxFramesInFlight; i++)
	{
		vkDestroySemaphore(_device, _semImageAvailable[i], nullptr);
		vkDestroySemaphore(_device, _semRenderFinished[i], nullptr);
		vkDestroyFence(_device, _fenInFlight[i], nullptr);
	}
	for (uint32_t i = 0; i < _swapchainImageViews.size(); i++)
		vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
	vkDestroySwapchainKHR(_device, _swapchain, nullptr);
	vmaDestroyAllocator(_allocator);
	SDL_Vulkan_DestroySurface(_instance, _surface, nullptr);
	SDL_DestroyWindow(_window);
	vkDestroyCommandPool(_device, _commandPool, nullptr);
	vkDestroyDevice(_device, nullptr);
	vkDestroyInstance(_instance, nullptr);
	volkFinalize();
	SDL_Vulkan_UnloadLibrary();
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	SDL_Quit();
}

void	Graphics::_createVkApplicationInfo(void)
{
	_appInfo = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pNext = nullptr,
		.pApplicationName = "nibbler - Vulkan",
		.applicationVersion = 1,
		.pEngineName = "nibblerVulkan",
		.engineVersion = 1,
		.apiVersion = VK_API_VERSION_1_3
	};
}

void	Graphics::_createVkInstance(void)
{
	uint32_t	instanceExtensionsCount{ 0 };
	char const* const* instanceExtensions{ SDL_Vulkan_GetInstanceExtensions(&instanceExtensionsCount) };

	uint32_t	layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	std::vector<VkLayerProperties>	availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const auto& layer: availableLayers)
	{
		if (strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0)
		{
			_hasValidationLayer = true;
			break;
		}
	}

	const char*	layers[] = { "VK_LAYER_KHRONOS_validation" };
	VkInstanceCreateInfo	instanceCI{
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext = nullptr,
		.flags = {},
		.pApplicationInfo = &_appInfo,
		.enabledLayerCount = 0,
		.ppEnabledLayerNames = {},
		.enabledExtensionCount = instanceExtensionsCount,
		.ppEnabledExtensionNames = instanceExtensions,
	};
	if (_hasValidationLayer)
	{
		instanceCI.enabledLayerCount = 1;
		instanceCI.ppEnabledLayerNames = layers;
	}
	chk(vkCreateInstance(&instanceCI, nullptr, &_instance));
	volkLoadInstance(_instance);
}

void	Graphics::_selectVkPhysicalDevice(void)
{
	uint32_t			physicalDevicesCount{ 0 };
	chk(vkEnumeratePhysicalDevices(_instance, &physicalDevicesCount, nullptr));
	std::vector<VkPhysicalDevice>	physicalDevices(physicalDevicesCount);
	chk(vkEnumeratePhysicalDevices(_instance, &physicalDevicesCount, physicalDevices.data()));

	VkPhysicalDeviceProperties2 deviceProperties{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
		.pNext = nullptr,
		.properties = {}
	};
	for (uint32_t i = 0; i < physicalDevicesCount; i++)
	{
		vkGetPhysicalDeviceProperties2(physicalDevices[i], &deviceProperties);
		if (deviceProperties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			_physicalDevice = physicalDevices[i];
			break;
		}
		if (_physicalDevice == VK_NULL_HANDLE)
			_physicalDevice = physicalDevices[i];
	}
	if (_physicalDevice == VK_NULL_HANDLE)
		throw std::runtime_error("No Vulkan GPU compatible found");
}

void	Graphics::_createVkDeviceAndQueue(void)
{
	_selectVkPhysicalDevice();

	uint32_t	queueFamilyCount{ 0 };
	vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &queueFamilyCount, queueFamilies.data());
	
	uint32_t graphicsQueueFamily{ UINT32_MAX };
	for (uint32_t i = 0; i < queueFamilyCount; i++) {
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			graphicsQueueFamily = i;
			break;
		}
	}
	if (!SDL_Vulkan_GetPresentationSupport(_instance, _physicalDevice, graphicsQueueFamily))
		throw std::runtime_error("No graphics queue family found");

	const float	priority = 1.0f;
	VkDeviceQueueCreateInfo	queueCI{
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.pNext = nullptr,
		.flags = {},
		.queueFamilyIndex = graphicsQueueFamily,
		.queueCount = 1,
		.pQueuePriorities = &priority
	};

	const char*	layers[] = { "VK_LAYER_KHRONOS_validation" };
	const char*	deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	VkPhysicalDeviceSynchronization2Features	sync2Features = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
		.pNext = VK_NULL_HANDLE,
		.synchronization2 = VK_TRUE
	};
	VkPhysicalDeviceDynamicRenderingFeatures	dynamicRendering = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
		.pNext = &sync2Features,
		.dynamicRendering = VK_TRUE
	};
	VkDeviceCreateInfo	deviceCI{
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = &dynamicRendering,
		.flags = {},
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &queueCI,
		.enabledLayerCount = 0,
		.ppEnabledLayerNames = {},
		.enabledExtensionCount = 1,
		.ppEnabledExtensionNames = deviceExtensions,
		.pEnabledFeatures = nullptr
	};
	if (_hasValidationLayer)
	{
		deviceCI.enabledLayerCount = 1;
		deviceCI.ppEnabledLayerNames = layers;
	}
	chk(vkCreateDevice(_physicalDevice, &deviceCI, nullptr, &_device));
	volkLoadDevice(_device);
	vkGetDeviceQueue(_device, graphicsQueueFamily, 0, &_queue);

	VkCommandPoolCreateInfo	commandPoolCI = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext = VK_NULL_HANDLE,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = graphicsQueueFamily
	};
	chk(vkCreateCommandPool(_device, &commandPoolCI, nullptr, &_commandPool));
	VkCommandBufferAllocateInfo	commandBufferAI = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext = VK_NULL_HANDLE,
		.commandPool = _commandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = _maxFramesInFlight,
	};
	chk(vkAllocateCommandBuffers(_device, &commandBufferAI, _commandBuffers.data()));
}

void	Graphics::_createAllocator(void)
{
	VmaVulkanFunctions	vkFunctions = {
		.vkGetInstanceProcAddr = vkGetInstanceProcAddr,
		.vkGetDeviceProcAddr = vkGetDeviceProcAddr,
		.vkGetPhysicalDeviceProperties = {},
		.vkGetPhysicalDeviceMemoryProperties = {},
		.vkAllocateMemory = {},
		.vkFreeMemory = {},
		.vkMapMemory = {},
		.vkUnmapMemory = {},
		.vkFlushMappedMemoryRanges = {},
		.vkInvalidateMappedMemoryRanges = {},
		.vkBindBufferMemory = {},
		.vkBindImageMemory = {},
		.vkGetBufferMemoryRequirements = {},
		.vkGetImageMemoryRequirements = {},
		.vkCreateBuffer = {},
		.vkDestroyBuffer = {},
		.vkCreateImage = vkCreateImage,
		.vkDestroyImage = {},
		.vkCmdCopyBuffer = {},
		.vkGetBufferMemoryRequirements2KHR = {},
		.vkGetImageMemoryRequirements2KHR = {},
		.vkBindBufferMemory2KHR = {},
		.vkBindImageMemory2KHR = {},
		.vkGetPhysicalDeviceMemoryProperties2KHR = {},
		.vkGetDeviceBufferMemoryRequirements = {},
		.vkGetDeviceImageMemoryRequirements = {},
		.vkGetMemoryWin32HandleKHR = {},
	};

	VmaAllocatorCreateInfo allocatorCI = {
		.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
		.physicalDevice = _physicalDevice,
		.device = _device,
		.preferredLargeHeapBlockSize = {},
		.pAllocationCallbacks = {},
		.pDeviceMemoryCallbacks = {},
		.pHeapSizeLimit = VK_NULL_HANDLE,
		.pVulkanFunctions = &vkFunctions,
		.instance = _instance,
		.vulkanApiVersion = VK_API_VERSION_1_3,
		.pTypeExternalMemoryHandleTypes = VK_NULL_HANDLE
	};

	chk(vmaCreateAllocator(&allocatorCI, &_allocator));
}

void	Graphics::_createWindowAndSurface(void)
{
	_window = SDL_CreateWindow("Fract3D", 800, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
												| SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS);
	if (!_window)
		throw std::runtime_error(SDL_GetError());
	SDL_SyncWindow(_window);

	if (!SDL_Vulkan_CreateSurface(_window, _instance, nullptr, &_surface))
		throw std::runtime_error(SDL_GetError());

	if (!SDL_GetWindowSizeInPixels(_window, &_windowSize.x, &_windowSize.y))
		throw std::runtime_error(SDL_GetError());

	SDL_SetWindowHitTest(_window, _hitTestCallback, this);
}

SDL_HitTestResult	Graphics::_hitTestCallback(SDL_Window* win, const SDL_Point* area, void* data)
{
	(void)win;
	Graphics*	self = static_cast<Graphics *>(data);

	int	btnCenterX = self->_windowSize.x - static_cast<int>(self->_decorationHeight / 2.0f);
	int	btnCenterY = static_cast<int>(self->_decorationHeight / 2.0f);
	int	radius = static_cast<int>(self->_decorationHeight / 2.0f);
	int	dx = area->x - btnCenterX;
	int	dy = area->y - btnCenterY;

	if (dx*dx + dy*dy <= radius*radius)
		return SDL_HITTEST_NORMAL;
	else if (area->y <= self->_decorationHeight)
		return SDL_HITTEST_DRAGGABLE;

	return SDL_HITTEST_NORMAL;
}

void	Graphics::_createSwapchain(void)
{
	VkSurfaceCapabilitiesKHR	surfaceCaps{};
	chk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_physicalDevice, _surface, &surfaceCaps));
	VkExtent2D	swapchainExtent{ surfaceCaps.currentExtent };
	if (surfaceCaps.currentExtent.width == 0xFFFFFFFF)
		swapchainExtent = {
			.width	= static_cast<uint32_t>(_windowSize.x),
			.height	= static_cast<uint32_t>(_windowSize.y)
		};

	uint32_t			surfaceCount = 1;
	VkSurfaceFormatKHR	surfaceFormat;
	vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, _surface, &surfaceCount, &surfaceFormat);
	_imageFormat = surfaceFormat.format;

	VkSwapchainCreateInfoKHR	swapchainCI{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.pNext = nullptr,
		.flags = {},
		.surface = _surface,
		.minImageCount = surfaceCaps.minImageCount,
		.imageFormat = _imageFormat,
		.imageColorSpace = surfaceFormat.colorSpace,
		.imageExtent{ .width = swapchainExtent.width, .height = swapchainExtent.height },
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode = {},
		.queueFamilyIndexCount = 1,
		.pQueueFamilyIndices = {},
		.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = VK_PRESENT_MODE_FIFO_KHR,
		.clipped = VK_FALSE,
		.oldSwapchain = {}
	};
	chk(vkCreateSwapchainKHR(_device, &swapchainCI, nullptr, &_swapchain));
}

void	Graphics::_createSwapchainImages(void)
{
	uint32_t	imagesCount = 0;
	chk(vkGetSwapchainImagesKHR(_device, _swapchain, &imagesCount, nullptr));
	_swapchainImages.resize(imagesCount);
	chk(vkGetSwapchainImagesKHR(_device, _swapchain, &imagesCount, _swapchainImages.data()));

	_swapchainImageViews.resize(imagesCount);
	for (uint32_t i = 0; i < imagesCount; i++)
	{
		const VkImageViewCreateInfo imageViewCI = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext = VK_NULL_HANDLE,
			.flags = {},
			.image = _swapchainImages[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = _imageFormat,
			.components = {
				.r = VK_COMPONENT_SWIZZLE_IDENTITY,
				.g = VK_COMPONENT_SWIZZLE_IDENTITY,
				.b = VK_COMPONENT_SWIZZLE_IDENTITY,
				.a = VK_COMPONENT_SWIZZLE_IDENTITY
			},
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		};

		chk(vkCreateImageView(_device, &imageViewCI, nullptr, &_swapchainImageViews[i]));
	}
}

void	Graphics::_createSyncObjects(void)
{
	for (uint32_t i = 0; i < _maxFramesInFlight; i++)
	{
		const VkSemaphoreCreateInfo	semCI = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = VK_NULL_HANDLE,
			.flags = 0
		};
		chk(vkCreateSemaphore(_device, &semCI, nullptr, &_semImageAvailable[i]));
		chk(vkCreateSemaphore(_device, &semCI, nullptr, &_semRenderFinished[i]));

		const VkFenceCreateInfo	fenceCI = {
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.pNext = VK_NULL_HANDLE,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT
		};
		chk(vkCreateFence(_device, &fenceCI, nullptr, &_fenInFlight[i]));
	}
}

static bool	isValidSPIRV(const std::filesystem::path& path)
{
	std::error_code		errcode;
	const unsigned long	size = std::filesystem::file_size(path, errcode);

	if (errcode || size < 20 || size % sizeof(uint32_t) != 0)
		return false;

	std::ifstream	file(path, std::ios::binary);
	uint32_t		magic = 0;

	file.read(reinterpret_cast<char*>(&magic), sizeof(magic));

	return file.good() && magic == MAGIC_SPIRV;
}

static bool	shaderNeedRecompilation(const std::filesystem::path& source, const std::filesystem::path& binary)
{
	std::error_code	errcode;

	if (!std::filesystem::exists(binary, errcode) || errcode)
		return true;

	if (!isValidSPIRV(binary))
		return true;

	const std::filesystem::file_time_type	sourceTime = std::filesystem::last_write_time(source, errcode);
	if (errcode)
		throw std::runtime_error("Impossible to read: " + source.string());
	const std::filesystem::file_time_type	binaryTime = std::filesystem::last_write_time(binary, errcode);
	if (errcode)
		return true;

	return sourceTime > binaryTime;
}

static std::string	quoteShell(const std::string& arg)
{
	std::string result = "'";

	for (char c : arg)
	{
		if (c == '\'')
			result += "'\\''";
		else
			result += c;
	}

	result += "'";
	return result;
}

static void	compileShader(const std::filesystem::path& source, const std::filesystem::path& binary)
{
	const std::filesystem::path	tmp = binary.string() + ".tmp";
	std::error_code				errcode;
	std::filesystem::remove(tmp, errcode);

	const std::string	command = quoteShell(GLSLC_PATH) + " " + quoteShell(source.string()) + " -o " + quoteShell(tmp.string());
	std::cout << "[Shader] Compilation : " << source << std::endl;

	if (std::system(command.c_str()) != 0)
	{
		std::filesystem::remove(tmp, errcode);
		throw std::runtime_error("Failed during the compilation");
	}

	if (!isValidSPIRV(tmp))
	{
		std::filesystem::remove(tmp, errcode);
		throw std::runtime_error("Invalid SPIR-V generated");
	}

	std::filesystem::rename(tmp, binary);
}

static std::vector<uint32_t>	readSPIRV(const std::filesystem::path& path)
{
	std::ifstream	file(path, std::ios::binary | std::ios::ate);

	if (!file)
		throw std::runtime_error("Impossible to open the shader: " + path.string());

	const std::streampos	end = file.tellg();
	if (end <= 0 || static_cast<uint64_t>(end) % sizeof(uint32_t) != 0)
		throw std::runtime_error("SPIR-V size invalid: " + path.string());

	std::vector<uint32_t>	codeSpirV(static_cast<size_t>(end) / sizeof(uint32_t));

	file.seekg(0);
	file.read(reinterpret_cast<char*>(codeSpirV.data()), static_cast<std::streamsize>(end));

	if (!file || codeSpirV.front() != MAGIC_SPIRV)
		throw std::runtime_error("Invalid SPIR-V read : " + path.string());

	return codeSpirV;
}

void	Graphics::_prepareShaders(void)
{
	std::filesystem::create_directories(SHADER_OUTPUT_DIR);

	for (const auto& shader : SHADERS)
	{
		const std::filesystem::path	source = shader.source;
		const std::filesystem::path	binary = shader.binary;

		if (!std::filesystem::exists(source))
			throw std::runtime_error("Impossible to found " + source.string());

		if (shaderNeedRecompilation(source, binary))
			compileShader(source, binary);
	}
}

void	Graphics::_createPipelines(void)
{
	VkPushConstantRange	pushConstantRange = {
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.offset = 0,
		.size = sizeof(PushConstantsFrag)
	};
	const VkPipelineLayoutCreateInfo	pipelineLayoutCI = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext = VK_NULL_HANDLE,
		.flags = {},
		.setLayoutCount = 0,
		.pSetLayouts = VK_NULL_HANDLE,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &pushConstantRange
	};
	chk(vkCreatePipelineLayout(_device, &pipelineLayoutCI, nullptr, &_pipelineLayout));
	
	_createPipelineMandelBox();
}

void	Graphics::_createBasePipeline(E_Pipeline fract, const VkPipelineShaderStageCreateInfo* shaderStages)
{
	const VkPipelineInputAssemblyStateCreateInfo	pipelineInputAssemblyStateCI = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.pNext = VK_NULL_HANDLE,
		.flags = 0,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = {}
	};
	const VkPipelineViewportStateCreateInfo			pipelineViewportStateCI = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.pNext = VK_NULL_HANDLE,
		.flags = 0,
		.viewportCount = 1,
		.pViewports = VK_NULL_HANDLE,
		.scissorCount = 1,
		.pScissors = VK_NULL_HANDLE
	};
	const VkPipelineRasterizationStateCreateInfo	pipelineRasterizationStateCI = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.pNext = VK_NULL_HANDLE,
		.flags = 0,
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = 0,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.depthBiasEnable = 0,
		.depthBiasConstantFactor = 0,
		.depthBiasClamp = 0,
		.depthBiasSlopeFactor = 0,
		.lineWidth = 1.0f
	};
	const VkPipelineMultisampleStateCreateInfo		pipelineMultisampleStateCI = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.pNext = VK_NULL_HANDLE,
		.flags = 0,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable = VK_FALSE,
		.minSampleShading = 0,
		.pSampleMask = VK_NULL_HANDLE,
		.alphaToCoverageEnable = 0,
		.alphaToOneEnable = 0
	};
	const VkPipelineColorBlendAttachmentState		pipelineColorBlendAttachmentState = {
		.blendEnable = VK_FALSE,
		.srcColorBlendFactor = {},
		.dstColorBlendFactor = {},
		.colorBlendOp = {},
		.srcAlphaBlendFactor = {},
		.dstAlphaBlendFactor = {},
		.alphaBlendOp = {},
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};
	const VkPipelineColorBlendStateCreateInfo		pipelineColorBlendStateCI = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.pNext = VK_NULL_HANDLE,
		.flags = 0,
		.logicOpEnable = VK_FALSE,
		.logicOp = {},
		.attachmentCount = 1,
		.pAttachments = &pipelineColorBlendAttachmentState,
		.blendConstants = 0
	};
	const VkDynamicState							dynamicStates[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	const VkPipelineDynamicStateCreateInfo			pipelineDynamicStateCI = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.pNext = VK_NULL_HANDLE,
		.flags = 0,
		.dynamicStateCount = 2,
		.pDynamicStates = dynamicStates
	};
	const VkPipelineRenderingCreateInfo				pipelineRenderingCI = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.pNext = VK_NULL_HANDLE,
		.viewMask = 0,
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &_imageFormat,
		.depthAttachmentFormat = VK_FORMAT_UNDEFINED,
		.stencilAttachmentFormat = VK_FORMAT_UNDEFINED
	};
	const VkPipelineVertexInputStateCreateInfo		pipelineVertexInputStateCI = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.pNext = VK_NULL_HANDLE,
		.flags = 0,
		.vertexBindingDescriptionCount = 0,
		.pVertexBindingDescriptions = VK_NULL_HANDLE,
		.vertexAttributeDescriptionCount = 0,
		.pVertexAttributeDescriptions = VK_NULL_HANDLE
	};

	const VkGraphicsPipelineCreateInfo	graphicsPipelineCI = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = &pipelineRenderingCI,
		.flags = {},
		.stageCount = 2,
		.pStages = shaderStages,
		.pVertexInputState = &pipelineVertexInputStateCI,
		.pInputAssemblyState = &pipelineInputAssemblyStateCI,
		.pTessellationState = VK_NULL_HANDLE,
		.pViewportState = &pipelineViewportStateCI,
		.pRasterizationState = &pipelineRasterizationStateCI,
		.pMultisampleState = &pipelineMultisampleStateCI,
		.pDepthStencilState = VK_NULL_HANDLE,
		.pColorBlendState = &pipelineColorBlendStateCI,
		.pDynamicState = &pipelineDynamicStateCI,
		.layout = _pipelineLayout,
		.renderPass = {},
		.subpass = 0,
		.basePipelineHandle = VK_NULL_HANDLE,
		.basePipelineIndex = 0
	};

	chk(vkCreateGraphicsPipelines(_device, nullptr, 1, &graphicsPipelineCI, nullptr, &_pipelines[fract]));
}

void	Graphics::_createPipelineMandelBox(void)
{
	const std::vector<uint32_t>	vertexCode = readSPIRV("build/shaders-spv/base.vert.spv");

	VkShaderModuleCreateInfo	shaderCI = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pNext = VK_NULL_HANDLE,
		.flags = {},
		.codeSize = vertexCode.size() * sizeof(uint32_t),
		.pCode = vertexCode.data()
	};
	VkShaderModule	shaderVertex;
	chk(vkCreateShaderModule(_device, &shaderCI, nullptr, &shaderVertex));

	const std::vector<uint32_t>	fragCode = readSPIRV("build/shaders-spv/mandelbox.frag.spv");

	shaderCI.codeSize = fragCode.size() * sizeof(uint32_t);
	shaderCI.pCode = fragCode.data();
	VkShaderModule	shaderFragment;
	chk(vkCreateShaderModule(_device, &shaderCI, nullptr, &shaderFragment));

	const VkPipelineShaderStageCreateInfo	vertStageInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.pNext = VK_NULL_HANDLE,
		.flags = 0,
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
		.module = shaderVertex,
		.pName = "main",
		.pSpecializationInfo = VK_NULL_HANDLE
	};
	const VkPipelineShaderStageCreateInfo	fragStageInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.pNext = VK_NULL_HANDLE,
		.flags = 0,
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.module = shaderFragment,
		.pName = "main",
		.pSpecializationInfo = VK_NULL_HANDLE
	};
	const VkPipelineShaderStageCreateInfo	shaderStages[] = { vertStageInfo, fragStageInfo };
	_createBasePipeline(E_PIPELINE_MANDELBOX, shaderStages);
	vkDestroyShaderModule(_device, shaderVertex, nullptr);
	vkDestroyShaderModule(_device, shaderFragment, nullptr);
}

void	Graphics::render(graphics_options& opt, const float& deltaTime)
{
	vkWaitForFences(_device, 1, &_fenInFlight[_currentFrame], VK_TRUE, UINT64_MAX);
	vkResetFences(_device, 1, &_fenInFlight[_currentFrame]);

	uint32_t	imageIndex;
	chk(vkAcquireNextImageKHR(_device, _swapchain, UINT64_MAX, _semImageAvailable[_currentFrame], VK_NULL_HANDLE, &imageIndex));

	VkCommandBuffer	cmdBuff = _commandBuffers[_currentFrame];
	chk(vkResetCommandBuffer(cmdBuff, 0));
	VkCommandBufferBeginInfo	cmdBeginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = VK_NULL_HANDLE,
		.flags = 0,
		.pInheritanceInfo = VK_NULL_HANDLE
	};
	chk(vkBeginCommandBuffer(cmdBuff, &cmdBeginInfo));

	VkImageMemoryBarrier2	imageMemoryBarrier[] = {
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.pNext = VK_NULL_HANDLE,
			.srcStageMask = VK_PIPELINE_STAGE_2_NONE,
			.srcAccessMask = VK_ACCESS_2_NONE,
			.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.srcQueueFamilyIndex = 0,
			.dstQueueFamilyIndex = 0,
			.image = _swapchainImages[imageIndex],
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = VK_REMAINING_MIP_LEVELS,
				.baseArrayLayer = 0,
				.layerCount = 1,
			}
		}
	};
	const VkDependencyInfo	dependecyInfo = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.pNext = VK_NULL_HANDLE,
		.dependencyFlags = 0,
		.memoryBarrierCount = 0,
		.pMemoryBarriers = VK_NULL_HANDLE,
		.bufferMemoryBarrierCount = 0,
		.pBufferMemoryBarriers = VK_NULL_HANDLE,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = imageMemoryBarrier,
	};
	vkCmdPipelineBarrier2(cmdBuff, &dependecyInfo);

	const VkRenderingAttachmentInfo	renderingAttachmentInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
		.pNext = VK_NULL_HANDLE,
		.imageView = _swapchainImageViews[imageIndex],
		.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
		.resolveMode = {},
		.resolveImageView = {},
		.resolveImageLayout = {},
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = { .color = { 0.0f, 0.0f, 0.0f, 1.0f } },
	};
	const VkRenderingInfo	renderingInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
		.pNext = VK_NULL_HANDLE,
		.flags = {},
		.renderArea = { { 0, 0 }, { static_cast<uint32_t>(_windowSize.x), static_cast<uint32_t>(_windowSize.y) } },
		.layerCount = 1,
		.viewMask = 0,
		.colorAttachmentCount = 1,
		.pColorAttachments = &renderingAttachmentInfo,
		.pDepthAttachment = VK_NULL_HANDLE,
		.pStencilAttachment = VK_NULL_HANDLE,
	};
	vkCmdBeginRendering(cmdBuff, &renderingInfo);


















	(void)opt;
	(void)deltaTime;
	E_Pipeline	fractToRender = _fractalToRender;
	vkCmdBindPipeline(cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelines[fractToRender]);

	const VkViewport	viewport = {
		.x = 0,
		.y = 0,
		.width = static_cast<float>(_windowSize.x),
		.height = static_cast<float>(_windowSize.y),
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};
	vkCmdSetViewport(cmdBuff, 0, 1, &viewport);

	const VkRect2D	scissor = {
		.offset = { 0, 0 },
		.extent = { static_cast<uint32_t>(_windowSize.x), static_cast<uint32_t>(_windowSize.y) }
	};
	vkCmdSetScissor(cmdBuff, 0, 1, &scissor);

	PushConstantsFrag constants = {
		.pos = opt.cam.pos,
		._pad = 0.0,
		.rot = opt.cam.rot,
		.screenSize = _windowSize
	};
	vkCmdPushConstants(cmdBuff, _pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(constants), &constants);
	
	vkCmdDraw(cmdBuff, 3, 1, 0, 0);












	vkCmdEndRendering(cmdBuff);

	imageMemoryBarrier[0].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	imageMemoryBarrier[0].newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	imageMemoryBarrier[0].srcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	imageMemoryBarrier[0].srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	imageMemoryBarrier[0].dstStageMask  = VK_PIPELINE_STAGE_2_NONE;
	imageMemoryBarrier[0].dstAccessMask = VK_ACCESS_2_NONE;
	vkCmdPipelineBarrier2(cmdBuff, &dependecyInfo);

	vkEndCommandBuffer(cmdBuff);

	const VkPipelineStageFlags	waitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	const VkSubmitInfo	submitInfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pNext = VK_NULL_HANDLE,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &_semImageAvailable[_currentFrame],
		.pWaitDstStageMask = &waitStages,
		.commandBufferCount = 1,
		.pCommandBuffers = &cmdBuff,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &_semRenderFinished[_currentFrame],
	};
	vkQueueSubmit(_queue, 1, &submitInfo, _fenInFlight[_currentFrame]);

	const VkPresentInfoKHR	presentInfo = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext = VK_NULL_HANDLE,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &_semRenderFinished[_currentFrame],
		.swapchainCount = 1,
		.pSwapchains = &_swapchain,
		.pImageIndices = &imageIndex,
		.pResults = VK_NULL_HANDLE,
	};
	vkQueuePresentKHR(_queue, &presentInfo);

	_currentFrame = (_currentFrame + 1) % _maxFramesInFlight;
}

void	Graphics::_getMouseInput(graphics_options& opt, const float& deltaTime)
{
	(void)deltaTime;

	SDL_Event	event;
	while (SDL_PollEvent(&event))
	{
		if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST)
		{
			SDL_SetWindowRelativeMouseMode(_window, false);
			opt.shouldRerender = true;
		}
		else if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED)
		{
			SDL_SetWindowRelativeMouseMode(_window, true);
			opt.shouldRerender = true;
		}
		else if (event.type == SDL_EVENT_QUIT)
		{
			opt.shouldRun = false;
			break;
		}
		else if (event.type == SDL_EVENT_MOUSE_WHEEL)
		{
			opt.forwardSpeed -= (event.wheel.y / 100.0f);
			if (opt.forwardSpeed < 0.0f)
				opt.forwardSpeed = 0.0001f;
		}
		else if (event.type == SDL_EVENT_MOUSE_MOTION)
		{
			float	yaw = -event.motion.xrel * opt.sensitivity;
			float	pitch = -event.motion.yrel * opt.sensitivity;

			glm::quat	quatYaw = glm::angleAxis(yaw, glm::vec3(0.0f, -1.0f, 0.0f));
			glm::quat	quatPitch = glm::angleAxis(pitch, glm::vec3(1.0, 0.0, 0.0));

			opt.cam.rot = glm::normalize(quatYaw * opt.cam.rot * quatPitch);
			opt.shouldRerender = true;
		}
	}
}

void	Graphics::_getKeyboardInput(graphics_options& opt, const float& deltaTime)
{
	glm::vec3	direction(0.0f, 0.0f, 0.0f);

	SDL_PumpEvents();
	if (_keyboardState[SDL_SCANCODE_ESCAPE])
		SDL_SetWindowRelativeMouseMode(_window, false);
	if (_keyboardState[SDL_SCANCODE_Q])
		opt.shouldRun = false;

	if (_keyboardState[SDL_SCANCODE_W])
		direction += glm::vec3(0.0f, 0.0f, 1.0f);
	if (_keyboardState[SDL_SCANCODE_S])
		direction += glm::vec3(0.0f, 0.0f, -1.0f);
	if (_keyboardState[SDL_SCANCODE_A])
		direction += glm::vec3(-1.0f, 0.0f, 0.0f);
	if (_keyboardState[SDL_SCANCODE_D])
		direction += glm::vec3(1.0f, 0.0f, 0.0f);
	if (_keyboardState[SDL_SCANCODE_LCTRL])
		direction += glm::vec3(0.0f, 1.0f, 0.0f);
	if (_keyboardState[SDL_SCANCODE_SPACE])
		direction += glm::vec3(0.0f, -1.0f, 0.0f);

	if (direction != glm::vec3(0.0f))
	{
		direction = glm::normalize(direction);
		opt.shouldRerender = true;
	}
	opt.cam.pos += (opt.cam.rot * direction) * opt.forwardSpeed * deltaTime;
}

void	Graphics::handleInputs(graphics_options& opt, const float& deltaTime)
{
	_getMouseInput(opt, deltaTime);
	_getKeyboardInput(opt, deltaTime);
}
