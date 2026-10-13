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

	ray		test = { pc.pos, normalize(rotate(normalizedCoord, pc.rot)) };
	return test;
}

vec3	calcNormal(vec3 p)
{
    const vec2 k = vec2(1.0, -1.0);
    const float h = 0.0001;
    return normalize(k.xyy * distanceEstimator(p + k.xyy * h)
                   + k.yyx * distanceEstimator(p + k.yyx * h)
                   + k.yxy * distanceEstimator(p + k.yxy * h)
                   + k.xxx * distanceEstimator(p + k.xxx * h));
}

vec3	basicLambert(vec3 normal)
{
	vec3	lightDir = normalize(vec3(0.5, -0.8, 0.3));
	return vec3(max(dot(normal, lightDir), 0.0));
}

float	distanceEstimator(vec3 p)
{
	return (length(p) - 1.0);
}

void	main()
{
	ray		r = calculateRayDirection();
	float	t = 0.0;
	float	d = 0.0;
	vec3	p;
	bool	hit = false;
	for (int i = 0; i < 64; i++)
	{
		p = r.ro + r.rd * t;
		d = distanceEstimator(p);
		if (d < 0.001)
		{
			hit = true;
			break;
		}

		t += d;
		if (t > 100.0)
			break;
	}

	if (hit)
	{
		fragColor = vec4(basicLambert(calcNormal(p)) * 0.9 + 0.1, 1.0);
	}
	else
		fragColor = vec4(0.0, 0.0, 0.0, 1.0);
}
