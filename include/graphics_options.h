#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

struct	camera
{
	glm::vec3	pos = { 0.0, 0.0, 0.0 };
	glm::quat	rot = { 1.0, 0.0, 0.0, 0.0 };
};

struct graphics_options
{
	bool		shouldRun = true;
	double		forwardSpeed = 0.0f;
	camera		cam;
};
