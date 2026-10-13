#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

struct	PushConstantsFrag
{
	glm::vec3	pos = { 0.0, 0.0, 0.0 };
	float		_pad = 0.0;
	glm::quat	rot = { 1.0, 0.0, 0.0, 0.0 };
	glm::vec2	screenSize = { 0, 0 };
};

struct graphics_options
{
	bool		shouldRun = true;
	float		forwardSpeed = 0.0f;
	float		sensitivity = 0.001f;
	PushConstantsFrag		cam;
};
