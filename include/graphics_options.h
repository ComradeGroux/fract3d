#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <chrono>

struct	PushConstantsFrag
{
	glm::vec3	pos = { 0.0, 0.0, -15.0 };
	float		_pad = 0.0;
	glm::quat	rot = { 1.0, 0.0, 0.0, 0.0 };
	glm::vec2	screenSize = { 0, 0 };
	int			theme = 0;
};

struct graphics_options
{
	bool						shouldRun = true;
	bool						shouldRerender = true;
	float						forwardSpeed = 0.5f;
	float						sensitivity = 0.001f;
	PushConstantsFrag			cam;
	int							theme = 0;
};
