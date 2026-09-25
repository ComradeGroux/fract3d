#include "Graphics.hpp"
#include "graphics_options.h"

int	main(int argc, char** argv)
{
	(void)argc;
	(void)argv;
	Graphics	vk;
	graphics_options	opt;

	while (opt.shouldRun)
	{
		vk.handleInputs(opt, 0.0);
		vk.render(opt, 0.0);
	}

	return 0;
}
