#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <atomic>

#include "graphics_options.h"

class Graphics
{
	private:
		VkApplicationInfo	_appInfo;
		VkInstance			_instance{ VK_NULL_HANDLE };
		VkPhysicalDevice	_physicalDevice{ VK_NULL_HANDLE };
		VkDevice			_device{ VK_NULL_HANDLE };
		VkQueue				_queue{ VK_NULL_HANDLE };
		VkSurfaceKHR		_surface{ VK_NULL_HANDLE };
		VkSwapchainKHR		_swapchain{ VK_NULL_HANDLE };
		VkCommandPool		_commandPool{ VK_NULL_HANDLE };

		enum E_Pipeline {
			E_PIPELINE_MANDELBOX,
			E_PIPELINE_COUNT
		};
		std::array<VkPipeline, E_PIPELINE_COUNT>	_pipelines;
		VkPipelineLayout							_pipelineLayout{ VK_NULL_HANDLE };
		std::atomic<E_Pipeline>						_fractalToRender = E_PIPELINE_MANDELBOX;

		const static uint32_t							_maxFramesInFlight = 2;
		std::array<VkCommandBuffer, _maxFramesInFlight>	_commandBuffers;

		VkFormat					_imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
		std::vector<VkImage>		_swapchainImages;
		std::vector<VkImageView>	_swapchainImageViews;

		std::array<VkSemaphore, _maxFramesInFlight>	_semImageAvailable;
		std::array<VkSemaphore, _maxFramesInFlight>	_semRenderFinished;
		std::array<VkFence, _maxFramesInFlight>		_fenInFlight;
		uint32_t									_currentFrame = 0;

		VmaAllocator		_allocator{ VK_NULL_HANDLE };

		SDL_Window*		_window = nullptr;
		glm::ivec2		_windowSize;
		bool			_hasValidationLayer = false;
		float			_decorationHeight = 30.0f;
		const bool*		_keyboardState;

		void	_createVkApplicationInfo(void);
		void	_createVkInstance(void);
		void	_selectVkPhysicalDevice(void);
		void	_createVkDeviceAndQueue(void);
		void	_createAllocator(void);
		void	_createWindowAndSurface(void);
		void	_createSwapchain(void);
		void	_createSwapchainImages(void);
		void	_createSyncObjects(void);

		static SDL_HitTestResult	_hitTestCallback(SDL_Window* win, const SDL_Point* area, void* data);

		void	_createPipelines(void);
		void	_createBasePipeline(E_Pipeline fract, const VkPipelineShaderStageCreateInfo* shaderStages);
		void	_createPipelineMandelBox(void);

		void	_renderUI(VkCommandBuffer& cmdBuff);

		void	_getMouseInput(graphics_options& opt, const double& deltaTime);
		void	_getKeyboardInput(graphics_options& opt, const double& deltaTime);

	public:
		Graphics(void);
		~Graphics(void);

		void	render(graphics_options& opt, const double& deltaTime);
		void	handleInputs(graphics_options& opt, const double& deltaTime);
};
