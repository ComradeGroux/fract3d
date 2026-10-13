#version 460

layout(push_constant) uniform uPushConstant
{
	vec3	pos;
	vec4	rot;
	vec2	screenSize;
} pc;

layout(location = 0) out vec4 fragColor;

struct ray {
	vec3	ro;
	vec3	rd;
};

float	distanceEstimator(vec3 p)
{
	return (length(p) - 1.0);
}

vec3	rotate(vec3 v, vec4 q)
{
    vec3 t = 2.0 * cross(q.xyz, v);
    return v + q.w * t + cross(q.xyz, t);
}

vec3	center(vec3 p)
{
	return vec3(p.x - (pc.screenSize.x / 2.0), p.y - (pc.screenSize.y / 2.0), p.z);
}

ray calculateRayDirection()
{
	vec3	normalizedCoord = center(vec3(gl_FragCoord.xy, 1.0));
	normalizedCoord = vec3(normalizedCoord.x / pc.screenSize.y, normalizedCoord.y / pc.screenSize.y, normalizedCoord.z);

	ray		test = { pc.pos, rotate(normalizedCoord, pc.rot) };
	return test;
}

void main()
{
	ray	r = calculateRayDirection();
	fragColor = vec4(r.rd, 1.0);
}
