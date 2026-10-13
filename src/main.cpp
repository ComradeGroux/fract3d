#include "Graphics.hpp"
#include "graphics_options.h"

#include <chrono>

int	main(int argc, char** argv)
{
	(void)argc;
	(void)argv;
	Graphics								vk;
	graphics_options						opt;
	std::chrono::steady_clock::time_point	currentTime = std::chrono::steady_clock::now();
	std::chrono::steady_clock::time_point	previousTime = currentTime;
	float									elapsedTime = 0.0f;

	while (opt.shouldRun)
	{
		currentTime = std::chrono::steady_clock::now();
		elapsedTime = std::chrono::duration_cast<std::chrono::duration<float>>(currentTime - previousTime).count();
		elapsedTime = std::min(elapsedTime, 0.1f);

		if (opt.shouldRerender)
		{
			vk.render(opt, elapsedTime);
			opt.shouldRerender = false;
		}

		vk.handleInputs(opt, elapsedTime);

		previousTime = currentTime;
	}

	return 0;
}
