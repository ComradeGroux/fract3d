#version 460

layout(push_constant) uniform uPushConstant
{
	vec3	pos;
	vec4	rot;
} pc;

layout(location = 0) out vec4 fragColor;

void main()
{
	fragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
